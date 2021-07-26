#ifndef ULEX_H
#define ULEX_H

typedef enum {
    /* keywords */
    TOKEN_CHAR,
    TOKEN_INT,
    TOKEN_VOID,
    TOKEN_BOOL,
    TOKEN_PRINTINT,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,

    /* literals */
    TOKEN_IDENT,
    TOKEN_NUMBER,
    TOKEN_HEX,
    TOKEN_CHAR_LIT,

    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_COLON,
    TOKEN_POUND,
    TOKEN_EQUAL,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_SLASH,
    TOKEN_STAR,
    TOKEN_BANG,
    TOKEN_LESS,
    TOKEN_GREATER,

    /* two character tokens */
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG_EQUAL,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,

    TOKEN_EOF, /* end of file */
    TOKEN_UNREC, /* unrecognized symbol */
    TOKEN_ERR /* error type */
} UTokenType;

typedef struct {
    UTokenType type;
    char *str;
    int len;
    int line;
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