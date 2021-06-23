#ifndef UMEM_H
#define UMEM_H

#include "uxncle.h"

#define GROW_FACTOR 2

#define UM_realloc(buf, size) \
    realloc(buf, size);

#define UM_freearray(buf) \
    UM_realloc(buf, NULL);

#define UM_free(buf) \
    UM_realloc(buf, NULL);

#define UM_growarray(type, buf, count, capacity) \
    if (count >= capacity || buf == NULL) { \
        int old = capacity; \
        capacity = old * GROW_FACTOR; \
        buf = (type*)UM_realloc(buf, sizeof(type) * capacity); \
    }

#endif