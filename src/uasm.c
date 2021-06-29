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

/* TODO:
    - write a thin library to handle heap allocation & deallocation. (we'll need this to store temporary values in scopes)
*/

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

void writeShortLit(UCompState *state, uint16_t lit) {
    fprintf(state->out, "#%.4x ", lit);
    state->pushed += 2;
}

void writeByteLit(UCompState *state, uint8_t lit) {
    fprintf(state->out, "#%.2x ", lit);
    state->pushed++;
}

uint16_t getSize(UCompState *state, UVar *var) {
    switch(var->type) {
        case TYPE_BYTE: return 1;
        case TYPE_SHORT: return 2;
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
    writeShortLit(state, getScopeSize(state, scope));
    fwrite(";alloc-uxncle JSR2\n", 19, 1, state->out);
    state->pushed -= 2;
}

void popScope(UCompState *state) {
    UScope *scope = state->scopes[--state->sCount];
    writeShortLit(state, getScopeSize(state, scope));
    fwrite(";dealloc-uxncle JSR2\n", 21, 1, state->out);
    state->pushed -= 2;
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

void getShortVar(UCompState *state, int scope, int var) {
    uint16_t offsetAddr = getOffset(state, scope, var);

    writeShortLit(state, offsetAddr); /* write the offset */
    fprintf(state->out, ";peek-uxncle-short JSR2\n"); /* call the mem lib */
}

void setShortVar(UCompState *state, int scope, int var) {
    uint16_t offsetAddr = getOffset(state, scope, var);

    writeShortLit(state, offsetAddr); /* write the offset */
    fprintf(state->out, ";poke-uxncle-short JSR2\n"); /* call the mem lib */
    state->pushed -= 4; /* pops the offset (short) & the value (short) */
}

UVar* getVarByID(UCompState *state, int scope, int var) {
    return &state->scopes[scope]->vars[var];
}

UVarType compileVar(UCompState *state, UASTNode *node) {
    UASTVarNode *var = (UASTVarNode*)node;
    UVar *rawVar = getVarByID(state, var->scope, var->var);

    switch(rawVar->type) {
        case TYPE_SHORT: getShortVar(state, var->scope, var->var); break;
        default:
            cError(state, "Unknown variable type! [%d]", rawVar->type);
    }
    
    return rawVar->type;
}

int compareVarTypes(UCompState *state, UVarType t1, UVarType t2) {
    return t1 == t2;
}

/* ==================================[[ arithmetic ]]================================== */

void cShortArith(UCompState *state, const char *instr) {
    fprintf(state->out, "%s2\n", instr);
    state->pushed -= 2;
}

void doArith(UCompState *state, const char *instr, UVarType type) {
    switch(type) {
        case TYPE_SHORT: cShortArith(state, instr); break;
        default:
            cError(state, "Unknown variable type! [%d]", type);
    }
}

UVarType compileExpression(UCompState *state, UASTNode *node) {
    UVarType lType = TYPE_NONE, rType = TYPE_NONE;
    /* first, traverse down the AST recusively */
    if (node->left)
        lType = compileExpression(state, node->left);
    if (node->right)
        rType = compileExpression(state, node->right);

    if (lType != TYPE_NONE && rType != TYPE_NONE && !compareVarTypes(state, lType, rType))
        cError(state, "lType '%s' doesn't match rType '%s'!", getTypeName(lType), getTypeName(rType));

    switch(node->type) {
        case NODE_ADD: doArith(state, "ADD", lType); break;
        case NODE_SUB: doArith(state, "SUB", lType); break;
        case NODE_MUL: doArith(state, "MUL", lType); break;
        case NODE_DIV: doArith(state, "DIV", lType); break;
        case NODE_SHORTLIT: writeShortLit(state, ((UASTIntNode*)node)->num); return TYPE_SHORT; break;
        case NODE_VAR: return compileVar(state, node); break;
        default:
            cError(state, "unknown AST node!! [%d]\n", node->type);
    }

    return lType;
}

void compilePrintInt(UCompState *state, UASTNode *node) {
    compileExpression(state, node->left);
    fwrite(";print-decimal JSR2 #20 .Console/char DEO\n", 42, 1, state->out);
}

void compileShort(UCompState *state, UASTNode *node) {
    UVarType type;
    UASTVarNode *var = (UASTVarNode*)node;
    UVar *rawVar = getVarByID(state, var->scope, var->var);

    /* if there's no assignment, the default value will be scary undefined memory :O */
    if (node->left) {
        type = compileExpression(state, node->left);
        if (type != TYPE_SHORT)
            cError(state, "Cannot assign type '%s' to %.*s of type 'short'", getTypeName(type), rawVar->len, rawVar->name);
        setShortVar(state, var->scope, var->var);
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
            case NODE_STATE_SHORT: compileShort(state, node); break;
            case NODE_STATE_EXPR: compileExpression(state, node->left); break;
            case NODE_STATE_SCOPE: compileScope(state, node); break;
            default:
                cError(state, "unknown statement node!! [%d]\n", node->type);
        }

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