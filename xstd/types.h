#pragma once
#include <sys/types.h>
#include <stddef.h>
#include <limits.h>
#include <stdint.h>

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;

typedef unsigned char byte;
typedef const char* xe_cstr;
typedef void* xe_ptr;
typedef const void* xe_cptr;

static constexpr nullptr_t null = nullptr;