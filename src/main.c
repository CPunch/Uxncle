#include "uparse.h"
#include "uasm.h"

char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    /* first, we need to know how big our file is */
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    /* allocate our buffer (+1 for NULL byte) */
    char *buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "failed to allocate!");
        exit(EXIT_FAILURE);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);

    if (bytesRead < fileSize) {
        printf("failed to read file \"%s\"!\n", path);
        exit(74);
    }

    /* place the null terminator to mark the end of the source */
    buffer[bytesRead] = '\0';

    /* close the file handler and return the source buffer */
    fclose(file);
    return buffer;
}

int main(int argc, const char *argv[]) {
    const char *out, *in;
    char *src;

    if (argc < 3) {
        printf("Usage: %s [SOURCE] [OUT]\nCompiler for the Uxntal assembly language.", argv[0]);
        exit(EXIT_FAILURE);
    }

    in = argv[1];
    out = argv[2];
    src = readFile(in);

    UASTRootNode *tree = UP_parseSource(src);
    UA_genTal(tree, fopen(out, "w"));

    /* clean up */
    UP_freeTree((UASTNode*)tree);
    free(src);

    printf("Compiled successfully! Wrote generated uxntal to %s\n", out);
    return 0;
}