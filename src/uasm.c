#include "uasm.h"

static const char preamble[] = 
    "|0100\n";

static const char postamble[] =
    "BRK";

void writeLit(FILE *out, int lit) {
    fprintf(out, "#%.2x ", lit);
}

void compileAST(FILE *out, UASTNode *node) {
    /* first, traverse down the AST recusively */
    if (node->left)
        compileAST(out, node->left);
    if (node->right)
        compileAST(out, node->right);

    switch(node->type) {
        case NODE_ADD: fwrite("ADD\n", 4, 1, out); break;
        case NODE_SUB: fwrite("SUB\n", 4, 1, out); break;
        case NODE_MUL: fwrite("MUL\n", 4, 1, out); break;
        case NODE_DIV: fwrite("DIV\n", 4, 1, out); break;
        case NODE_INTLIT: writeLit(out, node->num); break;
        default:
            printf("Compiler error! unknown AST node!! [%d]\n", node->type);
            exit(0);
    }
}

void UA_genTal(UASTNode *tree, FILE *out) {
    /* first, write the preamble */
    fwrite(preamble, sizeof(preamble)-1, 1, out);

    /* now parse the whole AST */
    compileAST(out, tree);

    /* finally, write the postamble */
    fwrite(postamble, sizeof(postamble)-1, 1, out);
}