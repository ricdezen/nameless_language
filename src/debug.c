#include <stdio.h>

#include "debug.h"
#include "value.h"

/**
 * Helper function to print an instruction that only has a name and no parameters.
 */
static int simpleInstruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

/**
 * Print constant instruction.
 */
static int constantInstruction(const char *name, Chunk *chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    // Skip two bytes cause need to skip operand.
    return offset + 2;
}

void disassembleChunk(Chunk *chunk, const char *name) {
    printf("== %s == \n", name);

    // Print all instructions one at a time.
    for (int offset = 0; offset < chunk->size;)
        offset = disassembleInstruction(chunk, offset);
}

int disassembleInstruction(Chunk *chunk, int offset) {
    printf("%08x ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
        // Still at the same line as previous instruction.
        printf("   | ");
    else
        // First line or new line.
        printf("%4d ", chunk->lines[offset]);

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}