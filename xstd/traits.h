#include <type_traits>

template<typename T>
struct xe_traits{
	static constexpr bool trivially_constructible = std::is_trivially_constructible<T>::value;
	static constexpr bool trivially_destructible = std::is_trivially_destructible<T>::value;
	static constexpr bool trivially_copy_constructible = std::is_trivially_copy_constructible<T>::value;
	static constexpr bool trivially_move_constructible = std::is_trivially_move_constructible<T>::value;
	static constexpr bool trivially_copy_assignable = std::is_trivially_copy_assignable<T>::value;
	static constexpr bool trivially_move_assignable = std::is_trivially_move_assignable<T>::value;

	static constexpr bool trivially_permanent_move = trivially_move_constructible && trivially_destructible;
	static constexpr bool trivially_reallocatable = trivially_move_constructible && trivially_constructible && trivially_destructible;
};