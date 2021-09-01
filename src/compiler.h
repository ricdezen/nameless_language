#ifndef NAMELESS_COMPILER_H
#define NAMELESS_COMPILER_H

#include "vm.h"
#include "object.h"

ObjFunction* compile(const char* source);

#endif
