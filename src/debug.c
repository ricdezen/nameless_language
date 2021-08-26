#include <stdio.h>

#include "debug.h"

/**
 * Helper function to print an instruction that only has a name and no parameters.
 */
static int simpleInstruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

void disassembleChunk(Chunk *chunk, const char *name) {
    printf("== %s == \n", name);

    // Print all instructions one at a time.
    for (int offset = 0; offset < chunk->size;)
        offset = disassembleInstruction(chunk, offset);
}

int disassembleInstruction(Chunk *chunk, int offset) {
    printf("%08x ", offset);

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}