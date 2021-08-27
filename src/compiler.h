#ifndef NAMELESS_COMPILER_H
#define NAMELESS_COMPILER_H

#include "vm.h"
#include "object.h"

bool compile(const char* source, Chunk* chunk);

#endif
