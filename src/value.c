#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"
#include "object.h"

void initValueArray(ValueArray *array) {
    array->values = NULL;
    array->capacity = 0;
    array->size = 0;
}

void writeValueArray(ValueArray *array, Value value) {
    if (array->capacity < array->size + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->size] = value;
    array->size++;
}

void freeValueArray(ValueArray *array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_OBJ:
            printObject(value);
            break;
    }
}

bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

bool valuesEqual(Value a, Value b) {
    // Can't compare values of different types.
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL:
            // Bool can be compared.
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:
            // nil type is always nil.
            return true;
        case VAL_NUMBER:
            // Number can be compared.
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:
            // Objects (including strings thanks to interning)
            // are only equal if their address is equal.
            // Remember `as.obj` in structure Value is a pointer.
            return AS_OBJ(a) == AS_OBJ(b);
        default:
            return false; // Unreachable.
    }
}
