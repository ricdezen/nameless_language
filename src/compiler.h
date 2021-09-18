#ifndef NAMELESS_COMPILER_H
#define NAMELESS_COMPILER_H

#include "vm.h"
#include "object.h"

/**
 * Compile source into an executable Function.
 *
 * @param source The source to compile.
 * @return The compiled function.
 */
ObjFunction *compile(const char *source);

/**
 * Mark the Values that are needed by the compiler.
 */
void markCompilerRoots();

#endif
