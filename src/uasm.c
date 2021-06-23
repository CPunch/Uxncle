#include "uasm.h"

static const char preamble[] = 
    "|10 @Console [ &pad $8 &char ]\n"
    "|0100\n";

static const char postamble[] =
    "BRK";

void UA_genTal(UASTNode *tree, FILE *out) {
    /* first, write the preamble */
    fwrite(preamble, sizeof(preamble), 1, out);

    /* now parse the whole AST */

    /* finally, write the postamble */
    fwrite(postamble, sizeof(postamble), 1, out);
}