#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "vm.h"

/**
 * Helper function to print an instruction that only has a name and no parameters.
 */
static int simpleInstruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

/**
 * Similar to constantInstruction but does not need the constant's value.
 */
static int byteInstruction(const char *name, Chunk *chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

/**
 * Prints where the jump instruction leads.
 */
static int jumpInstruction(const char *name, int sign, Chunk *chunk, int offset) {
    uint16_t jump = (uint16_t) (chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset,
           offset + 3 + sign * jump);
    return offset + 3;
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

/**
 * Print invocation instruction.
 */
static int invokeInstruction(const char *name, Chunk *chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("%-16s (%d args) %4d '", name, argCount, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
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
    char *name = opCodeNames[instruction];
    switch (instruction) {
        case OP_CONSTANT:
        case OP_GET_GLOBAL:
        case OP_DEFINE_GLOBAL:
        case OP_SET_GLOBAL:
        case OP_GET_PROPERTY:
        case OP_SET_PROPERTY:
        case OP_GET_SUPER:
        case OP_CLASS:
        case OP_METHOD:
            return constantInstruction(name, chunk, offset);
        case OP_NIL:
        case OP_TRUE:
        case OP_FALSE:
        case OP_POP:
        case OP_EQUAL:
        case OP_GREATER:
        case OP_LESS:
        case OP_ADD:
        case OP_SUBTRACT:
        case OP_MULTIPLY:
        case OP_DIVIDE:
        case OP_NOT:
        case OP_NEGATE:
        case OP_PRINT:
        case OP_CLOSE_UPVALUE:
        case OP_INHERIT:
        case OP_RETURN:
            return simpleInstruction(name, offset);
        case OP_GET_LOCAL:
        case OP_SET_LOCAL:
        case OP_GET_UPVALUE:
        case OP_SET_UPVALUE:
        case OP_CALL:
            return byteInstruction(name, chunk, offset);
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
            return jumpInstruction(name, 1, chunk, offset);
        case OP_LOOP:
            return jumpInstruction(name, -1, chunk, offset);
        case OP_INVOKE:
        case OP_SUPER_INVOKE:
            return invokeInstruction(name, chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant);
            printValue(chunk->constants.values[constant]);
            printf("\n");

            ObjFunction *function = AS_FUNCTION(chunk->constants.values[constant]);
            for (int j = 0; j < function->upvalueCount; j++) {
                int isLocal = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%08d      |                     %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
            }

            return offset;
        }
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

void printStack() {
    // Don't print empty stack.
    if (vm.stack >= vm.stackTop)
        return;
    // Print stack content.
    printf("          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
        printf("[ ");
        printValue(*slot);
        printf(" ]");
    }
    printf("\n");
}