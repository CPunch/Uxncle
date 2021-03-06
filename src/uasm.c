#include "uasm.h"
#include "uparse.h"

/* compiler state */
typedef struct {
    FILE *out;
    UScope *scopes[MAX_SCOPES];
    int sCount;
    int pushed; /* current bytes on the stack */
    int jmpID;
} UCompState;

static const char preamble[] =
    "|10 @Console [ &pad $8 &char $1 &byte $1 &short $2 &string $2 ]\n"
    "|0000\n"
    "@number [ &started $1 ]\n"
    "@uxncle [ &heap $2 ]\n"
    "|0100\n"
    "@main-prg\n"
        /* setup mem lib */
        ";uxncle-heap .uxncle/heap STZ2\n";

static const char postamble[] =
    "\n"
    "BRK\n"
        "@print-decimal\n"
        "\t#00 .number/started STZ\n"
        "\tDUP2 #2710 DIV2 DUP2 ,&digit JSR #2710 MUL2 SUB2\n"
        "\tDUP2 #03e8 DIV2 DUP2 ,&digit JSR #03e8 MUL2 SUB2\n"
        "\tDUP2 #0064 DIV2 DUP2 ,&digit JSR #0064 MUL2 SUB2\n"
        "\tDUP2 #000a DIV2 DUP2 ,&digit JSR #000a MUL2 SUB2\n"
        "\t,&digit JSR\n"
        "\t.number/started LDZ ,&end JCN\n"
        "\tLIT '0 .Console/char DEO\n"
        "\t&end\n"
    "JMP2r\n"
    "\t&digit\n"
        "\tSWP POP\n"
        "\tDUP .number/started LDZ ORA #02 JCN\n"
        "\tPOP JMP2r\n"
        "\tLIT '0 ADD .Console/char DEO\n"
        "\t#01 .number/started STZ\n"
    "JMP2r\n"
    /* start of thin memory library */
    "@alloc-uxncle\n" /* this subroutine handles allocating memory on the heap, expects the size (short) */
        ".uxncle/heap LDZ2\n" /* load the heap pointer onto the stack */
        "ADD2\n" /* add the size */
        ".uxncle/heap STZ2\n" /* store the new heap pointer */
        "JMP2r\n" /* return */
    "@dealloc-uxncle\n" /* this subroutine handles deallocating memory from the heap, expects the size (short) */
        ".uxncle/heap LDZ2\n" /* load the heap pointer onto the stack */
        "SWP2\n" /* move the heap pointer behind the size, so when we subtract it'll be heap - size, not size - heap */
        "SUB2\n" /* sub the size from the address */
        ".uxncle/heap STZ2\n" /* store the new heap pointer */
        "JMP2r\n" /* return */
    "@peek-uxncle-short\n" /* this subroutine handles loading a short from the heap and pushing it onto the stack, expects  the offset (short) */
        ".uxncle/heap LDZ2\n" /* load the heap pointer onto the stack */
        "SWP2\n" /* move the heap pointer behind the offset */
        "SUB2\n"
        "LDA2\n" /* loads the short from the heap onto the stack */
        "JMP2r\n" /* return */
    "@poke-uxncle-short\n" /* this subroutine handles popping a short from the stack and saving it into the heap, expects the value (short) and the offset (short) */
        ".uxncle/heap LDZ2\n" /* load the heap pointer onto the stack */
        "SWP2\n" /* move the heap pointer behind the offset */
        "SUB2\n"
        "STA2\n" /* stores the value into the address */
        "JMP2r\n" /* return */
    "@peek-uxncle\n" /* this subroutine handles loading a byte from the heap and pushing it onto the stack, expects the offset (short) */
        ".uxncle/heap LDZ2\n" /* load the heap pointer onto the stack */
        "SWP2\n" /* move the heap pointer behind the offset */
        "SUB2\n"
        "LDA\n" /* loads the byte from the heap onto the stack */
        "JMP2r\n" /* return */
    "@poke-uxncle\n" /* this subroutine handles popping a byte from the stack and saving it into the heap, expects the value (byte) and the offset (short) */
        ".uxncle/heap LDZ2\n" /* load the heap pointer onto the stack */
        "SWP2\n" /* move the heap pointer behind the offset */
        "SUB2\n"
        "STA\n" /* stores the value into the address */
        "JMP2r\n" /* return */
    "@uxncle-heap\n"
    "|ffff &end";

