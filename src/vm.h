#ifndef NAMELESS_VM_H
#define NAMELESS_VM_H

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
    Chunk *chunk;           // The current code chunk.
    uint8_t *ip;            // Instruction pointer. Where the "processor" is at.
    Value stack[STACK_MAX]; // Value stack.
    Value *stackTop;        // Pointer to stack top.
} VM;

typedef enum {
    INTERPRET_OK,               // No errors.
    INTERPRET_COMPILE_ERROR,    // Compile error met.
    INTERPRET_RUNTIME_ERROR     // Runtime error met.
} InterpretResult;

/**
 * Initialize a virtual machine.
 */
void initVM();

/**
 * Free a virtual machine.
 */
void freeVM();

/**
 * Interpret source code from a character buffer.
 *
 * @param source Target source code.
 * @return The result.
 */
InterpretResult interpret(const char *source);

/**
 * Push a value at the top of a vm.
 *
 * @param value The value to push.
 */
void push(Value value);

/**
 * Pop a value from the top of a vm's stack.
 *
 * @return The popped value.
 */
Value pop();

#endif
