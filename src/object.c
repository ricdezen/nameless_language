#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) (type*)allocateObject(sizeof(type), objectType)

/**
 * Allocate the memory for an object.
 *
 * @param size The size of the Object.
 * @param type The type of the Object.
 * @return The Object.
 */
static Obj *allocateObject(size_t size, ObjType type) {
    Obj *object = (Obj *) reallocate(NULL, 0, size);
    object->type = type;
    // Add object at the top of the linked list of the VM.
    object->next = vm.objects;
    vm.objects = object;

    return object;
}

/**
 * Allocate a String object.
 *
 * @param chars Pointer to char array.
 * @param length Length of the string.
 * @return The ObjString.
 */
static ObjString *allocateString(char *chars, int length) {
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjString *copyString(const char *chars, int length) {
    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length);
}

ObjString *takeString(char *chars, int length) {
    return allocateString(chars, length);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_C_STRING(value));
            break;
    }
}

