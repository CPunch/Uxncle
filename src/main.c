#include "uparse.h"
#include "uasm.h"

int main() {
    UASTNode *tree = UP_parseSource(
        "short a = 2;\n" 
        "short b = 48 / 4;\n" 
        "prntint b / a;"
    );
    UA_genTal(tree, fopen("bin/out.tal", "w"));

    /* clean up */
    UP_freeTree(tree);
    return 0;
}