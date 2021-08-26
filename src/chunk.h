#ifndef NAMELESS_CHUNK_H
#define NAMELESS_CHUNK_H

#include "common.h"
#include "value.h"

/**
 * Enum for instruction bytecodes.
 * Is supposed to remain below 256 values.
 */
typedef enum {
    OP_CONSTANT,    // Fetch a constant's value. One byte param: the index of the constant.
    OP_RETURN,
} OpCode;

/**
 * Generic byte dynamic array. Generally used to contain the bytecode.
 */
typedef struct {
    int size;               // Number of elements currently stored in the chunk.
    int capacity;           // Maximum number of elements that can be stored (length of array `code` points to).
    uint8_t *code;          // Pointer to dynamic array of bytes.
    int*lines;              // Array with the code lines for each byte of code.
    ValueArray constants;   // Array of constants used in the chunk (numbers etc.).
} Chunk;

/**
 * Initialize a Chunk for usage with other functions.
 *
 * @param chunk Pointer to the chunk to initialize.
 */
void initChunk(Chunk *chunk);

/**
 * Dispose of a chunk.
 *
 * @param chunk The chunk to free.
 */
void freeChunk(Chunk *chunk);

/**
 * Append a byte at the end of a chunk, resizing its capacity if necessary.
 *
 * @param chunk The target chunk.
 * @param byte The byte to write.
 * @param line The line in the source code.
 */
void writeChunk(Chunk *chunk, uint8_t byte, int line);

/**
 * Add a constant to a chunk. Why not a macro? Because this way we also return the index where the constant is.
 *
 * @param chunk The target chunk.
 * @param value The value to add.
 * @return The offset at which the constant was output.
 */
int addConstant(Chunk *chunk, Value value);

#endif
