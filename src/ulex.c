#include "umem.h"
#include "ulex.h"

typedef struct {
    UTokenType type;
    const char *word;
    int len;
} UReservedWord;

UReservedWord reservedWords[] = {
    {TOKEN_CHAR, "char", 4},
    {TOKEN_INT, "int", 3},
    {TOKEN_VOID, "void", 4},
    {TOKEN_PRINTINT, "prntint", 7}
};

void UL_initLexState(ULexState *state, const char *src) {
    state->current = (char*)src;
    state->line = 1;
    state->last = TOKEN_ERR;
}

UToken makeToken(ULexState *state, UTokenType type) {
    UToken tkn;
    tkn.str = state->start;
    tkn.len = state->current - state->start;
    tkn.line = state->line;
    tkn.type = type;

    /* update the state's last token type */
    state->last = type;
    return tkn;
}

UToken makeError(ULexState *state, const char *msg) {
    UToken tkn;
    tkn.str = (char*)msg;
    tkn.len = strlen(msg);
    tkn.line = state->line;
    tkn.type = TOKEN_ERR;

    return tkn;
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
        /* if it's a new line, make sure we count it */
        if (peek(state) == '\n')
            state->line++;
        next(state);
    }
}

UTokenType identifierType(ULexState *state) {
    int i;
    int len = state->current - state->start;

    /* walk through each reserved word and compare it */
    for (i = 0; i < sizeof(reservedWords)/sizeof(UReservedWord); i++) {
        if (reservedWords[i].len == len && !memcmp(state->start, reservedWords[i].word, len))
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

int consumeCharacter(ULexState *state) {
    char c = next(state);
    if (c == '\\') {
        switch(next(state)) {
            case '\\': return '\\';
            case 'n': return '\n';
            case 't': return '\t';
            case 'r': return '\r';
            default:
                return -1; /* error result */
        }
    }

    return c;
}

UToken readCharacter(ULexState *state) {
    if (isEnd(state))
        return makeError(state, "Expected end to character literal!");

    /* consume character */
    if (consumeCharacter(state) == -1)
        return makeError(state, "Unknown special character!");

    if (next(state) != '\'')
        return makeError(state, "Expected end to character literal!");

    return makeToken(state, TOKEN_CHAR_LIT);
}

UToken UL_scanNext(ULexState *state) {
    char c;

    /* check if it's the end of the string */
    if (isEnd(state))
        return makeToken(state, TOKEN_EOF);

    /* skip all whitespace characters then grab the next character */
    skipWhitespace(state);
    state->start = state->current;
    c = next(state);

    switch (c) {
        /* single character tokens */
        case '(': return makeToken(state, TOKEN_LEFT_PAREN);
        case ')': return makeToken(state, TOKEN_RIGHT_PAREN);
        case '{': return makeToken(state, TOKEN_LEFT_BRACE);
        case '}': return makeToken(state, TOKEN_RIGHT_BRACE);
        case '[': return makeToken(state, TOKEN_LEFT_BRACKET);
        case ']': return makeToken(state, TOKEN_RIGHT_BRACKET);
        case '=': return makeToken(state, TOKEN_EQUAL);
        case '+': return makeToken(state, TOKEN_PLUS);
        case '-': return makeToken(state, TOKEN_MINUS);
        case '/': return makeToken(state, TOKEN_SLASH);
        case '*': return makeToken(state, TOKEN_STAR);
        case ';': return makeToken(state, TOKEN_COLON);
        case '\'': return readCharacter(state);
        case '\0': return makeToken(state, TOKEN_EOF);
        default:
            if (isNumeric(c))
                return readNumber(state);
            
            /* its not a number, its probably a keyword or identifier */
            if (isAlpha(c))
                return readIdentifier(state);
    }

    /* it's none of those, so it's an unrecognized token */
    return makeToken(state, TOKEN_UNREC);
}