void compileAST(UCompState *state, UASTNode *node);
UVarType compileExpression(UCompState *state, UASTNode *node);

/* ==================================[[ generic helper functions ]]================================== */

/* throws a compiler error */
void cError(UCompState *state, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("Compiler error!\n\t");
    vprintf(fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

void cErrorNode(UCompState *state, UASTNode *node, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("Compiler error at '%.*s' on line %d\n\t", node->tkn.len, node->tkn.str, node->tkn.line);
    vprintf(fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

void writeIntLit(UCompState *state, uint16_t lit) {
    fprintf(state->out, "#%.4x ", lit);
    state->pushed += SIZE_INT;
}

void writeByteLit(UCompState *state, uint8_t lit) {
    fprintf(state->out, "#%.2x ", lit);
    state->pushed += SIZE_CHAR;
}

uint16_t getSize(UCompState *state, UVar *var) {
    switch(var->type) {
        case TYPE_CHAR: case TYPE_BOOL: return 1;
        case TYPE_INT: return 2;
        default:
            cError(state, "unknown type! [%d]", var->type);
            return 0;
    }
}

uint16_t getScopeSize(UCompState *state, UScope *scope) {
    uint16_t size = 0;
    int i;

    /* add up all the sizes */
    for (i = 0; i < scope->vCount; i++)
        size += getSize(state, &scope->vars[i]);
    
    return size;
}

void pushScope(UCompState *state, UScope *scope) {
    state->scopes[state->sCount++] = scope;
    int scopeSize = getScopeSize(state, scope);

    if (scopeSize > 0) {
        writeIntLit(state, getScopeSize(state, scope));
        fwrite(";alloc-uxncle JSR2\n", 19, 1, state->out);
        state->pushed -= SIZE_INT;
    }
}

void popScope(UCompState *state) {
    UScope *scope = state->scopes[--state->sCount];
    int scopeSize = getScopeSize(state, scope);

    if (scopeSize > 0) {
        writeIntLit(state, scopeSize);
        fwrite(";dealloc-uxncle JSR2\n", 21, 1, state->out);
        state->pushed -= SIZE_INT;
    }
}

uint16_t getOffset(UCompState *state, int scope, int var) {
    uint16_t offsetAddr = 0; 
    int i, z;

    /* sanity check */
    if (state->sCount < scope || state->scopes[scope]->vCount < var)
        cError(state, "Invalid variable id!");

    /* walk scopes adding the size of the data on the heap */
    for (i = scope; i >= 0; i--) 
        for (z = var; z >= 0; z--)
            offsetAddr += getSize(state, &state->scopes[i]->vars[z]);

    return offsetAddr;
}

void getIntVar(UCompState *state, int scope, int var) {
    uint16_t offsetAddr = getOffset(state, scope, var);

    writeIntLit(state, offsetAddr); /* write the offset */
    fprintf(state->out, ";peek-uxncle-short JSR2\n"); /* call the mem lib */
}

UVar* getVarByID(UCompState *state, int scope, int var) {
    return &state->scopes[scope]->vars[var];
}

void setIntVar(UCompState *state, int scope, int var) {
    uint16_t offsetAddr = getOffset(state, scope, var);

    writeIntLit(state, offsetAddr); /* write the offset */
    fprintf(state->out, ";poke-uxncle-short JSR2\n"); /* call the mem lib */
    state->pushed -= SIZE_INT + SIZE_INT; /* pops the offset (short) & the value (short) */
}

void setVar(UCompState *state, int scope, int var, UVarType type) {
    switch(type) {
        case TYPE_INT: setIntVar(state, scope, var); break;
        default:
            cError(state, "Unimplemented setter for type '%s'", getTypeName(type));
    }
}

UVarType getVar(UCompState *state, int scope, int var) {
    UVar *rawVar = getVarByID(state, scope, var);

    switch(rawVar->type) {
        case TYPE_INT: getIntVar(state, scope, var); break;
        default:
            cError(state, "Unimplemented getter for type '%s'", getTypeName(rawVar->type));
    }
    
    return rawVar->type;
}

UVarType compileVar(UCompState *state, UASTNode *node) {
    UASTVarNode *var = (UASTVarNode*)node;
    return getVar(state, var->scope, var->var);
}

int compareVarTypes(UCompState *state, UVarType t1, UVarType t2) {
    return t1 == t2;
}

/* assumes `from` is already on the stack */
int tryTypeCast(UCompState *state, UVarType from, UVarType to) {
    if (compareVarTypes(state, from, to))
        return 1;

    switch(to) {
        case TYPE_CHAR:
            switch(from) {
                case TYPE_INT: fprintf(state->out, "SWP POP\n"); state->pushed -= 1; break; /* moves the most significant byte to the front and pops it */
                case TYPE_BOOL: break; /* TYPE_BOOL is already the same size */
                default: return 0;
            }
            break;
        case TYPE_INT:
            switch(from) {
                /* the process to convert TYPE_CHAR & TYPE_BOOL to TYPE_INT is the same */
                case TYPE_BOOL: case TYPE_CHAR: fprintf(state->out, "#00 SWP\n"); state->pushed += 1; break; /* pushes an empty byte to the stack and moves it to the most significant byte */
                default: return 0;
            }
            break;
        case TYPE_BOOL: /* do a comparison if the value is not equal to zero */
            switch(from) {
                case TYPE_INT: fprintf(state->out, "#0000 NEQ2\n"); state->pushed -= 1; break;
                case TYPE_CHAR: fprintf(state->out, "#00 NEQ\n"); break;
                default: return 0;
            }
            break;
        default: return 0;
    }

    return 1;
}

int newLbl(UCompState *state) {
    return state->jmpID++;
}

void defineSubLbl(UCompState *state, int subLblID) {
    fprintf(state->out, "&lbl%d\n", subLblID);
}

/* expects TYPE_BOOL at the top of the stack */
void jmpCondSub(UCompState *state, int subLblID) {
    fprintf(state->out, ",&lbl%d JCN\n", subLblID);
    state->pushed -= SIZE_BOOL;
}

void jmpSub(UCompState *state, int subLblID) {
    fprintf(state->out, ",&lbl%d JMP\n", subLblID);
}

/* ==================================[[ arithmetic ]]================================== */

void pop(UCompState *state, int size) {
    int i;

    /* use POP2 for as much as we can */
    for (i = size; i-2 >= 0; i-=2) {
        fprintf(state->out, "POP2\n");
    }

    /* we might have a left over byte that still needs to be popped */
    if (i == 1)
        fprintf(state->out, "POP\n");
    
    state->pushed-=size;
}

void dupValue(UCompState *state, UVarType type) {
    switch(type) {
        case TYPE_INT: fprintf(state->out, "DUP2\n"); state->pushed+=SIZE_INT; break;
        case TYPE_CHAR: case TYPE_BOOL: fprintf(state->out, "DUP\n"); state->pushed+=SIZE_CHAR; break;
        default:
            cError(state, "Unknown variable type! [%d]", type);
    }
}

void cIntArith(UCompState *state, const char *instr) {
    fprintf(state->out, "%s2\n", instr);
    /* arith operations pop 2 shorts, and push 1 short, so in total we have 1 short less on the stack */
    state->pushed -= SIZE_INT;
}

void doArith(UCompState *state, const char *instr, UVarType type) {
    switch(type) {
        case TYPE_INT: cIntArith(state, instr); break;
        default:
            cError(state, "Unknown variable type! [%d]", type);
    }
}

void doComp(UCompState *state, const char *instr, UVarType type) {
    switch(type) {
        case TYPE_INT:
            fprintf(state->out, "%s2\n", instr);
            state->pushed -= SIZE_INT*2; /* pop the two shorts */
            break;
        case TYPE_CHAR: /* char and bool are the same size */
        case TYPE_BOOL:
            fprintf(state->out, "%s\n", instr);
            state->pushed -= SIZE_CHAR*2; /* pop the two bytes */
            break;
        default:
            cError(state, "Unknown variable type! [%d]", type);
    }
    state->pushed += SIZE_BOOL;
}

UVarType compileAssignment(UCompState *state, UASTNode *node, int expectsVal) {
    UASTVarNode *nVar = (UASTVarNode*)node->left;
    UVar *rawVar = getVarByID(state, nVar->scope, nVar->var);
    UVarType expType;

    /* get the value of the expression */
    expType = compileExpression(state, node->right);

    /* make sure we can assign the value of this expression to this variable */
    if (!tryTypeCast(state, expType, rawVar->type))
        cErrorNode(state, node, "Cannot assign type '%s' to '%.*s' of type '%s'", getTypeName(expType), rawVar->len, rawVar->name, getTypeName(rawVar->type));

    /* duplicate the value on the stack if it's expected */
    if (expectsVal)
        dupValue(state, expType);

    /* assign the copy to the variable, leaving a copy on the stack for the expression */
    setVar(state, nVar->scope, nVar->var, expType);

    return rawVar->type;
}

UVarType compileExpression(UCompState *state, UASTNode *node) {
    UVarType lType = TYPE_NONE, rType = TYPE_NONE;

    /* assignments are special, they're like statements but can be inside of expressions */
    if (node->type == NODE_ASSIGN)
        return compileAssignment(state, node, 1);

    /* first, traverse down the AST recusively */
    if (node->left)
        lType = compileExpression(state, node->left);
    if (node->right)
        rType = compileExpression(state, node->right);

    if (lType != TYPE_NONE && rType != TYPE_NONE && !compareVarTypes(state, lType, rType))
        cErrorNode(state, node, "lType '%s' doesn't match rType '%s'!", getTypeName(lType), getTypeName(rType));

    switch(node->type) {
        case NODE_ADD: doArith(state, "ADD", lType); break;
        case NODE_SUB: doArith(state, "SUB", lType); break;
        case NODE_MUL: doArith(state, "MUL", lType); break;
        case NODE_DIV: doArith(state, "DIV", lType); break;
        case NODE_EQUAL: doComp(state, "EQU", lType); return TYPE_BOOL;
        case NODE_NEQUAL: doComp(state, "NEQ", lType); return TYPE_BOOL;
        case NODE_LESS: doComp(state, "LTH", lType); return TYPE_BOOL;
        case NODE_GREATER: doComp(state, "GTH", lType); return TYPE_BOOL;
        /* TODO: NODE_LESS_EQUAL && NODE_GREATER_EQUAL */
        case NODE_INTLIT: writeIntLit(state, ((UASTIntNode*)node)->num); return TYPE_INT;
        case NODE_VAR: return compileVar(state, node); break;
        default:
            cError(state, "unknown AST node!! [%d]\n", node->type);
    }

    return lType;
}

/* compile an expression without leaving anything on the stack */
void compileVoidExpression(UCompState *state, UASTNode *node) {
    int savedPushed = state->pushed;

    if (node->type == NODE_ASSIGN)
        compileAssignment(state, node, 0);
    else
        compileExpression(state, node);

    /* clean the stack */
    pop(state, state->pushed - savedPushed);
}

void compilePrintInt(UCompState *state, UASTNode *node) {
    compileExpression(state, node->left);
    fwrite(";print-decimal JSR2 #20 .Console/char DEO\n", 42, 1, state->out);
    state->pushed -= SIZE_INT;
}

void compileDeclaration(UCompState *state, UASTNode *node) {
    UVarType type;
    UASTVarNode *var = (UASTVarNode*)node;
    UVar *rawVar = getVarByID(state, var->scope, var->var);

    /* if there's no assignment, the default value will be scary undefined memory :O */
    if (node->left) {
        type = compileExpression(state, node->left);
        if (!compareVarTypes(state, type, rawVar->type))
            cErrorNode(state, node, "Cannot assign type '%s' to %.*s of type '%s'", getTypeName(type), rawVar->len, rawVar->name, getTypeName(rawVar->type));
        setIntVar(state, var->scope, var->var);
    }
}

void compileScope(UCompState *state, UASTNode *node) {
    UASTScopeNode *nScope = (UASTScopeNode*)node;
    pushScope(state, &nScope->scope);

    /* compile the statements in the scope */
    compileAST(state, node->left);

    popScope(state);
}

void compileIf(UCompState *state, UASTNode *node) {
    UASTIfNode *ifNode = (UASTIfNode*)node;
    int jmpID = newLbl(state);

    /* compile conditional */
    UVarType type = compileExpression(state, node->left);

    if (!tryTypeCast(state, type, TYPE_BOOL))
        cErrorNode(state, (UASTNode*)node->left, "Cannot cast type '%s' to type '%s'", getTypeName(type), getTypeName(TYPE_BOOL));

    if (ifNode->elseBlock) {
        int tmpJmp = jmpID;
        /* write comparison jump, if the flag is equal to true, jump to the true block */
        jmpCondSub(state, tmpJmp);
        compileAST(state, ifNode->elseBlock);
        jmpSub(state, jmpID = newLbl(state)); /* skip the true block */
        /* true block */
        defineSubLbl(state, tmpJmp);
        compileAST(state, ifNode->block);
    } else {
        /* write comparison jump, if the flag is not equal to true, skip the true block */
        fprintf(state->out, "#01 NEQ ");
        jmpCondSub(state, jmpID);
        compileAST(state, ifNode->block);
    }

    defineSubLbl(state, jmpID);
}

void compileWhile(UCompState *state, UASTNode *node) {
    UASTWhileNode *whileNode = (UASTWhileNode*)node;
    int loopStart = newLbl(state);
    int loopExit = newLbl(state);

    /* compile conditional */
    defineSubLbl(state, loopStart);
    UVarType type = compileExpression(state, node->left);

    if (!tryTypeCast(state, type, TYPE_BOOL))
        cErrorNode(state, node, "Cannot cast type '%s' to type '%s'", getTypeName(type), getTypeName(TYPE_BOOL));

    /* write comparison jump, if the flag is not equal to true, exit the loop */
    fprintf(state->out, "#01 NEQ ");
    jmpCondSub(state, loopExit);

    compileAST(state, whileNode->block);

    /* jump back to the start of the loop */
    jmpSub(state, loopStart);
    defineSubLbl(state, loopExit);
}

void compileFor(UCompState *state, UASTNode *node) {
    UVarType type;
    UASTForNode *forNode = (UASTForNode*)node;
    int loopEntry = newLbl(state);
    int loopStart = newLbl(state);
    int loopExit = newLbl(state);

    /* compile initalizer */
    compileVoidExpression(state, node->left);
    jmpSub(state, loopEntry); /* on entry, we skip the iterator */

    /* compile iterator */
    defineSubLbl(state, loopStart);
    compileVoidExpression(state, forNode->iter);

    /* compile conditional */
    defineSubLbl(state, loopEntry);
    type = compileExpression(state, forNode->cond);

    if (!tryTypeCast(state, type, TYPE_BOOL))
        cErrorNode(state, node, "Cannot cast type '%s' to type '%s'", getTypeName(type), getTypeName(TYPE_BOOL));

    /* write comparison jump, if the flag is not equal to true, exit the loop */
    fprintf(state->out, "#01 NEQ ");
    jmpCondSub(state, loopExit);

    /* finally, compile loop block */
    compileAST(state, forNode->block);

    /* jump back to the start of the loop */
    jmpSub(state, loopStart);
    defineSubLbl(state, loopExit);
}

void compileAST(UCompState *state, UASTNode *node) {
    /* STATE nodes hold the expression in node->left, and the next expression in node->right */
    while (node) {
        switch(node->type) { /* these functions should NOT leave any values on the stack */
            case NODE_STATE_PRNT: compilePrintInt(state, node); break;
            case NODE_STATE_DECLARE_VAR: compileDeclaration(state, node); break;
            case NODE_STATE_EXPR: compileVoidExpression(state, node->left); break;
            case NODE_STATE_SCOPE: compileScope(state, node); break;
            case NODE_STATE_IF: compileIf(state, node); break;
            case NODE_STATE_WHILE: compileWhile(state, node); break;
            case NODE_STATE_FOR: compileFor(state, node); break;
            default:
                cError(state, "unknown statement node!! [%d]\n", node->type);
        }

        /* move to the next statement */
        node = node->right;
    }
}

void UA_genTal(UASTRootNode *tree, FILE *out) {
    UCompState state;
    state.sCount = 0;
    state.pushed = 0;
    state.jmpID = 0;
    state.out = out;

    /* first, write the preamble */
    fwrite(preamble, sizeof(preamble)-1, 1, out);

    /* now parse the whole AST */
    pushScope(&state, &tree->scope);
    compileAST(&state, tree->_node.left);
    popScope(&state);

    /* finally, write the postamble */
    fwrite(postamble, sizeof(postamble)-1, 1, out);
}