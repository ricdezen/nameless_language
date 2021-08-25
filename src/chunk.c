#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void initChunk(Chunk *chunk) {
    chunk->size = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
}

void writeChunk(Chunk *chunk, uint8_t byte) {
    if (chunk->size + 1 > chunk->capacity) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->size] = byte;
    chunk->size++;
}