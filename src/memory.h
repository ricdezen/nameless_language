#ifndef NAMELESS_MEMORY_H
#define NAMELESS_MEMORY_H

#include "common.h"
#include "object.h"

/**
 * Macro to allocate memory for an array of values (and cast the pointer to the desired type).
 *
 * @param type The type to cast the pointer to.
 * @param count How many values.
 */
#define ALLOCATE(type, count) (type*)reallocate(NULL, 0, sizeof(type) * (count))

/**
 * Same as `ALLOCATE` but this memory is not managed by garbage collection, and must be freed manually. Calls `realloc`
 * from the standard library, meaning it can also be used in place of `GROW_ARRAY`.
 * It is heavily advised to only use this for the dynamic array of gray objects during garbage collection.
 */
#define ALLOCATE_UNMANAGED(type, count) (type*)realloc(NULL, sizeof(type) * (count))

/**
 * Free a pointer.
 *
 * @param type The type of the object to free.
 * @param pointer Pointer to the object.
 */
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

/**
 * Free a pointer to unmanaged memory.
 */
#define FREE_UNMANAGED(ptr) free(ptr)

/**
 * Macro aimed to increase an array's size.
 * Arrays start at a minimum size of 8 and then it gets doubled every time.
 *
 * @param old The old size.
 */
#define GROW_CAPACITY(old) ((old) < 8 ? 8 : (old) * 2)

/**
 * Reallocate a dynamic array.
 *
 * @param type The type of data. Used to get the size of the array.
 * @param pointer The pointer to the memory area.
 * @param oldCount The number of elements previously contained in the array.
 * @param newCount The desired amount of elements for the array.
 */
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

/**
 * Free a dynamic array.
 *
 * @param type The type of data. Used to get the size of the array.
 * @param pointer The pointer to the memory area.
 * @param oldCount The previous array size.
 */
#define FREE_ARRAY(type, pointer, oldCount) reallocate(pointer, sizeof(type) * (oldCount), 0)


/**
 * Function to reallocate a dynamic memory block.
 * If newSize = 0, free the memory block.
 * Otherwise allocate (oldSize = 0) or reallocate a block of memory with size newSize.
 * Can trigger garbage collection.
 *
 * @param pointer Pointer to the memory area.
 * @param oldSize The size the memory block had. If 0, allocate a new block.
 * @param newSize The size the memory block should have. If 0, free the memory block.
 * @return The pointer to the new allocated memory area.
 */
void *reallocate(void *pointer, size_t oldSize, size_t newSize);

/**
 * Mark an Object as reachable during garbage collection. Adds it to gray objects.
 *
 * @param object The Object to mark.
 */
void markObject(Obj *object);

/**
 * Mark a Value as reachable during garbage collection. Ignores Values corresponding to Numbers, Booleans and Nil, since
 * they are not allocated on the heap. An Object will be added to the "gray" objects when it is marked. It's basically
 * the frontier of the object graph exploration.
 *
 * @param value The value to mark.
 */
void markValue(Value value);

/**
 * Start garbage collection. The algorithm is a simple mark and sweep. Visits all the objects and remove the ones that
 * are not reachable from the roots.
 */
void collectGarbage();

/**
 * Free the linked list of objects of the vm.
 */
void freeObjects();

#endif
