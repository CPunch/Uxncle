#include "uparse.h"
#include "uasm.h"

int main() {
    UASTNode *tree = UP_parseSource("prntint 6 + 2 * 21 + 3 * 6; prntint 6 / 2; prntint 24");
    UA_genTal(tree, fopen("bin/out.tal", "w"));

    /* clean up */
    UP_freeTree(tree);
    return 0;
}