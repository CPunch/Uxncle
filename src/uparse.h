#ifndef UPARSE_H
#define UPARSE_H

#include "ulex.h"

#define MAX_SCOPES 32
#define MAX_LOCALS 128

#define COMMON_NODE_HEADER UASTNode _node;

typedef enum {
    /* arith op */
    NODE_ADD,
    NODE_SUB,
    NODE_MUL,
    NODE_DIV,
    NODE_EQUAL,
    NODE_NOTEQUAL,
    /* literals */
    NODE_INTLIT,
    NODE_VAR,
    NODE_ASSIGN, /* node->left holds Var node, node->right holds expression */
    /* 
        statement nodes below
            node->left holds expression tree, node->right holds the next statement
    */
    NODE_TREEROOT,
    NODE_STATE_PRNT,
    NODE_STATE_DECLARE_VAR,
    NODE_STATE_DECLARE_FUNC,
    NODE_STATE_EXPR,
    NODE_STATE_IF,
    /* scopes are different, node->left holds the statement tree for the scope, node->right holds the next statement */
    NODE_STATE_SCOPE,
} UASTNodeType;

typedef enum {
    TYPE_CHAR,
    TYPE_BOOL,
    TYPE_INT,
    TYPE_NONE
} UVarType;

typedef struct {
    UVarType type;
    char *name;
    int len;
    int scope;
    int var;
    int declared; /* if the variable can be used yet */
} UVar;

typedef struct {
    UVar vars[MAX_LOCALS];
    int vCount; /* count of active local variables */
} UScope;

typedef struct s_UASTNode {
    UASTNodeType type;
    UToken tkn;
    struct s_UASTNode *left;
    struct s_UASTNode *right;
} UASTNode;

typedef struct {
    COMMON_NODE_HEADER;
    UScope scope;
} UASTRootNode;

typedef struct {
    COMMON_NODE_HEADER;
    int var; /* index of the UVar */
    int scope; /* index of the scope */
} UASTVarNode;

typedef struct {
    COMMON_NODE_HEADER;
    int num;
} UASTIntNode;

typedef struct {
    COMMON_NODE_HEADER;
    UScope scope;
} UASTScopeNode;

typedef struct {
    COMMON_NODE_HEADER;
    UASTNode *block;
    UASTNode *elseBlock;
} UASTIfNode;

typedef struct {
    /* lexer related info */
    ULexState lstate;
    UToken current;
    UToken previous;
    /* scopes */
    UScope scopes[MAX_SCOPES];
    int sCount; /* count of active scopes */
} UParseState;

const char* getTypeName(UVarType type);

/* returns the base AST node, or NULL if a syntax error occurred */
UASTRootNode *UP_parseSource(const char *src);

void UP_freeTree(UASTNode *tree);

#endif