#include "umem.h"
#include "uparse.h"

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    /* = */
    PREC_COMPAR,        /* == != */
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
UASTNode* statement(UParseState *state);
ParseRule ruleTable[];

/* ==================================[[ generic helper functions ]]================================== */

void errorAt(UToken *token, const char *fmt, va_list args) {
    printf("Syntax error at '%.*s' on line %d\n\t", token->len, token->str, token->line);
    vprintf(fmt, args);
    printf("\n");
    exit(EXIT_FAILURE);
}

void error(UParseState *state, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    errorAt(&state->previous, fmt, args);
    va_end(args);
}

UASTNode *newBaseNode(UParseState *state, UToken tkn, size_t size, UASTNodeType type, UASTNode *left, UASTNode *right) {
    UASTNode *node = UM_realloc(NULL, size);
    node->type = type;
    node->left = left;
    node->right = right;
    node->tkn = tkn;

    return node;
}

UASTNode *newNode(UParseState *state, UToken tkn, UASTNodeType type, UASTNode *left, UASTNode *right) {
    return newBaseNode(state, tkn, sizeof(UASTNode), type, left, right);
}

UASTNode *newNumNode(UParseState *state, UToken tkn, UASTNode *left, UASTNode *right, int num) {
    UASTIntNode *node = (UASTIntNode*)newBaseNode(state, tkn,  sizeof(UASTIntNode), NODE_INTLIT, left, right);
    node->num = num;
    return (UASTNode*)node;
}

UASTNode *newScopeNode(UParseState *state, UToken tkn, UASTNode *left, UASTNode *right, UScope *scope) {
    UASTScopeNode *node = (UASTScopeNode*)newBaseNode(state, tkn, sizeof(UASTScopeNode), NODE_STATE_SCOPE, left, right);
    node->scope = *scope;
    return (UASTNode*)node;
}

UScope* newScope(UParseState *state) {
    UScope *scope = &state->scopes[state->sCount++];

    /* sanity check */
    if (state->sCount >= MAX_SCOPES)
        error(state, "Max scope limit reached!");

    scope->vCount = 0;
    return scope;
}

void endScope(UParseState *state) {
    state->sCount--;
}

UScope* getScope(UParseState *state) {
    return &state->scopes[state->sCount-1];
}

UVar* findVar(UParseState *state, char *name, int length) {
    int i, z;

    /* walk the scopes and variables */
    for (i = state->sCount-1; i >= 0; i--) 
        for (z = state->scopes[i].vCount-1; z >= 0; z--)
            if (state->scopes[i].vars[z].len == length && !memcmp(state->scopes[i].vars[z].name, name, length))
                return &state->scopes[i].vars[z];

    /* var wasn't found */
    return NULL;
}

int newVar(UParseState *state, UVarType type, char *name, int length) {
    UScope *scope = getScope(state);
    UVar *var = &scope->vars[scope->vCount++];

    /* make sure the variable name wasn't already in use */
    if (findVar(state, name, length) != NULL)
        error(state, "Variable '%.*s' already declared!", length, name);

    /* sanity check */
    if (scope->vCount >= MAX_LOCALS)
        error(state, "Max local limit reached, too many locals declared in scope!");

    /* set the var and return */
    var->type = type;
    var->name = name;
    var->len = length;
    var->scope = state->sCount-1;
    var->var = scope->vCount-1;
    var->declared = 0;
    return scope->vCount-1;
}

void advance(UParseState *state) {
    state->previous = state->current;
    state->current = UL_scanNext(&state->lstate);

    switch(state->current.type) {
        case TOKEN_UNREC: error(state, "Unrecognized symbol '%.*s'!", state->current.len, state->current.str); break;
        case TOKEN_ERR: error(state, "%.*s", state->current.len, state->current.str); break;
        default: break;
    }
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
    int num = strtol(state->previous.str, NULL, 10);
    return newNumNode(state, state->previous, NULL, NULL, num);
}

UASTNode* hexnum(UParseState *state, UASTNode *left, Precedence currPrec) {
    int num = strtol(state->previous.str + 2, NULL, 16); /* +2 to skip 0x */
    return newNumNode(state, state->previous, NULL, NULL, num);
}

