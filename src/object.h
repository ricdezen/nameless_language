#ifndef NAMELESS_OBJECT_H
#define NAMELESS_OBJECT_H

#include "common.h"
#include "value.h"

/**
 * Get an object's type. Check it actually is an Object, you forgetful buffoon.
 */
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

/**
 * Check if the Value is a string object.
 */
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

/**
 * Should I repeat myself again about the shadow realm and whatnot?
 */
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_C_STRING(value)      (((ObjString*)AS_OBJ(value))->chars)

/**
 * First order object types.
 */
typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj *next;
};

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