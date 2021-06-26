#ifndef UASM_H
#define UASM_H

#include "uxncle.h"
#include "uparse.h"

/* default heap space to hold temporary values */
#define HEAP_SPACE 0x1800

#include <stdio.h>

/* takes a syntax tree and spits out the generated asm into the provided file stream */
void UA_genTal(UASTNode *tree, FILE *out);

#endif