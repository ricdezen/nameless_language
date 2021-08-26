#include <stdio.h>

#include "memory.h"
#include "value.h"

void initValueArray(ValueArray *array) {
    array->values = NULL;
    array->capacity = 0;
    array->size = 0;
}

void writeValueArray(ValueArray *array, Value value) {
    if (array->capacity < array->size + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values,
                                   oldCapacity, array->capacity);
    }

    array->values[array->size] = value;
    array->size++;
}

void freeValueArray(ValueArray *array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void printValue(Value value) {
    printf("%g", value);
}