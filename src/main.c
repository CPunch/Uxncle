#include "uparse.h"
#include "uasm.h"

int main() {
    UASTRootNode *tree = UP_parseSource(
        "int i = 0;\n"
        "while (i != 10) {\n"
        "   prntint i;"
        "   i = i + 1;\n"
        "}\n"
    );
    UA_genTal(tree, fopen("bin/out.tal", "w"));

    /* clean up */
    UP_freeTree((UASTNode*)tree);
    return 0;
}