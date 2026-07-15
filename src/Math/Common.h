#pragma once

#include <numbers>

namespace math {

	inline constexpr float PI = std::numbers::pi_v<float>;
	inline constexpr float HALF_PI = std::numbers::pi_v<float> / 2.0f;
	inline constexpr float TWO_PI = std::numbers::pi_v<float> * 2.0f;
	inline constexpr float EPSILON = 1e-5f;

	inline float radians(float degrees) {
		return degrees * PI /180.f;
	}
} // namespace math
