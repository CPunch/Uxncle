#include "uparse.h"
#include "uasm.h"

int main() {
    UASTRootNode *tree = UP_parseSource(
        "int a = 2 * 4;"
        "if (a == 8)\n"
        "prntint 0xFFFF;\n"
    );
    UA_genTal(tree, fopen("bin/out.tal", "w"));

    /* clean up */
    UP_freeTree((UASTNode*)tree);
    return 0;
}