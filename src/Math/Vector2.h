#pragma once

#include <cmath>
#include <algorithm>

#include "Assertion.h"

#include "Common.h"

namespace math {

	// This Vector implementation uses row convention: The vector is a 1x3 matrix
	struct Vector2 {
		union {
			struct {
				float x, y;
			};

			float coords[2]{ 0, 0 };
		};

		static const Vector2 Right;
		static const Vector2 Left;
		static const Vector2 Up;
		static const Vector2 Down;
		static const Vector2 Zero;
		static const Vector2 One;

		constexpr Vector2() noexcept : x(), y() {}
		explicit constexpr Vector2(float uniform) noexcept : x(uniform), y(uniform) {}
		constexpr Vector2(float x, float y) noexcept : x(x), y(y) {}

		[[nodiscard]] constexpr float lengthSquared() const noexcept;
		[[nodiscard]] float length() const noexcept;
		void normalized() noexcept;
		[[nodiscard]] bool isNormalized() const noexcept;

		[[nodiscard]] constexpr bool operator==(const Vector2& other) const noexcept;
		[[nodiscard]] constexpr bool operator!=(const Vector2& other) const noexcept;
		[[nodiscard]] constexpr Vector2 operator+(const Vector2& other) const noexcept;
		constexpr Vector2& operator+=(const Vector2& other) noexcept;
		[[nodiscard]] constexpr Vector2 operator-(const Vector2& other) const noexcept;
		constexpr Vector2& operator-=(const Vector2& other) noexcept;
		[[nodiscard]] constexpr Vector2 operator-() noexcept;
		[[nodiscard]] constexpr Vector2 operator*(float scale) const noexcept;
		constexpr Vector2& operator*=(float scale) noexcept;
		[[nodiscard]] constexpr Vector2 operator/(float scale) const noexcept;
		constexpr Vector2& operator/=(float scale) noexcept;
		[[nodiscard]] constexpr float& operator[](int index) noexcept;
		[[nodiscard]] constexpr const float& operator[](int index) const noexcept;
	};

	inline constexpr Vector2 Vector2::Right{ (1.F), (0.F) };
	inline constexpr Vector2 Vector2::Left{ (-1.F), (0.F) };
	inline constexpr Vector2 Vector2::Up{ (0.F), (1.F) };
	inline constexpr Vector2 Vector2::Down{ (0.F), (-1.F) };
	inline constexpr Vector2 Vector2::Zero{ (0.F) };
	inline constexpr Vector2 Vector2::One{ (1.F) };

	inline constexpr float Vector2::lengthSquared() const noexcept {
		return x * x + y * y;
	}

	inline float Vector2::length() const noexcept {
		return sqrt(lengthSquared());
	}

	inline void Vector2::normalized() noexcept {
		float squared = lengthSquared();
		if (squared == 0)
			return;
		float length = sqrt(squared);
		x /= length;
		y /= length;
	}

	inline bool Vector2::isNormalized() const noexcept {
		float result = abs(lengthSquared() - 1.F);
		return result < EPSILON;
	}

	inline constexpr bool Vector2::operator==(const Vector2& other) const noexcept {
		return x == other.x && y == other.y;
	}

	inline constexpr bool Vector2::operator!=(const Vector2& other) const noexcept {
		return x != other.x || y != other.y;
	}

	inline constexpr Vector2 Vector2::operator+(const Vector2& other) const noexcept {
		return { x + other.x, y + other.y };
	}

	inline constexpr Vector2& Vector2::operator+=(const Vector2& other) noexcept {
		x += other.x;
		y += other.y;
		return *this;
	}

	inline constexpr Vector2 Vector2::operator-(const Vector2& other) const noexcept {
		return { x - other.x, y - other.y };
	}

	inline constexpr Vector2& Vector2::operator-=(const Vector2& other) noexcept {
		x -= other.x;
		y -= other.y;
		return *this;
	}

	inline constexpr Vector2 Vector2::operator-() noexcept {
		return { -x, -y };
	}

	inline constexpr Vector2 Vector2::operator*(float scale) const noexcept {
		return { x * scale, y * scale };
	}

	inline constexpr Vector2& Vector2::operator*=(float scale) noexcept {
		x *= scale;
		y *= scale;
		return *this;
	}

	inline constexpr Vector2 Vector2::operator/(float scale) const noexcept {
		return { x / scale, y / scale };
	}

	inline constexpr Vector2& Vector2::operator/=(float scale) noexcept {
		x /= scale;
		y /= scale;
		return *this;
	}

	inline constexpr float& Vector2::operator[](int index) noexcept {
		CBK_ASSERT(index == 0 || index == 1, "Trying to acces a Vector with invalid index!")
		return coords[index];
	}

	inline constexpr const float& Vector2::operator[](int index) const noexcept {
		CBK_ASSERT(index == 0 || index == 1, "Trying to acces a Vector with invalid index!")
		return coords[index];
	}

	inline constexpr float dot(const Vector2& a, const Vector2& b) noexcept {
		return a.x * b.x + a.y * b.y;
	}

	inline constexpr bool perpendicular(const Vector2& a, const Vector2& b) noexcept {
		return dot(a, b) == 0;
	}

	inline float angleBetween(const Vector2& a, const Vector2& b) noexcept {
		float denom = a.length() * b.length();
		if (denom == 0.F)
			return 0.F;
		float cosTheta = dot(a, b) / denom;
		cosTheta = std::clamp(cosTheta, -1.F, 1.F);
		return std::acos(cosTheta);
	}

} // namespace math
