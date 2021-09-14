#ifndef NAMELESS_OBJECT_H
#define NAMELESS_OBJECT_H

#include "common.h"
#include "value.h"
#include "chunk.h"

/**
 * Get an object's type. Check it actually is an Object, you forgetful buffoon.
 */
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

/**
 * Check if the Value is a Closure object.
 */
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)

/**
 * Check if the Value is a function object.
 */
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)

/**
 * Check if the Value is a native function object.
 */
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)

/**
 * Check if the Value is a string object.
 */
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

/**
 * Don't cast without checking etc.
 */
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))

/**
 * Don't cast without checking etc.
 */
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))

/**
 * Don't cast without checking etc.
 */
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)

/**
 * Should I repeat myself again about the shadow realm and whatnot?
 */
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_C_STRING(value)      (((ObjString*)AS_OBJ(value))->chars)

/**
 * First order object types.
 */
typedef enum {
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
} ObjType;

/**
 * Structure representing an Object.
 */
struct Obj {
    ObjType type;
    struct Obj *next;
};

/**
 * Structure for a function object.
 */
typedef struct {
    Obj obj;            // The base Object structure.
    int arity;          // How many parameters the function takes.
    int upvalueCount;   // How many up-values the function references.
    Chunk chunk;        // The code of the function.
    ObjString *name;    // The string name of the function.
} ObjFunction;

/**
 * C function to be called from a binding. Takes a pointer to its first argument and how many arguments were passed.
 */
typedef Value (*NativeFn)(int argCount, Value *args);

/**
 * Structure for a native function.
 */
typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

/**
 * Notice the Obj field is first. This is fundamental to allow down-casting from an Obj structure that we assert
 * contains a string, to an ObjString structure.
 */
struct ObjString {
    Obj obj;
    uint32_t hash;
    int length;
    char *chars;
};

typedef struct {
    Obj obj;
    ObjFunction *function;
} ObjClosure;

/**
 * Allocate a new Closure Object.
 *
 * @param function The function object to enclose.
 * @return A Closure object.
 */
ObjClosure *newClosure(ObjFunction *function);

/**
 * Allocate a new function Object.
 *
 * @return An allocated ObjFunction.
 */
ObjFunction *newFunction();

/**
 * Allocate a new native function binding.
 *
 * @param function The function to bind.
 * @return The ObjNative object.
 */
ObjNative *newNative(NativeFn function);

/**
 * Take a string and put it in an object after copying it.
 *
 * @param chars The character array this string comes from.
 * @param length The length of the string. Can't be inferred because we do not know where the string comes from.
 * @return An object containing the string.
 */
ObjString *copyString(const char *chars, int length);

/**
 * Like `copyString` but takes ownership of the given string.
 *
 * @param chars The character array this string comes from.
 * @param length The length of the string. Can't be inferred because we do not know where the string comes from.
 * @return An object containing the string.
 */
ObjString *takeString(char *chars, int length);

/**
 * Print an Object.
 *
 * @param value A Value which must be an Object.
 */
void printObject(Value value);

/**
 * Check an object's type. Why don't we add another couple of macros like we did for built in values? Because here one
 * of the arguments is used twice. If we do: `SOME_MACRO(foo())` and `SOME_MACRO` uses the argument twice, it calls
 * `foo()` twice. On the other hand we add the `inline` keyword to tell the compiler it may paste the body of this
 * function (after evaluating its parameters) if it deems it profitable.
 *
 * @param value A Value.
 * @param type The desired type.
 * @return Whether the value is in fact of the given type.
 */
static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif