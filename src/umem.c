#include "umem.h"

void* UM_realloc(void *buf, size_t size) {
    void *newBuf;

    /* if the size is 0, just free it :) */
    if (size == 0) {
        free(buf);
        return NULL;
    }
    
    if (!(newBuf = realloc(buf, size))) {
        printf("Failed to reallocate memory!\n");
        exit(EXIT_FAILURE);
    }

    return newBuf;
}