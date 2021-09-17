#ifndef NAMELESS_VALUE_H
#define NAMELESS_VALUE_H

#include "common.h"

// Just typedef the struct for ease of use.
typedef struct Obj Obj;
typedef struct ObjString ObjString;

/**
 * Type of Value.
 */
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ
} ValueType;

/**
 * A value is an union, it can either be a boolean, a number or object.
 */
typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj *obj;
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
 * Create Value from an object.
 */
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)(object)}})

/**
 * Cast Value to bool. If you do this unsafely, you may open a portal to the shadow realm. Check the type before.
 */
#define AS_BOOL(value)    ((value).as.boolean)

/**
 * Cast Value to number. If you do this unsafely, you may end up on the naughty list. Check the type before.
 */
#define AS_NUMBER(value)  ((value).as.number)

/**
 * Cast Value to Object. If you do this unsafely, thou shalt be damned. Check the type before.
 */
#define AS_OBJ(value)     ((value).as.obj)

/**
 * Macros for Value type checking. In case you have not read so yet, If you do not use these before a AS_X call,
 * you risk some The end of Evangelion level mumbo jumbo. You can obviously check the Value's type directly, with an if
 * or switch, whatever does it for you.
 */
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

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