UASTNode* assignment(UParseState *state, UASTNode *left, Precedence currPrec) {
    UToken tkn = state->previous;
    if (left->type != NODE_VAR)
        error(state, "Expected identifier before '='!");

    UASTNode *right = expression(state);
    return newNode(state, tkn, NODE_ASSIGN, left, right);
}

UASTNode* binOperator(UParseState *state, UASTNode *left, Precedence currPrec) {
    UASTNodeType type;
    UToken tkn = state->previous;
    UASTNode *right;

    /* grab the node type */
    switch (state->previous.type) {
        case TOKEN_PLUS: type = NODE_ADD; break;
        case TOKEN_MINUS: type = NODE_SUB; break;
        case TOKEN_STAR: type = NODE_MUL; break;
        case TOKEN_SLASH: type = NODE_DIV; break;
        case TOKEN_LESS: type = NODE_LESS; break;
        case TOKEN_GREATER: type = NODE_GREATER; break;
        case TOKEN_EQUAL_EQUAL: type = NODE_EQUAL; break;
        case TOKEN_BANG_EQUAL: type = NODE_NEQUAL; break;
        case TOKEN_LESS_EQUAL: type = NODE_LESS_EQUAL; break;
        case TOKEN_GREATER_EQUAL: type = NODE_GREATER_EQUAL; break;
        default:
            error(state, "Unknown binary operator '%.*s'!", state->current.len, state->current.str);
            return NULL;
    }

    /* grab the right node */
    right = parsePrecedence(state, NULL, currPrec);
    return newNode(state, tkn, type, left, right);
}

UASTNode* identifer(UParseState *state, UASTNode *left, Precedence currPrec) {
    UASTVarNode *nVar;
    UVar *var = findVar(state, state->previous.str, state->previous.len);

    if (var == NULL)
        error(state, "Identifer '%.*s' not found!", state->previous.len, state->previous.str);

    /* finally, create the Var node */
    nVar = (UASTVarNode*)newBaseNode(state, state->previous, sizeof(UASTVarNode), NODE_VAR, NULL, NULL);
    nVar->var = var->var;
    nVar->scope = var->scope;
    return (UASTNode*)nVar;
}

ParseRule ruleTable[] = {
    /* keywords */
    {NULL, NULL, PREC_NONE}, /* TOKEN_CHAR */
    {NULL, NULL, PREC_NONE}, /* TOKEN_INT */
    {NULL, NULL, PREC_NONE}, /* TOKEN_VOID */
    {NULL, NULL, PREC_NONE}, /* TOKEN_BOOL */
    {NULL, NULL, PREC_NONE}, /* TOKEN_PRINTINT */
    {NULL, NULL, PREC_NONE}, /* TOKEN_IF */
    {NULL, NULL, PREC_NONE}, /* TOKEN_ELSE */
    {NULL, NULL, PREC_NONE}, /* TOKEN_WHILE */
    {NULL, NULL, PREC_NONE}, /* TOKEN_FOR */

    /* literals */
    {identifer, NULL, PREC_LITERAL}, /* TOKEN_IDENT */
    {number, NULL, PREC_LITERAL}, /* TOKEN_NUMBER */
    {hexnum, NULL, PREC_LITERAL}, /* TOKEN_HEX */
    {NULL, NULL, PREC_NONE}, /* TOKEN_CHAR_LIT */

    {NULL, NULL, PREC_NONE}, /* TOKEN_LEFT_BRACE */
    {NULL, NULL, PREC_NONE}, /* TOKEN_RIGHT_BRACE */
    {NULL, NULL, PREC_NONE}, /* TOKEN_LEFT_PAREN */
    {NULL, NULL, PREC_NONE}, /* TOKEN_RIGHT_PAREN */
    {NULL, NULL, PREC_NONE}, /* TOKEN_LEFT_BRACKET */
    {NULL, NULL, PREC_NONE}, /* TOKEN_RIGHT_BRACKET */
    {NULL, NULL, PREC_NONE}, /* TOKEN_COLON */
    {NULL, NULL, PREC_NONE}, /* TOKEN_POUND */
    {NULL, assignment, PREC_ASSIGNMENT}, /* TOKEN_EQUAL */
    {NULL, binOperator, PREC_TERM}, /* TOKEN_PLUS */
    {NULL, binOperator, PREC_TERM}, /* TOKEN_MINUS */
    {NULL, binOperator, PREC_FACTOR}, /* TOKEN_SLASH */
    {NULL, binOperator, PREC_FACTOR}, /* TOKEN_STAR */
    {NULL, NULL, PREC_NONE}, /* TOKEN_BANG */
    {NULL, binOperator, PREC_COMPAR}, /* TOKEN_LESS */
    {NULL, binOperator, PREC_COMPAR}, /* TOKEN_GREATER */

    {NULL, binOperator, PREC_COMPAR}, /* TOKEN_EQUAL_EQUAL */
    {NULL, binOperator, PREC_COMPAR}, /* TOKEN_BANG_EQUAL */
    {NULL, binOperator, PREC_COMPAR}, /* TOKEN_LESS_EQUAL */
    {NULL, binOperator, PREC_COMPAR}, /* TOKEN_GREATER_EQUAL */
    {NULL, NULL, PREC_NONE}, /* TOKEN_EOF */
    {NULL, NULL, PREC_NONE}, /* TOKEN_UNREC */
    {NULL, NULL, PREC_NONE}, /* TOKEN_ERR */
};

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

