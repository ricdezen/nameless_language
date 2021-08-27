#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        case OP_NIL:
            return simpleInstruction("OP_NIL", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

void printToken(Token *token) {
    // Allocate space needed for token's content.
    char *content = malloc(token->length + 1);
    // Copy the content of the token.
    memcpy(content, token->start, token->length);
    // Put terminator.
    content[token->length] = '\0';

    printf("TOKEN_");
    TokenType type = token->type;
    switch (type) {
        case TOKEN_NUMBER: {
            printf("NUMBER");
            break;
        }
        case TOKEN_STRING: {
            printf("STRING");
            break;
        }
        case TOKEN_IDENTIFIER: {
            printf("IDENTIFIER");
            break;
        }
        case TOKEN_ERROR: {
            printf("ERROR");
            break;
        }
        default: {
            printf("SYMBOL");
            break;
        }
    }
    printf(": %s\n", content);

    // Free buffer.
    free(content);
}