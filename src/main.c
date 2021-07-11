#include "uparse.h"
#include "uasm.h"

int main() {
    UASTRootNode *tree = UP_parseSource(
        "int a;\n"
        "int b;" 
        "a = 8;\n"
        "b = 64 / a / 2;"
        "prntint a;"
        "prntint b;"
    );
    UA_genTal(tree, fopen("bin/out.tal", "w"));

    /* clean up */
    UP_freeTree((UASTNode*)tree);
    return 0;
}