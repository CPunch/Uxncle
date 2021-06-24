#include "uasm.h"

static const char preamble[] =
    "|10 @Console [ &pad $8 &char $1 &byte $1 &short $2 &string $2 ]\n"
    "|0000\n"
    "@number [ &started $1 ]\n"
    "|0100\n";

static const char postamble[] =
    "\n;print-decimal JSR2\n"
    "BRK\n"
        "@print-decimal\n"
        "\t#00 .number/started STZ\n"
        "\tDUP2 #2710 DIV2 DUP2 ,&digit JSR #2710 MUL2 SUB2\n"
        "\tDUP2 #03e8 DIV2 DUP2 ,&digit JSR #03e8 MUL2 SUB2\n"
        "\tDUP2 #0064 DIV2 DUP2 ,&digit JSR #0064 MUL2 SUB2\n"
        "\tDUP2 #000a DIV2 DUP2 ,&digit JSR #000a MUL2 SUB2\n"
        "\t,&digit JSR\n"
        "\t.number/started LDZ ,&end JCN\n"
        "\tLIT '0 .Console/char DEO\n"
        "\t&end\n"
    "JMP2r\n"
    "\t&digit\n"
        "\tSWP POP\n"
        "\tDUP .number/started LDZ ORA #02 JCN\n"
        "\tPOP JMP2r\n"
        "\tLIT '0 ADD .Console/char DEO\n"
        "\t#01 .number/started STZ\n"
    "JMP2r\n";

void writeShortLit(FILE *out, int lit) {
    fprintf(out, "#%.4x ", lit);
}

void compileAST(FILE *out, UASTNode *node) {
    /* first, traverse down the AST recusively */
    if (node->left)
        compileAST(out, node->left);
    if (node->right)
        compileAST(out, node->right);

    switch(node->type) {
        case NODE_ADD: fwrite("ADD2\n", 5, 1, out); break;
        case NODE_SUB: fwrite("SUB2\n", 5, 1, out); break;
        case NODE_MUL: fwrite("MUL2\n", 5, 1, out); break;
        case NODE_DIV: fwrite("DIV2\n", 5, 1, out); break;
        case NODE_INTLIT: writeShortLit(out, node->num); break;
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