#include "umem.h"
#include "uparse.h"

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    // =
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_PRIMARY        // everything else
} Precedence;

typedef UASTNode* (*ParseFunc)(UParseState* pstate);

typedef struct {
    ParseFunc prefix;
    ParseFunc infix;
    Precedence level;
} ParseRule;

/* ==================================[[ generic helper functions ]]================================== */

UASTNode *newNode(UParseState *state, UASTNodeType type, UASTNode *left, UASTNode *right) {
    UASTNode *node = UM_realloc(NULL, sizeof(UASTNode));
    node->left = left;
    node->right = right;

    return node;
}

void errorAt(UToken *token, int line, const char *fmt, va_list args) {
    print("Syntax error at '%*s' on line %d", token->len, token->str, line);
    vprintf(fmt, args);
    exit(0);
}

void error(UParseState *state, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    errorAt(&state->current, state->lstate.line, fmt, args);
    va_end(args);
}

void next(UParseState *state) {
    state->previous = state->current;
    state->current = UL_scanNext(&state->lstate);
}

UASTNode *binExpression(UParseState *state) {
    
}

ParseRule ruleTable[] = {
    /* keywords */
    {NULL, NULL, PREC_NONE}, /* TOKEN_BYTE */
    {NULL, NULL, PREC_NONE}, /* TOKEN_BYTE16 */
    {NULL, NULL, PREC_NONE}, /* TOKEN_VOID */

    /* literals */
    {NULL, NULL, PREC_NONE}, /* TOKEN_IDENT */
    {NULL, NULL, PREC_NONE}, /* TOKEN_NUMBER */

    {NULL, NULL, PREC_NONE}, /* TOKEN_LEFT_BRACE */
    {NULL, NULL, PREC_NONE}, /* TOKEN_RIGHT_BRACE */
    {NULL, NULL, PREC_NONE}, /* TOKEN_LEFT_PAREN */
    {NULL, NULL, PREC_NONE}, /* TOKEN_RIGHT_PAREN */
    {NULL, NULL, PREC_NONE}, /* TOKEN_LEFT_BRACKET */
    {NULL, NULL, PREC_NONE}, /* TOKEN_RIGHT_BRACKET */
    {NULL, NULL, PREC_NONE}, /* TOKEN_COLON */
    {NULL, NULL, PREC_NONE}, /* TOKEN_POUND */
    {NULL, NULL, PREC_NONE}, /* TOKEN_PLUS */
    {NULL, NULL, PREC_NONE}, /* TOKEN_MINUS */
    {NULL, NULL, PREC_NONE}, /* TOKEN_SLASH */
    {NULL, NULL, PREC_NONE}, /* TOKEN_STAR */

    {NULL, NULL, PREC_NONE}, /* TOKEN_EOF */
    {NULL, NULL, PREC_NONE}, /* TOKEN_ERR */
};

UASTNode *UP_parseSource(const char *src) {
    UParseState state;
    UL_initLexState(&state.lstate, src);

    return NULL;
}