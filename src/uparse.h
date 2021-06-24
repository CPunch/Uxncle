#ifndef UPARSE_H
#define UPARSE_H

#include "ulex.h"

typedef enum {
    NODE_ADD,
    NODE_SUB,
    NODE_MUL,
    NODE_DIV,
    NODE_STATE, /* node->left holds expression tree, node->right holds the next statement */
    NODE_INTLIT
} UASTNodeType;

typedef enum {
    STATE_PRNT,
    STATE_EXPR
} UStateType;

typedef struct s_UASTNode {
    UASTNodeType type;
    struct s_UASTNode *left;
    struct s_UASTNode *right;
    union {
        int num;
        UStateType sType;
    };
} UASTNode;

typedef struct {
    ULexState lstate;
    UToken current;
    UToken previous;
} UParseState;

/* returns the base AST node, or NULL if a syntax error occurred */
UASTNode *UP_parseSource(const char *src);

void UP_freeTree(UASTNode *tree);

#endif