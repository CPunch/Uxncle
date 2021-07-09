#include "uparse.h"
#include "uasm.h"

int main() {
    UASTNode *tree = UP_parseSource(
        "int a;\n"
        "int b;" 
        "a = 8;\n"
        "b = 64 / a / 2;"
        "prntint a;"
        "prntint b;"
    );
    UA_genTal(tree, fopen("bin/out.tal", "w"));

    /* clean up */
    UP_freeTree(tree);
    return 0;
}