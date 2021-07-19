#ifndef UASM_H
#define UASM_H

#include "uxncle.h"
#include "uparse.h"

/* default heap space to hold temporary values */
#define HEAP_SPACE 0x1800

#define SIZE_INT    2
#define SIZE_CHAR   1
#define SIZE_BOOL   1

#include <stdio.h>

/* takes a syntax tree and spits out the generated asm into the provided file stream */
void UA_genTal(UASTRootNode *tree, FILE *out);

#endif