/* ==================================[[ parse statement functions ]]================================== */

UASTNode* parseScope(UParseState *state, int expectBrace) {
    UASTNode *root = NULL, *current = NULL;

    do {
        if (root == NULL) {
            root = statement(state);
            current = root;
        } else {
            current->right = statement(state);
            current = current->right;
        }
    } while(!isPEnd(state) && (!expectBrace || !check(state, TOKEN_RIGHT_BRACE)));

    if (expectBrace && !match(state, TOKEN_RIGHT_BRACE))
        error(state, "Expected '}' to end scope!");

    return root;
}

UASTNode* printStatement(UParseState *state) {
    UToken tkn = state->previous;
    /* make our statement node & return */
    return newNode(state, tkn, NODE_STATE_PRNT, expression(state), NULL);
}

UASTNode* varTypeStatement(UParseState *state, UVarType type) {
    UASTVarNode *node;
    int var;

    /* consume the identifer */
    if (!match(state, TOKEN_IDENT))
        error(state, "Expected identifer!");

    /* define the variable */
    var = newVar(state, type, state->previous.str, state->previous.len);

    /* if it's assigned a value, evaluate the expression & set the left node, if not set it to NULL */
    node = (UASTVarNode*)newBaseNode(state, state->previous, sizeof(UASTVarNode), NODE_STATE_DECLARE_VAR, (match(state, TOKEN_EQUAL)) ? expression(state) : NULL, NULL);
    node->var = var;
    node->scope = state->sCount-1;
    return (UASTNode*)node;
}

UASTNode* scopeStatement(UParseState *state) {
    UASTScopeNode *node;
    UToken tkn = state->previous;
    UScope *scope = newScope(state);

    /* create scope node and copy the finished scope struct */
    node = (UASTScopeNode*)newBaseNode(state, tkn, sizeof(UASTScopeNode), NODE_STATE_SCOPE, parseScope(state, 1), NULL);
    node->scope = *scope;

    endScope(state);

    return (UASTNode*)node;
}

UASTNode* ifStatement(UParseState *state) {
    UASTIfNode *node = (UASTIfNode*)newBaseNode(state, state->previous, sizeof(UASTIfNode), NODE_STATE_IF, NULL, NULL);

    if (!match(state, TOKEN_LEFT_PAREN))
        error(state, "Expected '(' to start if conditional!");

    /* set the expression */
    node->_node.left = expression(state);

    if (!match(state, TOKEN_RIGHT_PAREN))
        error(state, "Expected ')' to end if conditional!");

    /* parse the true block */
    node->block = statement(state);

    /* if there's an else block, parse it too */
    node->elseBlock = match(state, TOKEN_ELSE) ? statement(state) : NULL;
    return (UASTNode*)node;
}

