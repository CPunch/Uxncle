#ifndef UPARSE_H
#define UPARSE_H

#include "ulex.h"

typedef enum {
    NODE_ADD,
    NODE_SUB
} UASTNodeType;

typedef struct s_UASTNode {
    UASTNodeType type;
    struct s_UASTNode *left;
    struct s_UASTNode *right;
    union {
        int num;
    };
} UASTNode;

typedef struct {
    ULexState lstate;
    UToken current;
    UToken previous;
} UParseState;

/* returns the base AST node, or NULL if a syntax error occurred */
UASTNode *UP_parseSource(const char *src);

#endif