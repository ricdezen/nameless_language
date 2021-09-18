#include <stdlib.h>

#include "memory.h"
#include "vm.h"
#include "compiler.h"

#ifdef DEBUG_LOG_GC

#include <stdio.h>
#include "debug.h"

#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
    vm.bytesAllocated += newSize - oldSize;

    // Run garbage collection if needed.
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif
        if (vm.bytesAllocated > vm.nextGC) {
            collectGarbage();
        }
    }

    // While `realloc` does allow this behaviour, it would make
    // it impossible for us to distinguish a freeing from running out of memory.
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, newSize);

    // If result is null we presumably ran out of memory.
    if (result == NULL)
        exit(1);

    return result;
}

void markObject(Obj *object) {
    // Avoid NULL and avoid black objects.
    if (object == NULL) return;
    if (object->isMarked) return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void *) object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    // The object is reachable.
    object->isMarked = true;

    // Resize gray stack if necessary.
    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = ALLOCATE_UNMANAGED(Obj*, vm.grayCapacity);
        // Allocation failure.
        if (vm.grayStack == NULL)
            exit(1);
    }

    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
    if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

/**
 * Mark all Values in a ValueArray.
 *
 * @param array A ValueArray.
 */
static void markArray(ValueArray *array) {
    for (int i = 0; i < array->size; i++) {
        markValue(array->values[i]);
    }
}

/**
 * Explore references of an Object.
 *
 * @param object The object.
 */
static void blackenObject(Obj *object) {

#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void *) object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod *bound = (ObjBoundMethod *) object;
            markValue(bound->receiver);
            markObject((Obj *) bound->method);
            break;
        }
        case OBJ_CLASS: {
            ObjClass *klass = (ObjClass *) object;
            markObject((Obj *) klass->name);
            markTable(&klass->methods);
            break;
        }
        case OBJ_CLOSURE: {
            // Wrapped function and upvalues.
            ObjClosure *closure = (ObjClosure *) object;
            markObject((Obj *) closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject((Obj *) closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            // Mark a function's constants and name.
            ObjFunction *function = (ObjFunction *) object;
            markObject((Obj *) function->name);
            markArray(&function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance *instance = (ObjInstance *) object;
            markObject((Obj *) instance->klass);
            markTable(&instance->fields);
            break;
        }
        case OBJ_UPVALUE:
            // This is safe: if the Value is not closed yet,
            // it is on the stack, and object->closed is NIL_VAL.
            markValue(((ObjUpvalue *) object)->closed);
            break;
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

static void freeObject(Obj *object) {

#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void *) object, object->type);
#endif

    switch (object->type) {
        case OBJ_BOUND_METHOD:
            FREE(ObjBoundMethod, object);
            break;
        case OBJ_CLASS: {
            ObjClass *klass = (ObjClass *) object;
            freeTable(&klass->methods);
            FREE(ObjClass, object);
            break;
        }
        case OBJ_CLOSURE: {
            // Clear closure and upvalues.
            ObjClosure *closure = (ObjClosure *) object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction *) object;
            freeChunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_STRING: {
            ObjString *string = (ObjString *) object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance *instance = (ObjInstance *) object;
            freeTable(&instance->fields);
            FREE(ObjInstance, object);
            break;
        }
        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;

    }
}

/**
 * Mark the roots as reachable.
 */
static void markRoots() {
    // Values on the stack.
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }

    // Closures on the call frame.
    for (int i = 0; i < vm.frameCount; i++) {
        markObject((Obj *) vm.frames[i].closure);
    }

    // Open upvalues are needed.
    for (ObjUpvalue *upvalue = vm.openUpvalues;
         upvalue != NULL;
         upvalue = upvalue->next) {
        markObject((Obj *) upvalue);
    }

    // Global variables.
    markTable(&vm.globals);

    // The compilers also take memory from the heap for literals.
    // Although it only needs to mark the function it is working on.
    markCompilerRoots();

    // "init" string.
    markObject((Obj *) vm.initString);
}

/**
 * Trace the references an object holds, such as its class and fields.
 */
static void traceReferences() {
    while (vm.grayCount > 0) {
        Obj *object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

/**
 * Loop through objects and free all non-reachable objects.
 * If the object is reachable clear its marked field for the next collection.
 */
static void sweep() {
    Obj *previous = NULL;
    Obj *object = vm.objects;
    while (object != NULL) {
        // Skip black objects.
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Obj *unreached = object;
            object = object->next;

            // If the non-reachable object was head of the list or in the middle.
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }

            freeObject(unreached);
        }
    }
}

void collectGarbage() {

#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
    printStack();
#endif

    markRoots();                    // Mark all roots.
    traceReferences();              // Mark reachable objects.
    tableRemoveWhite(&vm.strings);  // Remove unreachable strings.
    sweep();                        // Delete all non-reachable objects.

    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf(
            "   collected %zu bytes (from %zu to %zu) next at %zu\n",
            before - vm.bytesAllocated,
            before,
            vm.bytesAllocated,
            vm.nextGC
    );
#endif

}

void freeObjects() {
    // Live Objects
    Obj *object = vm.objects;
    while (object != NULL) {
        Obj *next = object->next;
        freeObject(object);
        object = next;
    }

    // Memory of the gray stack.
    FREE_UNMANAGED(vm.grayStack);
}