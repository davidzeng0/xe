#pragma once

template<typename T, typename U>
static constexpr inline bool xe_overflow_add(T& a, U b){
	return __builtin_add_overflow(a, b, &a);
}

template<typename T, typename U>
static constexpr inline bool xe_overflow_add(T& c, U a, U b){
	return __builtin_add_overflow(a, b, &c);
}

template<typename T, typename U>
static constexpr inline bool xe_overflow_sub(T& a, U b){
	return __builtin_sub_overflow(a, b, &a);
}

template<typename T, typename U>
static constexpr inline bool xe_overflow_sub(T& c, U a, U b){
	return __builtin_sub_overflow(a, b, &c);
}

template<typename T, typename U>
static constexpr inline bool xe_overflow_mul(T& a, U b){
	return __builtin_mul_overflow(a, b, &a);
}

template<typename T, typename U>
static constexpr inline bool xe_overflow_mul(T& c, U a, U b){
	return __builtin_mul_overflow(a, b, &c);
}