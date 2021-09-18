#include <stdlib.h>
#include <stdio.h>

#include "vm.h"
#include "chunk.h"
#include "memory.h"

void initChunk(Chunk *chunk) {
    chunk->size = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk *chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk); // Reset chunk to initial state.
}

void writeChunk(Chunk *chunk, uint8_t byte, int line) {
    if (chunk->size + 1 > chunk->capacity) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->size] = byte;
    chunk->lines[chunk->size] = line;
    chunk->size++;
}

int addConstant(Chunk *chunk, Value value) {
    push(value);
    writeValueArray(&chunk->constants, value);
    pop();
    return chunk->constants.size - 1;
}