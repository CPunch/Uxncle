#include "uparse.h"
#include "uasm.h"

int main() {
    UASTNode *tree = UP_parseSource("2 * 4 + 6");
    UA_genTal(tree, fopen("out.tal", "w"));

    return 0;
}