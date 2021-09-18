#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "compiler.h"
#include "object.h"
#include "memory.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

// Just a global member.
VM vm;

/**
 * Return the seconds from the epoch.
 */
static Value clockNative(int argCount, Value *args) {
    return NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);
}

/**
 * Helper function to reset a vm's stack.
 */
static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

/**
 * Print a runtime error. Basically printf with line information as an extra.
 *
 * @param format Format string.
 * @param ... The arguments.
 */
static void runtimeError(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // Print the stack trace.
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame *frame = &vm.frames[i];
        ObjFunction *function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

/**
 * Add a native function binding.
 *
 * @param name The name of the function in the global namespace.
 * @param function The C function containing the implementation.
 */
static void defineNative(const char *name, NativeFn function) {
    // We're putting the objects in the stack to avoid garbage collection.
    push(OBJ_VAL(copyString(name, (int) strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM() {
    resetStack();
    vm.objects = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings);
    vm.initString = NULL;   // Since we allocate it dynamically, the gc may read it before initializing it.
    vm.initString = copyString("init", 4);

    // GC stuff
    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;

    // Temporary solution to define natives.
    defineNative("clock", clockNative);
}

void freeVM() {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    vm.initString = NULL;
    freeObjects();
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

/**
 * Helper to peek at a value without popping it.
 * Should not ever be called with a distance > vm.stackTop - vm.stack.
 */
static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

/**
 * Perform a call. Remember argument 0 in the stack is reserved in each chunk of code.
 * Due to how our stack behaves, we already have the callee in the stack before the arguments, so we let the function's
 * stack point to that.
 *
 * @param function The function to call.
 * @param argCount How many arguments it has.
 * @return true.
 */
static bool call(ObjClosure *closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("You did it, my boy. You have finally become Stack Overflow.");
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frameCount++]; // Prepare the next frame.
    frame->closure = closure;                       // Set the function.
    frame->ip = closure->function->chunk.code;      // Set the instruction pointer.
    frame->slots = vm.stackTop - argCount - 1;      // Point at where the arguments begin (argument 0 is callee).
    return true;
}

/**
 * Attempt to call a value. Raise an error if it can't.
 *
 * @param callee The value to call.
 * @param argCount How many arguments is has on the stack.
 * @return the result of `call` if the callee could be called, false otherwise.
 */
static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod *bound = AS_BOUND_METHOD(callee);
                // Set this' value in slot 0 (relative to method's stack).
                vm.stackTop[-argCount - 1] = bound->receiver;
                return call(bound->method, argCount);
            }
            case OBJ_CLASS: {
                ObjClass *klass = AS_CLASS(callee);
                vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
                // Look for the initialized method. Run it if any.
                Value initializer;
                if (tableGet(&klass->methods, vm.initString, &initializer)) {
                    return call(AS_CLOSURE(initializer), argCount);
                } else if (argCount != 0) {
                    // No initializer method -> can't pass arguments.
                    runtimeError("Expected 0 arguments but got %d.", argCount);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
            default:
                break; // Non-callable object type.
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
}

/**
 * Call a method.
 *
 * @param klass The class.
 * @param name The method's name.
 * @param argCount The argument count.
 * @return the result of `call`.
 */
static bool invokeFromClass(ObjClass *klass, ObjString *name, int argCount) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    return call(AS_CLOSURE(method), argCount);
}

/**
 * Invoke a method.
 *
 * @param name The name of the method.
 * @param argCount The number of arguments.
 * @return the result of `invokeFromClass`.
 */
static bool invoke(ObjString *name, int argCount) {
    Value receiver = peek(argCount);

    // Binding does something similar.
    if (!IS_INSTANCE(receiver)) {
        runtimeError("Only instances have methods.");
        return false;
    }
    ObjInstance *instance = AS_INSTANCE(receiver);

    // If a field in the object has been overwritten, call that instead.
    Value value;
    if (tableGet(&instance->fields, name, &value)) {
        vm.stackTop[-argCount - 1] = value;
        return callValue(value, argCount);
    }

    // Or just call the method.
    return invokeFromClass(instance->klass, name, argCount);
}

/**
 * Bind a method to an object.
 *
 * @param klass The class of the object.
 * @param name The name of the method.
 * @return True if the method was found and pushed, False otherwise.
 */
static bool bindMethod(ObjClass *klass, ObjString *name) {
    Value method;
    // No method -> can't bind.
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }

    // Method found -> bind, pop object, push method.
    ObjBoundMethod *bound = newBoundMethod(peek(0), AS_CLOSURE(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}

/**
 * Initialize a new upvalue pointing to an existing runtime value.
 *
 * @param local The Value to point to.
 * @return An upvalue.
 */
static ObjUpvalue *captureUpvalue(Value *local) {
    ObjUpvalue *prevUpvalue = NULL;
    ObjUpvalue *upvalue = vm.openUpvalues;
    // Loop through the upvalues until the list is over
    // Or we found the position for the new upvalue.
    // > can be used because open upvalues are physically in the stack.
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    // Found the upvalue.
    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue *createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    // Created first upvalue.
    if (prevUpvalue == NULL)
        vm.openUpvalues = createdUpvalue;
    else
        prevUpvalue->next = createdUpvalue;

    return createdUpvalue;
}

/**
 * Close upvalues instead of only popping.
 *
 * @param last The last upvalue to close.
 */
static void closeUpvalues(Value *last) {
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue *upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

/**
 * Define a method in a class. Second to last value has to be a class.
 *
 * @param name The name of the method.
 */
static void defineMethod(ObjString *name) {
    Value method = peek(0);
    ObjClass *klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, name, method);
    pop();
}

/**
 * Concatenate the two last strings in the stack.
 */
static void concatenate() {
    // Instead of popping, we keep them on the stack to avoid garbage collection.
    ObjString *b = AS_STRING(peek(0));
    ObjString *a = AS_STRING(peek(1));

    int length = a->length + b->length;
    char *chars = ALLOCATE(
            char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    // Take result, pop operands, push result.
    ObjString *result = takeString(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

static InterpretResult run() {

    CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])

#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(valueType, op) { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) \
            { runtimeError("Operands must be numbers."); return INTERPRET_RUNTIME_ERROR; } \
        double b = AS_NUMBER(pop()); double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } // ! Careful with semicolons after this macro !

    for (;;) {

#ifdef DEBUG_TRACE_EXECUTION
        printStack();
        // Disassemble on the fly whilst debugging.
        disassembleInstruction(
                &frame->closure->function->chunk, (int) (frame->ip - frame->closure->function->chunk.code)
        );
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NIL:
                push(NIL_VAL);
                break;
            case OP_TRUE:
                push(BOOL_VAL(true));
                break;
            case OP_FALSE:
                push(BOOL_VAL(false));
                break;
            case OP_POP:
                pop();
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString *name = READ_STRING();
                Value value;
                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString *name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString *name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) {
                    // Set a global variable's value.
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                // Get an upvalue.
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                // Get an upvalue.
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(0))) {
                    runtimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance *instance = AS_INSTANCE(peek(0));
                ObjString *name = READ_STRING();

                Value value;
                if (tableGet(&instance->fields, name, &value)) {
                    pop(); // Instance.
                    push(value);
                    break;
                }

                // Try to bind field and raise error if it does not exist.
                if (!bindMethod(instance->klass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(1))) {
                    runtimeError("Only instances have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance *instance = AS_INSTANCE(peek(1));
                tableSet(&instance->fields, READ_STRING(), peek(0));
                Value value = pop();
                pop();
                push(value);
                break;
            }
            case OP_GET_SUPER: {
                ObjString *name = READ_STRING();
                ObjClass *superclass = AS_CLASS(pop());

                if (!bindMethod(superclass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >);
                break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <);
                break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtimeError(
                            "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -)
                break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *)
                break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /)
                break;
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                // We can NOT be sure there are exactly enough arguments after compilation.
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_INVOKE: {
                ObjString *method = READ_STRING();
                int argCount = READ_BYTE();
                if (!invoke(method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString *method = READ_STRING();
                int argCount = READ_BYTE();
                ObjClass *superclass = AS_CLASS(pop());
                if (!invokeFromClass(superclass, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                // Take last declared constant (function declaration).
                ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
                // Capture the local variables it referred to.
                ObjClosure *closure = newClosure(function);
                push(OBJ_VAL(closure));
                // Load the upvalues.
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE: {
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            }
            case OP_CLASS:
                push(OBJ_VAL(newClass(READ_STRING())));
                break;
            case OP_INHERIT: {
                Value superclass = peek(1);
                if (!IS_CLASS(superclass)) {
                    runtimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjClass *subclass = AS_CLASS(peek(0));
                tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
                pop(); // Subclass.
                break;
            }
            case OP_METHOD:
                defineMethod(READ_STRING());
                break;
            case OP_RETURN: {
                // Pop the result, the last value the function left on the stack is its return.
                Value result = pop();
                // Close the upvalues.
                closeUpvalues(frame->slots);
                // Drop the function frame.
                vm.frameCount--;
                // Last stack -> program is done.
                if (vm.frameCount == 0) {
                    // Pop reserved main stack slot.
                    pop();
                    return INTERPRET_OK;
                }
                // Push the result after coming back and removing the function call from the stack.
                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }

// Won't need the macros outside the function.
#undef BINARY_OP
#undef READ_STRING
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_BYTE

}

InterpretResult interpret(const char *source) {
    ObjFunction *function = compile(source);
    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    // The compiler still returns a function.
    // Wrap the function and replace it, then call it.
    push(OBJ_VAL(function));
    ObjClosure *closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}