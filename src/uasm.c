#include "uasm.h"
#include "uparse.h"

/* compiler state */
typedef struct {
    FILE *out;
    UScope *scopes[MAX_SCOPES];
    int sCount;
    int pushed; /* current bytes on the stack */
} UCompState;

static const char preamble[] =
    "|10 @Console [ &pad $8 &char $1 &byte $1 &short $2 &string $2 ]\n"
    "|0000\n"
    "@number [ &started $1 ]\n"
    "@uxncle [ &heap $2 ]\n"
    "|0100\n"
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
        case TYPE_CHAR: return 1;
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
    writeIntLit(state, getScopeSize(state, scope));
    fwrite(";alloc-uxncle JSR2\n", 19, 1, state->out);
    state->pushed -= SIZE_INT;
}

void popScope(UCompState *state) {
    UScope *scope = state->scopes[--state->sCount];
    writeIntLit(state, getScopeSize(state, scope));
    fwrite(";dealloc-uxncle JSR2\n", 21, 1, state->out);
    state->pushed -= SIZE_INT;
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
        case TYPE_CHAR: fprintf(state->out, "DUP\n"); state->pushed+=SIZE_CHAR; break;
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

UVarType compileAssignment(UCompState *state, UASTNode *node) {
    UASTVarNode *nVar = (UASTVarNode*)node->left;
    UVar *rawVar = getVarByID(state, nVar->scope, nVar->var);
    UVarType expType;

    /* get the value of the expression */
    expType = compileExpression(state, node->right);

    /* make sure we can assign the value of this expression to this variable */
    if (!compareVarTypes(state, expType, rawVar->type))
        cErrorNode(state, node, "Cannot assign type '%s' to '%.*s' of type '%s'", getTypeName(expType), rawVar->len, rawVar->name, getTypeName(rawVar->type));

    /* duplicate the value on the stack */
    dupValue(state, expType);

    /* assign the copy to the variable, leaving a copy on the stack for the expression */
    setVar(state, nVar->scope, nVar->var, expType);

    return expType;
}

UVarType compileExpression(UCompState *state, UASTNode *node) {
    UVarType lType = TYPE_NONE, rType = TYPE_NONE;

    /* assignments are special, they're like statements but can be inside of expressions */
    if (node->type == NODE_ASSIGN)
        return compileAssignment(state, node);

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
        case NODE_INTLIT: writeIntLit(state, ((UASTIntNode*)node)->num); return TYPE_INT; break;
        case NODE_VAR: return compileVar(state, node); break;
        default:
            cError(state, "unknown AST node!! [%d]\n", node->type);
    }

    return lType;
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
        if (compareVarTypes(state, type, rawVar->type))
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

void compileAST(UCompState *state, UASTNode *node) {
    /* STATE nodes hold the expression in node->left, and the next expression in node->right */
    while (node) {
        switch(node->type) {
            case NODE_STATE_PRNT: compilePrintInt(state, node); break;
            case NODE_STATE_DECLARE_VAR: compileDeclaration(state, node); break;
            case NODE_STATE_EXPR: compileExpression(state, node->left); break;
            case NODE_STATE_SCOPE: compileScope(state, node); break;
            default:
                cError(state, "unknown statement node!! [%d]\n", node->type);
        }

        /* clean the stack */
        pop(state, state->pushed);

        /* move to the next statement */
        node = node->right;
    }
}

void UA_genTal(UASTNode *tree, FILE *out) {
    UCompState state;
    state.sCount = 0;
    state.pushed = 0;
    state.out = out;

    /* first, write the preamble */
    fwrite(preamble, sizeof(preamble)-1, 1, out);

    /* now parse the whole AST */
    compileAST(&state, tree);

    /* finally, write the postamble */
    fwrite(postamble, sizeof(postamble)-1, 1, out);
}