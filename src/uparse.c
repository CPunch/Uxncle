#include "umem.h"
#include "uparse.h"

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    // =
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_LITERAL,       // literal values
    PREC_PRIMARY        // everything else
} Precedence;

typedef UASTNode* (*ParseFunc)(UParseState *state, UASTNode *left, Precedence currPrec);

typedef struct {
    ParseFunc prefix;
    ParseFunc infix;
    Precedence level;
} ParseRule;

UASTNode* parsePrecedence(UParseState *state, UASTNode *left, Precedence prec);
UASTNode* expression(UParseState *state);
ParseRule ruleTable[];

/* ==================================[[ generic helper functions ]]================================== */

UASTNode *newNode(UParseState *state, UASTNodeType type, UASTNode *left, UASTNode *right) {
    UASTNode *node = UM_realloc(NULL, sizeof(UASTNode));
    node->type = type;
    node->left = left;
    node->right = right;

    return node;
}

void errorAt(UToken *token, int line, const char *fmt, va_list args) {
    printf("Syntax error at '%*s' on line %d", token->len, token->str, line);
    vprintf(fmt, args);
    exit(0);
}

void error(UParseState *state, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    errorAt(&state->current, state->lstate.line, fmt, args);
    va_end(args);
}

void advance(UParseState *state) {
    state->previous = state->current;
    state->current = UL_scanNext(&state->lstate);
}

int check(UParseState *state, UTokenType type) {
    return state->current.type == type;
}

int match(UParseState *state, UTokenType type) {
    if (!check(state, type))
        return 0;

    /* it matched! consume the token and return true */
    advance(state);
    return 1;
}

ParseRule* getRule(UTokenType type) {
    return &ruleTable[type];
}

const char* getNodeType(UASTNodeType type) {
    switch(type) {
        case NODE_ADD: return "ADD";
        case NODE_SUB: return "SUB";
        case NODE_MUL: return "MUL";
        case NODE_DIV: return "DIV";
        case NODE_NUM: return "NUM";
        default: return "err";
    }
}

/* ==================================[[ parse functions ]]================================== */

UASTNode* number(UParseState *state, UASTNode *left, Precedence currPrec) {
    int num = atoi(state->current.str);
    return newNode(state, NODE_NUM, NULL, NULL);
}

UASTNode* binOperator(UParseState *state, UASTNode *left, Precedence currPrec) {
    UASTNodeType type;
    UASTNode *right;

    /* grab the node type */
    switch (state->previous.type) {
        case TOKEN_PLUS: type = NODE_ADD; break;
        case TOKEN_MINUS: type = NODE_SUB; break;
        case TOKEN_STAR: type = NODE_MUL; break;
        case TOKEN_SLASH: type = NODE_DIV; break;
        default:
            error(state, "Unknown binary operator '%*s'!", state->current.len, state->current.str);
            return NULL;
    }

    /* grab the right node */
    right = expression(state);
    return newNode(state, type, left, right);
}

ParseRule ruleTable[] = {
    /* keywords */
    {NULL, NULL, PREC_NONE}, /* TOKEN_BYTE */
    {NULL, NULL, PREC_NONE}, /* TOKEN_BYTE16 */
    {NULL, NULL, PREC_NONE}, /* TOKEN_VOID */

    /* literals */
    {NULL, NULL, PREC_NONE}, /* TOKEN_IDENT */
    {number, NULL, PREC_LITERAL}, /* TOKEN_NUMBER */

    {NULL, NULL, PREC_NONE}, /* TOKEN_LEFT_BRACE */
    {NULL, NULL, PREC_NONE}, /* TOKEN_RIGHT_BRACE */
    {NULL, NULL, PREC_NONE}, /* TOKEN_LEFT_PAREN */
    {NULL, NULL, PREC_NONE}, /* TOKEN_RIGHT_PAREN */
    {NULL, NULL, PREC_NONE}, /* TOKEN_LEFT_BRACKET */
    {NULL, NULL, PREC_NONE}, /* TOKEN_RIGHT_BRACKET */
    {NULL, NULL, PREC_NONE}, /* TOKEN_COLON */
    {NULL, NULL, PREC_NONE}, /* TOKEN_POUND */
    {NULL, binOperator, PREC_TERM}, /* TOKEN_PLUS */
    {NULL, binOperator, PREC_TERM}, /* TOKEN_MINUS */
    {NULL, binOperator, PREC_FACTOR}, /* TOKEN_SLASH */
    {NULL, binOperator, PREC_FACTOR}, /* TOKEN_STAR */

    {NULL, NULL, PREC_NONE}, /* TOKEN_EOF */
    {NULL, NULL, PREC_NONE}, /* TOKEN_ERR */
};


UASTNode* parsePrecedence(UParseState *state, UASTNode *left, Precedence prec) {
    ParseFunc func;

    /* grab the prefix function */
    advance(state);
    func = getRule(state->previous.type)->prefix;
    if (func == NULL) {
        error(state, "Illegal syntax!");
        return NULL;
    }

    left = func(state, left, prec);
    while (prec <= getRule(state->current.type)->level) {
        func = getRule(state->current.type)->infix;
        advance(state);
        left = func(state, left, prec);
    }

    return left;
}

UASTNode* expression(UParseState *state) {
    return parsePrecedence(state, NULL, PREC_ASSIGNMENT);
}

UASTNode* statement(UParseState *state) {
    /* TODO */
    return NULL;
}

void printTree(UASTNode *node, int indent) {
    printf("%*s%s\n", indent, "", getNodeType(node->type));
    if (node->left)
        printTree(node->left, indent-5);
    if (node->right)
        printTree(node->right, indent+5);
}

UASTNode *UP_parseSource(const char *src) {
    UParseState state;
    UL_initLexState(&state.lstate, src);

    advance(&state);

    UASTNode *tree = expression(&state);
    printTree(tree, 8);

    return NULL;
}