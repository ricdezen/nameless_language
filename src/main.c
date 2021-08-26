#include <stdio.h>

#include "chunk.h"
#include "debug.h"

int main() {
    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, OP_RETURN);
    disassembleChunk(&chunk, "Hello World!");
    freeChunk(&chunk);
    return 0;
}
