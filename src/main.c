#include "uparse.h"
#include "uasm.h"

int main() {
    UASTRootNode *tree = UP_parseSource(
        "prntint 0xFF;"
    );
    UA_genTal(tree, fopen("bin/out.tal", "w"));

    /* clean up */
    UP_freeTree((UASTNode*)tree);
    return 0;
}