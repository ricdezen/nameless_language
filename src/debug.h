#ifndef NAMELESS_DEBUG_H
#define NAMELESS_DEBUG_H

#include "chunk.h"

/**
 * Print chunk in a human readable format.
 *
 * @param chunk The chunk.
 * @param name The name of the chunk (e.g. function name).
 */
void disassembleChunk(Chunk *chunk, const char *name);

/**
 * Print an instruction in a human-readable format.
 *
 * @param chunk The chunk.
 * @param offset The offset of the instruction.
 * @return The next instruction.
 */
int disassembleInstruction(Chunk *chunk, int offset);

#endif