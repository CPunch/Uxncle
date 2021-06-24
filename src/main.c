#include "uparse.h"
#include "uasm.h"

int main() {
    UASTNode *tree = UP_parseSource("6 + 2 * 21 + 3 * 6");
    UA_genTal(tree, fopen("out.tal", "w"));

    return 0;
}