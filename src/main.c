#include "uparse.h"
#include "uasm.h"

int main() {
    UASTRootNode *tree = UP_parseSource(
        "int a = 2 * 4;\n"
        "if (a == 9)\n"
        "prntint 0xFFFF;\n"
        "else\n"
        "prntint a;"
    );
    UA_genTal(tree, fopen("bin/out.tal", "w"));

    /* clean up */
    UP_freeTree((UASTNode*)tree);
    return 0;
}