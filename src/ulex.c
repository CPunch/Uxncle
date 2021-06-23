#include "umem.h"
#include "ulex.h"

typedef struct {
    UTokenType type;
    const char *word;
    int len;
} UReservedWord;

UReservedWord reservedWords[] = {
    {TOKEN_BYTE, "byte", 4},
    {TOKEN_BYTE16, "byte16", 6},
    {TOKEN_VOID, "void", 4}
};

void UL_initLexState(ULexState *state, const char *src) {
    state->current = (char*)src;
    state->line = 1;
    state->last = TOKEN_ERR;
}

UToken makeToken(ULexState *state, UTokenType type) {
    UToken token;
    token.str = state->start;
    token.len = state->current - state->start;
    token.type = type;

    /* update the state's last token type */
    state->last = type;
    return token;
}

/* ==================================[[ char helper functions ]]================================== */

/* check if the current character is a null terminator */
int isEnd(ULexState *state) {
    return *state->current == '\0';
}

/* increment the current pointer and return the previous character */
char next(ULexState *state) {
    state->current++;
    return state->current[-1];
}

char peek(ULexState *state) {
    return *state->current;
}

char peekNext(ULexState *state) {
    if (isEnd(state))
        return '\0';
    
    /* return the next character */
    return state->current[1];
}

int isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; /* identifiers can have '_' */
}

int isNumeric(char c) {
    return c >= '0' && c <= '9';
}

int isWhitespace(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

/* ==================================[[ parse long tokens ]]================================== */

void skipWhitespace(ULexState *state) {
    /* consume all whitespace */
    while (isWhitespace(peek(state))) {
        if (peek(state) == '\n') /* if it's a new line, make sure we count it */
            state->line++;
        next(state);
    }
}

UTokenType identifierType(ULexState *state) {
    int i;
    int len = state->start - state->current;

    /* walk through each reserved word and compare it */
    for (i = 0; i < sizeof(reservedWords)/sizeof(UReservedWord); i++) {
        if (reservedWords[i].len == len && memcmp(state->start, reservedWords[i].word, len))
            return reservedWords[i].type;
    }

    /* it wasn't found in the reserved word list */
    return TOKEN_IDENT;
}

UToken readNumber(ULexState *state) {
    while (isNumeric(peek(state)))
        next(state);

    return makeToken(state, TOKEN_NUMBER);
}

UToken readIdentifier(ULexState *state) {
    while (!isEnd(state) && (isAlpha(peek(state)) || isNumeric(peek(state)))) 
        next(state);

    return makeToken(state, identifierType(state)); /* is it a reserved word? */
}

UToken UL_scanNext(ULexState *state) {
    char c;

    /* mark start of token and check if it's the end of the string */
    state->start = state->current;
    if (isEnd(state))
        return makeToken(state, TOKEN_EOF);

    /* skip all whitespace characters then grab the next character */
    skipWhitespace(state);
    c = next(state);

    switch (c) {
        /* single character tokens */
        case '(': return makeToken(state, TOKEN_LEFT_PAREN);
        case ')': return makeToken(state, TOKEN_RIGHT_PAREN);
        case '{': return makeToken(state, TOKEN_LEFT_BRACE);
        case '}': return makeToken(state, TOKEN_RIGHT_BRACE);
        case '[': return makeToken(state, TOKEN_LEFT_BRACKET);
        case ']': return makeToken(state, TOKEN_RIGHT_BRACKET);
        case '+': return makeToken(state, TOKEN_PLUS);
        case '-': return makeToken(state, TOKEN_MINUS);
        case '/': return makeToken(state, TOKEN_SLASH);
        case '*': return makeToken(state, TOKEN_STAR);
        case ';': return makeToken(state, TOKEN_COLON);
        case '\0': return makeToken(state, TOKEN_EOF);
        default:
            if (isNumeric(c))
                return readNumber(state);
            
            /* its not a number, its probably a keyword or identifier */
            if (isAlpha(c))
                return readIdentifier(state);
    }

    /* it's none of those, so it's an unrecognized token. return an error result for now */
    return makeToken(state, TOKEN_ERR);
}