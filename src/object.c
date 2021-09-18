#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
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
    object->isMarked = false;
    // Add object at the top of the linked list of the VM.
    object->next = vm.objects;
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void *) object, size, type);
#endif

    return object;
}

ObjBoundMethod *newBoundMethod(Value receiver, ObjClosure *method) {
    ObjBoundMethod *bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjClass *newClass(ObjString *name) {
    ObjClass *klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name = name;
    initTable(&klass->methods);
    return klass;
}

ObjClosure *newClosure(ObjFunction *function) {
    ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjFunction *newFunction() {
    ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjInstance *newInstance(ObjClass *klass) {
    ObjInstance *instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

ObjNative *newNative(NativeFn function) {
    ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

/**
 * Allocate a String object.
 *
 * @param chars Pointer to char array.
 * @param length Length of the string.
 * @return The ObjString.
 */
static ObjString *allocateString(char *chars, int length, uint32_t hash) {
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    // Intern the string.
    push(OBJ_VAL(string));
    tableSet(&vm.strings, string, NIL_VAL);
    pop();
    return string;
}

/**
 * Compute a string's hash.
 *
 * @param key The string to hash.
 * @param length The length of the string.
 * @return The hashcode resulting from FNV-1a hash function.
 */
static uint32_t hashString(const char *key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t) key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjString *copyString(const char *chars, int length) {
    uint32_t hash = hashString(chars, length);

    // If the string is already there, return that instead.
    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL)
        return interned;

    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

ObjUpvalue *newUpvalue(Value *slot) {
    ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

static void printFunction(ObjFunction *function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<function %s>", function->name->chars);
}

ObjString *takeString(char *chars, int length) {
    uint32_t hash = hashString(chars, length);

    // If the string is already there, return that instead.
    ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        // We specified this function takes ownership of the char array
        // -> It is up to us to free it since we do not need it.
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD:
            printFunction(AS_BOUND_METHOD(value)->method->function);
            break;
        case OBJ_CLASS:
            printf("<class '%s'>", AS_CLASS(value)->name->chars);
            break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            printf("<'%s' object>", AS_INSTANCE(value)->klass->name->chars);
            break;
        case OBJ_NATIVE:
            printf("<native @ %p>", AS_OBJ(value));
            break;
        case OBJ_STRING:
            printf("%s", AS_C_STRING(value));
            break;
        case OBJ_UPVALUE:
            printf("<upvalue>");
            break;
    }
}

