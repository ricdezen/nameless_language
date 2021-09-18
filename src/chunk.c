#include "chunk.h"
#include "memory.h"
#include "vm.h"

char *opCodeNames[] = {
        [OP_CONSTANT]       = "OP_CONSTANT",
        [OP_NIL]            = "OP_NIL",
        [OP_TRUE]           = "OP_TRUE",
        [OP_FALSE]          = "OP_FALSE",
        [OP_POP]            = "OP_POP",
        [OP_GET_LOCAL]      = "OP_GET_LOCAL",
        [OP_SET_LOCAL]      = "OP_SET_LOCAL",
        [OP_GET_GLOBAL]     = "OP_GET_GLOBAL",
        [OP_DEFINE_GLOBAL]  = "OP_DEFINE_GLOBAL",
        [OP_SET_GLOBAL]     = "OP_SET_GLOBAL",
        [OP_GET_UPVALUE]    = "OP_GET_UPVALUE",
        [OP_SET_UPVALUE]    = "OP_SET_UPVALUE",
        [OP_GET_PROPERTY]   = "OP_GET_PROPERTY",
        [OP_SET_PROPERTY]   = "OP_SET_PROPERTY",
        [OP_GET_SUPER]      = "OP_GET_SUPER",
        [OP_EQUAL]          = "OP_EQUAL",
        [OP_GREATER]        = "OP_GREATER",
        [OP_LESS]           = "OP_LESS",
        [OP_ADD]            = "OP_ADD",
        [OP_SUBTRACT]       = "OP_SUBTRACT",
        [OP_MULTIPLY]       = "OP_MULTIPLY",
        [OP_DIVIDE]         = "OP_DIVIDE",
        [OP_NOT]            = "OP_NOT",
        [OP_NEGATE]         = "OP_NEGATE",
        [OP_PRINT]          = "OP_PRINT",
        [OP_JUMP]           = "OP_JUMP",
        [OP_JUMP_IF_FALSE]  = "OP_JUMP_IF_FALSE",
        [OP_LOOP]           = "OP_LOOP",
        [OP_CALL]           = "OP_CALL",
        [OP_INVOKE]         = "OP_INVOKE",
        [OP_SUPER_INVOKE]   = "OP_SUPER_INVOKE",
        [OP_CLOSURE]        = "OP_CLOSURE",
        [OP_CLOSE_UPVALUE]  = "OP_CLOSE_UPVALUE",
        [OP_CLASS]          = "OP_CLASS",
        [OP_INHERIT]        = "OP_INHERIT",
        [OP_METHOD]         = "OP_METHOD",
        [OP_RETURN]         = "OP_RETURN",
};

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