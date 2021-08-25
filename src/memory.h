#ifndef NAMELESS_MEMORY_H
#define NAMELESS_MEMORY_H

#include "common.h"

/**
 * Macro aimed to increase an array's size.
 * Arrays start at a minimum size of 8 and then it gets doubled every time.
 *
 * @param old The old size.
 */
#define GROW_CAPACITY(old) ((old) < 8 ? 8 : (old) * 2)

/**
 * @param type The type of data. Used to get the size of the array.
 * @param pointer The pointer to the memory area.
 * @param oldCount The number of elements previously contained in the array.
 * @param newCount The desired amount of elements for the array.
 */
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))


/**
 * Function to reallocate a dynamic memory block.
 * If newSize = 0, free the memory block.
 * Otherwise allocate (oldSize = 0) or reallocate a block of memory with size newSize.
 *
 * @param pointer Pointer to the memory area.
 * @param oldSize The size the memory block had. If 0, allocate a new block.
 * @param newSize The size the memory block should have. If 0, free the memory block.
 * @return The pointer to the new allocated memory area.
 */
void *reallocate(void *pointer, size_t oldSize, size_t newSize);

#endif
