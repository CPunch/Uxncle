#include "umem.h"
#include "uparse.h"

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    /* = */
    PREC_TERM,          /* + - */
    PREC_FACTOR,        /* * / */
    PREC_LITERAL,       /* literal values */
    PREC_PRIMARY        /* everything else */
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

int str2int(char *str, int len) {
    int i;
    int ret = 0;

    for(i = 0; i < len; ++i) {
        ret = ret * 10 + (str[i] - '0');
    }

    return ret;
}

/* ==================================[[ generic helper functions ]]================================== */

UASTNode *newNode(UParseState *state, UASTNodeType type, UASTNode *left, UASTNode *right) {
    UASTNode *node = UM_realloc(NULL, sizeof(UASTNode));
    node->type = type;
    node->left = left;
    node->right = right;

    return node;
}

UASTNode *newNumNode(UParseState *state, UASTNode *left, UASTNode *right, int num) {
    UASTNode *node = newNode(state, NODE_INTLIT, left, right);
    node->num = num;

    return node;
}

void errorAt(UToken *token, int line, const char *fmt, va_list args) {
    printf("Syntax error at '%.*s' on line %d\n\t", token->len, token->str, line);
    vprintf(fmt, args);
    printf("\n");
    exit(EXIT_FAILURE);
}

void error(UParseState *state, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    errorAt(&state->previous, state->lstate.line, fmt, args);
    va_end(args);
}

void advance(UParseState *state) {
    state->previous = state->current;
    state->current = UL_scanNext(&state->lstate);

    printf("consumed '%.*s', with type %d\n", state->current.len, state->current.str, state->current.type);

    if (state->current.type == TOKEN_ERR)
        error(state, "unrecognized symbol '%.*s'!", state->current.len, state->current.str);
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

int isPEnd(UParseState *state) {
    return check(state, TOKEN_ERR) || check(state, TOKEN_EOF);
}

ParseRule* getRule(UTokenType type) {
    return &ruleTable[type];
}

/* ==================================[[ parse functions ]]================================== */

UASTNode* number(UParseState *state, UASTNode *left, Precedence currPrec) {
    int num = str2int(state->previous.str, state->previous.len);
    printf("got number %d! from token '%.*s' [%d]\n", num, state->previous.len, state->previous.str, state->previous.type);
    return newNumNode(state, NULL, NULL, num);
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
            error(state, "Unknown binary operator '%.*s'!", state->current.len, state->current.str);
            return NULL;
    }

    /* grab the right node */
    right = parsePrecedence(state, NULL, currPrec);
    return newNode(state, type, left, right);
}

ParseRule ruleTable[] = {
    /* keywords */
    {NULL, NULL, PREC_NONE}, /* TOKEN_BYTE */
    {NULL, NULL, PREC_NONE}, /* TOKEN_SHORT */
    {NULL, NULL, PREC_NONE}, /* TOKEN_VOID */
    {NULL, NULL, PREC_NONE}, /* TOKEN_PRINTINT */

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

/* ==================================[[ parse statement functions ]]================================== */

UASTNode* printStatement(UParseState *state) {
    /* make our statement node & return */
    UASTNode *node = expression(state);
    return newNode(state, NODE_STATE_PRNT, node, NULL);
}

UASTNode* parsePrecedence(UParseState *state, UASTNode *left, Precedence prec) {
    ParseFunc func;

    /* grab the prefix function */
    advance(state);
    func = getRule(state->previous.type)->prefix;
    if (func == NULL) {
        error(state, "Illegal syntax! [prefix]");
        return NULL;
    }

    left = func(state, left, prec);
    while (prec <= getRule(state->current.type)->level) {
        func = getRule(state->current.type)->infix;
        if (func == NULL) {
            error(state, "Illegal syntax! [infix]");
            return NULL;
        }
        advance(state);
        left = func(state, left, getRule(state->previous.type)->level);
    }

    return left;
}

UASTNode* expression(UParseState *state) {
    UASTNode *node = parsePrecedence(state, NULL, PREC_ASSIGNMENT);

    if (!node)
        error(state, "Expected expression!");

    return node;
}

UASTNode* statement(UParseState *state) {
    UASTNode *node;

    /* find a statement match */
    if (match(state, TOKEN_PRINTINT)) {
        node = printStatement(state);
    } else {
        /* no statement match was found, just parse the expression */
        node = expression(state);
        node = newNode(state, NODE_STATE_EXPR, node, NULL);
    }

    if (!match(state, TOKEN_COLON))
        error(state, "Expected ';'!");

    return node;
}

void printNode(UASTNode *node) {
    switch(node->type) {
        case NODE_ADD: printf("ADD"); break;
        case NODE_SUB: printf("SUB"); break;
        case NODE_MUL: printf("MUL"); break;
        case NODE_DIV: printf("DIV"); break;
        case NODE_INTLIT: printf("[%d]", node->num); break;
        case NODE_STATE_PRNT: printf("PRNT"); break;
        case NODE_STATE_EXPR: printf("EXPR"); break;
        default: break;
    }
}

void printTree(UASTNode *node, int indent) {
    printf("%*s", indent, "");
    printNode(node);
    printf("\n");

    if (node->left)
        printTree(node->left, indent-5);
    if (node->right)
        printTree(node->right, indent+5);
}

UASTNode *UP_parseSource(const char *src) {
    UParseState state;
    UASTNode *root = NULL, *current = NULL;
    int treeIndent = 8;

    UL_initLexState(&state.lstate, src);
    advance(&state);

    do {
        if (root == NULL) {
            root = statement(&state);
            current = root;
        } else {
            current->right = statement(&state);
            current = current->right;
            treeIndent += 4;
        }
    } while(!isPEnd(&state));

    printTree(root, treeIndent);
    return root;
}

void UP_freeTree(UASTNode *tree) {
    if (tree->left)
        UP_freeTree(tree->left);
    if (tree->right)
        UP_freeTree(tree->right);

    UM_free(tree);
}