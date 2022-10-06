#pragma once
#include <type_traits>

template<typename T>
struct xe_traits{
	static constexpr bool trivially_constructible = std::is_trivially_constructible_v<T>;
	static constexpr bool trivially_destructible = std::is_trivially_destructible_v<T>;
	static constexpr bool trivially_copy_constructible = std::is_trivially_copy_constructible_v<T>;
	static constexpr bool trivially_move_constructible = std::is_trivially_move_constructible_v<T>;
	static constexpr bool trivially_copy_assignable = std::is_trivially_copy_assignable_v<T>;
	static constexpr bool trivially_move_assignable = std::is_trivially_move_assignable_v<T>;

	static constexpr bool trivially_movable = trivially_move_constructible && trivially_destructible;
	static constexpr bool trivially_reallocatable = trivially_move_constructible && trivially_constructible && trivially_destructible;
};