#pragma once

#define XE_SOURCE (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__), __LINE__