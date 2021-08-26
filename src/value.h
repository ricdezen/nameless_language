#ifndef NAMELESS_VALUE_H
#define NAMELESS_VALUE_H

#include "common.h"

/**
 * Type corresponding to a Value. Temporary it is just a double.
 */
typedef double Value;

/**
 * Just like a Chunk. C has no generics. Shame.
 * Used to keep a list of constants that a chunk of code has.
 */
typedef struct {
    int capacity;
    int size;
    Value *values;
} ValueArray;

/**
 * Initialize a Value array for later use.
 *
 * @param array Pointer to the array to initialize.
 */
void initValueArray(ValueArray *array);

/**
 * Append a value to the list.
 *
 * @param array The target array.
 * @param value The value to append.
 */
void writeValueArray(ValueArray *array, Value value);

/**
 * Free an array.
 *
 * @param array The array to free.
 */
void freeValueArray(ValueArray *array);

/**
 * Print a value.
 *
 * @param value The value to print.
 */
void printValue(Value value);

#endif
