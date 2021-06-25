#ifndef UPARSE_H
#define UPARSE_H

#include "ulex.h"

typedef enum {
    NODE_ADD,
    NODE_SUB,
    NODE_MUL,
    NODE_DIV,
    NODE_INTLIT,
    /* 
        statement nodes below
            node->left holds expression tree, node->right holds the next statement
    */
    NODE_STATE_PRNT,
    NODE_STATE_EXPR,
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

void UP_freeTree(UASTNode *tree);

#endif