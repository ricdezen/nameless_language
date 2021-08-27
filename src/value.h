#ifndef NAMELESS_VALUE_H
#define NAMELESS_VALUE_H

#include "common.h"

/**
 * Type of Value.
 */
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
} ValueType;

/**
 * A value is an union, it can either be a boolean, a number or object.
 */
typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
    } as;
} Value;

/**
 * Make a NULL (nil) value.
 */
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})

/**
 * Create a Value from a boolean.
 */
#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = (value)}})

/**
 * Create a Value from a number.
 */
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = (value)}})

/**
 * Cast value to bool. If you do this unsafely, you may open a portal to the shadow realm. Check the type with the
 * other macros.
 */
#define AS_BOOL(value)    ((value).as.boolean)

/**
 * Cast value to number. If you do this unsafely, you may end up on the naughty list. Check the type with the other
 * macros.
 */
#define AS_NUMBER(value)  ((value).as.number)

/**
 * Three macros for Value type checking. In case you have not read so yet, If you do not use these before a AS_X call,
 * you risk some The end of Evangelion level mumbo jumbo. You can obviously check the Value's type directly, with an if
 * or switch, whatever does it for you.
 */
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)

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

/**
 * @param value Any value.
 * @return true if the value is nil or logical false. Everything else is true.
 */
bool isFalsey(Value value);

/**
 * @param a Any value.
 * @param b Any other value.
 * @return Return whether the two values are equal.
 */
bool valuesEqual(Value a, Value b);

#endif
