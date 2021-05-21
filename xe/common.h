#pragma once
#include "xe/types.h"

#define null nullptr

#define xe_offsetof(struct, field) ((uintptr_t)(&((struct*)0) -> field))
#define xe_containerof(struct, field, addr) ((struct*)((uintptr_t)(addr) - xe_offsetof(struct, field)))