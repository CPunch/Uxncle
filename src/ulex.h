#ifndef ULEX_H
#define ULEX_H

typedef enum {
    /* keywords */
    TOKEN_BYTE,
    TOKEN_SHORT,
    TOKEN_VOID,
    TOKEN_PRINTINT,

    /* literals */
    TOKEN_IDENT,
    TOKEN_NUMBER,

    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_COLON,
    TOKEN_POUND,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_SLASH,
    TOKEN_STAR,

    TOKEN_EOF, /* end of file */
    TOKEN_ERR /* error type */
} UTokenType;

typedef struct {
    UTokenType type;
    char *str;
    int len;
} UToken;

typedef struct {
    char *current;
    char *start;
    int line;
    UTokenType last;
} ULexState;

void UL_initLexState(ULexState *state, const char *src);

/* grabs the next token from the sequence */
UToken UL_scanNext(ULexState *state);

#endif