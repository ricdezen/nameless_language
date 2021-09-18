#ifndef NAMELESS_VM_H
#define NAMELESS_VM_H

#include "chunk.h"
#include "value.h"
#include "table.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

/**
 * Representation of a single function call.
 */
typedef struct {
    ObjClosure *closure;    // The function (closure) being called.
    uint8_t *ip;            // Return address. Jump to here when the call ends.
    Value *slots;           // Pointer to the first slot of the stack that this function owns.
} CallFrame;

/**
 * Representation of the virtual machine.
 */
typedef struct {
    CallFrame frames[FRAMES_MAX];   // Static array of CallFrames. 64 maximum recursion levels.
    int frameCount;
    Value stack[STACK_MAX];         // Value stack.
    Value *stackTop;                // Pointer to stack top.
    Table globals;                  // Global variables. String names as keys, values as values.
    Table strings;                  // Hashtable containing strings, for interning.
    ObjUpvalue *openUpvalues;       // List of upvalues. Must be kept sorted by stack slot index.
    Obj *objects;                   // As a temporary solution, a linked list of objects.

    // Temporary solution: unmanaged list of objects for garbage collection.
    size_t bytesAllocated;
    size_t nextGC;
    int grayCount;
    int grayCapacity;
    Obj **grayStack;
} VM;

extern VM vm;

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
