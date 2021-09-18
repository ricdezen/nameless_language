#ifndef NAMELESS_CHUNK_H
#define NAMELESS_CHUNK_H

#include "common.h"
#include "value.h"

/**
 * Enum for instruction bytecodes.
 * It is supposed to remain below 256 values.
 */
typedef enum {
    OP_CONSTANT,        // Fetch a constant's value. One byte param: the index of the constant.
    OP_NIL,             // "nil" literal.
    OP_TRUE,            // "true" literal.
    OP_FALSE,           // "false" literal.
    OP_POP,             // Just pop a value. An example usage is in expression statements.
    // OP_DEFINE_LOCAL     Not used because when defining a local variable, it just becomes the last stack value during
    //                     compile time so no extra operation is needed during runtime.
    OP_GET_LOCAL,       // Get a local variable. Push onto the stack the local corresponding to the next byte.
    OP_SET_LOCAL,       // Set a local variable. Since it is an expression, it does not pop from the stack, looks only.
    OP_GET_GLOBAL,      // Get a global variable's name. Push the value of the variable to the stack.
    OP_DEFINE_GLOBAL,   // Define a global variable. This is a statement therefore pops the value from the stack.
    OP_SET_GLOBAL,      // Set a global variable. Since it is an expression, it does not pop from the stack, looks only.
    OP_GET_UPVALUE,     // Get an up-value's value. Push the value onto the stack.
    OP_SET_UPVALUE,     // Set an up-value's value. It is an expression, it does not pop from the stack.
    OP_GET_PROPERTY,    // Get an object's property. Takes field name operand. Pops an object from the stack and pushes the value.
    OP_SET_PROPERTY,    // Set an object's property. Takes field name operand. Pops object and value, assigns, then pushes the value.
    OP_GET_SUPER,       // Get a superclass' method. Takes field name operand. Pops class from stack.
    OP_EQUAL,           // (==) Pops the last two values and returns whether they are equal.
    OP_GREATER,         // (>) Pops the last two values a and b and returns whether a > b (boolean).
    OP_LESS,            // (<) Pops the last two values a and b and returns whether a < b (boolean).
    OP_ADD,             // (+) Pops the last two values from the stack and pushes the result.
    OP_SUBTRACT,        // (-) Pops the last two values from the stack and pushes the result.
    OP_MULTIPLY,        // (*) Pops the last two values from the stack and pushes the result.
    OP_DIVIDE,          // (/) Pops the last two values from the stack and pushes the result.
    OP_NOT,             // (!) Unary Not. Pops the last value from the stack, negates it, pushes the result.
    OP_NEGATE,          // Replace the value at the top of the stack with its negation.
    OP_PRINT,           // Print statement. Pop the last value and print it.
    OP_JUMP,            // Jump. Takes 2-byte operand. Used at the end of a then branch in an if.
    OP_JUMP_IF_FALSE,   // Jump if the last value on the stack is false. Takes 2-byte operand. Does not pop.
    OP_LOOP,            // Jump backwards. Takes 2-byte operand, which is how many bytes to jump backwards.
    OP_CALL,            // Call an object. Does not need to pop.
    OP_INVOKE,          // Invoke a method. Take method name operand and argument count operand.
    OP_SUPER_INVOKE,    // Invoke a method from the superclass. Take method name operand and argument count operand.
    OP_CLOSURE,         // Make a Closure. Capture the necessary upvalues.
    OP_CLOSE_UPVALUE,   // Close over an upvalue instead of only popping it.
    OP_CLASS,           // Declare a class. Next operand is the class's name.
    OP_INHERIT,         // Take last class and add all methods of second to last class to it, then pop the subclass.
    OP_METHOD,          // Declare a method. Pop last value and put it in the second to last value's methods table.
    OP_RETURN,          // Pop the value at the top of the stack.
} OpCode;

extern char *opCodeNames[];

/**
 * Generic byte dynamic array. Generally used to contain the bytecode.
 */
typedef struct {
    int size;               // Number of elements currently stored in the chunk.
    int capacity;           // Maximum number of elements that can be stored (length of array `code` points to).
    uint8_t *code;          // Pointer to dynamic array of bytes.
    int *lines;             // Array with the code lines for each byte of code.
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
