#pragma once

#include "Vector2.h"

namespace math {

	// This Vector implementation uses row convention: The vector is a 1x3 matrix
	struct Vector3 {
		union {
			struct {
				float x, y, z;
			};

			float coords[3];
		};

		static const Vector3 Right;
		static const Vector3 Left;
		static const Vector3 Up;
		static const Vector3 Down;
		static const Vector3 Forward;
		static const Vector3 Backward;
		static const Vector3 Zero;
		static const Vector3 One;

		constexpr Vector3() noexcept : x(), y(), z() {}
		explicit constexpr Vector3(float uniform) noexcept : x(uniform), y(uniform), z(uniform) {}
		constexpr Vector3(float x, float y, float z) noexcept : x(x), y(y), z(z) {}
		explicit constexpr Vector3(Vector2 xy, float z = 0) : x(xy.x), y(xy.y), z(z) {}

		[[nodiscard]] constexpr float lengthSquared() const noexcept;
		[[nodiscard]] float length() const noexcept;
		void normalized() noexcept;
		[[nodiscard]] Vector3 normalize() const noexcept;
		[[nodiscard]] bool isNormalized() const noexcept;

		[[nodiscard]] constexpr bool operator==(const Vector3& other) const noexcept;
		[[nodiscard]] constexpr bool operator!=(const Vector3& other) const noexcept;
		[[nodiscard]] constexpr Vector3 operator+(const Vector3& other) const noexcept;
		constexpr Vector3& operator+=(const Vector3& other) noexcept;
		[[nodiscard]] constexpr Vector3 operator-(const Vector3& other) const noexcept;
		constexpr Vector3& operator-=(const Vector3& other) noexcept;
		[[nodiscard]] constexpr Vector3 operator-() noexcept;
		[[nodiscard]] constexpr Vector3 operator*(float scale) const noexcept;
		constexpr Vector3& operator*=(float scale) noexcept;
		[[nodiscard]] constexpr Vector3 operator/(float scale) const noexcept;
		constexpr Vector3& operator/=(float scale) noexcept;
		[[nodiscard]] constexpr float& operator[](int index) noexcept;
		[[nodiscard]] constexpr const float& operator[](int index) const noexcept;
	};

	inline constexpr Vector3 Vector3::Right{ (1.f), (0.f), (0.f) };
	inline constexpr Vector3 Vector3::Left{ (-1.f), (0.f), (0.f) };
	inline constexpr Vector3 Vector3::Up{ (0.f), (1.f), (0.f) };
	inline constexpr Vector3 Vector3::Down{ (0.f), (-1.f), (0.f) };
	inline constexpr Vector3 Vector3::Forward{ (0.f), (0.f), (-1.f) };
	inline constexpr Vector3 Vector3::Backward{ (0.f), (0.f), (1.f) };
	inline constexpr Vector3 Vector3::Zero{ (0.f) };
	inline constexpr Vector3 Vector3::One{ (1.f) };

	inline constexpr float Vector3::lengthSquared() const noexcept {
		return x * x + y * y + z * z;
	}

	inline float Vector3::length() const noexcept {
		return sqrt(lengthSquared());
	}

	inline void Vector3::normalized() noexcept {
		float squared = lengthSquared();
		if (squared == 0)
			return;
		float length = sqrt(squared);
		x /= length;
		y /= length;
		z /= length;
	}

	inline Vector3 Vector3::normalize() const noexcept {
		float squared = lengthSquared();
		if (squared == 0)
			return Vector3{};
		float length = sqrt(squared);
		return { x / length, y / length, z / length };
	}

	inline bool Vector3::isNormalized() const noexcept {
		float result = abs(lengthSquared() - 1.f);
		return result < EPSILON;
	}

	inline constexpr bool Vector3::operator==(const Vector3& other) const noexcept {
		return x == other.x && y == other.y && z == other.z;
	}

	inline constexpr bool Vector3::operator!=(const Vector3& other) const noexcept {
		return x != other.x || y != other.y || z != other.z;
	}

	inline constexpr Vector3 Vector3::operator+(const Vector3& other) const noexcept {
		return { x + other.x, y + other.y, z + other.z };
	}

	inline constexpr Vector3& Vector3::operator+=(const Vector3& other) noexcept {
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}

	inline constexpr Vector3 Vector3::operator-(const Vector3& other) const noexcept {
		return { x - other.x, y - other.y, z - other.z };
	}

	inline constexpr Vector3& Vector3::operator-=(const Vector3& other) noexcept {
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return *this;
	}

	inline constexpr Vector3 Vector3::operator-() noexcept {
		return { -x, -y, -z };
	}

	inline constexpr Vector3 Vector3::operator*(float scale) const noexcept {
		return { x * scale, y * scale, z * scale };
	}

	inline constexpr Vector3& Vector3::operator*=(float scale) noexcept {
		x *= scale;
		y *= scale;
		z *= scale;
		return *this;
	}

	inline constexpr Vector3 Vector3::operator/(float scale) const noexcept {
		CBK_ASSERT(scale != 0, "Trying to divide by zero!");
		return { x / scale, y / scale, z / scale };
	}

	inline constexpr Vector3& Vector3::operator/=(float scale) noexcept {
		CBK_ASSERT(scale != 0, "Trying to divide by zero!");
		x /= scale;
		y /= scale;
		z /= scale;
		return *this;
	}

	inline constexpr float& Vector3::operator[](int index) noexcept {
		CBK_ASSERT(index >= 0 || index < 3, "Trying to acces a Vector with invalid index!")
		return coords[index];
	}

	inline constexpr const float& Vector3::operator[](int index) const noexcept {
		CBK_ASSERT(index >= 0 || index < 3, "Trying to acces a Vector with invalid index!")
		return coords[index];
	}

	inline constexpr float dot(const Vector3& a, const Vector3& b) noexcept {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	inline constexpr Vector3 cross(const Vector3& a, const Vector3& b) noexcept {
		Vector3 res{};
		res.x = a.y * b.z - a.z * b.y;
		res.y = a.z * b.x - a.x * b.z;
		res.z = a.x * b.y - a.y * b.x;
		return res;
	}

	inline constexpr bool perpendicular(const Vector3& a, const Vector3& b) noexcept {
		return dot(a, b) == 0;
	}

	inline float angleBetween(const Vector3& a, const Vector3& b) noexcept {
		float denom = a.length() * b.length();
		if (denom == 0.f)
			return 0.f;
		float cosTheta = dot(a, b) / denom;
		cosTheta = std::clamp(cosTheta, -1.f, 1.f);
		return std::acos(cosTheta);
	}

} // namespace math