UASTNode* whileStatement(UParseState *state) {
    UASTWhileNode *node = (UASTWhileNode*)newBaseNode(state, state->previous, sizeof(UASTWhileNode), NODE_STATE_WHILE, NULL, NULL);

    if (!match(state, TOKEN_LEFT_PAREN))
        error(state, "Expected '(' to start while conditional!");

    /* set the expression */
    node->_node.left = expression(state);

    if (!match(state, TOKEN_RIGHT_PAREN))
        error(state, "Expected ')' to end while conditional!");

    /* parse the loop block */
    node->block = statement(state);
    return (UASTNode*)node;
}

UASTNode* forStatement(UParseState *state) {
    UASTForNode *node = (UASTForNode*)newBaseNode(state, state->previous, sizeof(UASTForNode), NODE_STATE_FOR, NULL, NULL);

    if (!match(state, TOKEN_LEFT_PAREN))
        error(state, "Expected '(' to start for initalizer!");

    /* set the initalizer */
    node->_node.left = expression(state);

    if (!match(state, TOKEN_COLON))
        error(state, "Expected ';' to start for conditional!");

    node->cond = expression(state);

    if (!match(state, TOKEN_COLON))
        error(state, "Expected ';' to start for iterator!");

    node->iter = expression(state);

    if (!match(state, TOKEN_RIGHT_PAREN))
        error(state, "Expected ')' to end for iterator!");

    /* parse the loop block */
    node->block = statement(state);
    return (UASTNode*)node;
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
    } else if (match(state, TOKEN_INT)) {
        node = varTypeStatement(state, TYPE_INT);
    } else if (match(state, TOKEN_BOOL)) {
        node = varTypeStatement(state, TYPE_BOOL);
    /* the statements below don't require a colon, they directly return skipping that check */
    } else if (match(state, TOKEN_LEFT_BRACE)) {
        return scopeStatement(state);
    } else if (match(state, TOKEN_IF)) {
        return ifStatement(state);
    } else if (match(state, TOKEN_WHILE)) {
        return whileStatement(state);
    } else if (match(state, TOKEN_FOR)) {
        return forStatement(state);
    } else {
        UToken tkn = state->previous;
        /* no statement match was found, just parse the expression */
        node = expression(state);
        node = newNode(state, tkn, NODE_STATE_EXPR, node, NULL);
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
        case NODE_LESS: printf("LTH"); break;
        case NODE_GREATER: printf("GTH"); break;
        case NODE_LESS_EQUAL: printf("LEQ"); break;
        case NODE_GREATER_EQUAL: printf("GEQ"); break;
        case NODE_EQUAL: printf("EQU"); break;
        case NODE_NEQUAL: printf("NEQ"); break;
        case NODE_ASSIGN: printf("ASSIGN"); break;
        case NODE_INTLIT: printf("[%d]", ((UASTIntNode*)node)->num); break;
        case NODE_TREEROOT: printf("ROOT"); break;
        case NODE_STATE_PRNT: printf("PRNT"); break;
        case NODE_STATE_SCOPE: printf("SCPE"); break;
        case NODE_STATE_DECLARE_VAR: printf("NVAR"); break;
        case NODE_STATE_IF: printf("IF"); break;
        case NODE_STATE_WHILE: printf("WHIL"); break;
        case NODE_STATE_FOR: printf("FOR"); break;
        case NODE_VAR: printf("VAR[%d]", ((UASTVarNode*)node)->var); break;
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

const char* getTypeName(UVarType type) {
    switch(type) {
        case TYPE_INT: return "int";
        case TYPE_CHAR: return "char";
        case TYPE_BOOL: return "bool";
        default:
            return "<errtype>";
    }
}

UASTRootNode *UP_parseSource(const char *src) {
    UParseState state;
    UASTRootNode *root = NULL;
    UScope *scope;

    UL_initLexState(&state.lstate, src);
    advance(&state);
    state.sCount = 0;
    scope = newScope(&state);

    /* create scope node and copy the finished scope struct */
    root = (UASTRootNode*)newBaseNode(&state, state.previous, sizeof(UASTRootNode), NODE_STATE_SCOPE, parseScope(&state, 0), NULL);
    root->scope = *scope;

    endScope(&state);
    /* printTree((UASTNode*)root, 16); */
    return root;
}

void UP_freeTree(UASTNode *tree) {
    if (tree->left)
        UP_freeTree(tree->left);
    if (tree->right)
        UP_freeTree(tree->right);

    UM_free(tree);
}