#pragma once
#include <sys/types.h>
#include <cstddef>
#include <climits>
#include <cstdint>

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;

typedef unsigned char byte;
typedef const char* xe_cstr;
typedef void* xe_ptr;
typedef const void* xe_cptr;

static constexpr std::nullptr_t null = nullptr;