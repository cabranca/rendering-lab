#pragma once

#include "Vector3.h"

namespace math {

	struct Vector4 {
		union {
			struct {
				float x, y, z, w;
			};

			float coords[4];
		};

		constexpr Vector4() noexcept : x(), y(), z(), w() {}
		explicit constexpr Vector4(float uniform) noexcept : x(uniform), y(uniform), z(uniform), w(uniform) {}
		constexpr Vector4(float x, float y, float z, float w) noexcept : x(x), y(y), z(z), w(w) {}
		constexpr Vector4(const Vector3& v, float w = 1.f) noexcept : x(v.x), y(v.y), z(v.z), w(w) {}

		[[nodiscard]] constexpr float lengthSquared() const noexcept;
		[[nodiscard]] float length() const noexcept;
		void normalized() noexcept;
		[[nodiscard]] bool isNormalized() const noexcept;

		[[nodiscard]] constexpr bool operator==(const Vector4& other) const noexcept;
		[[nodiscard]] constexpr bool operator!=(const Vector4& other) const noexcept;
		[[nodiscard]] constexpr Vector4 operator+(const Vector4& other) const noexcept;
		constexpr Vector4& operator+=(const Vector4& other) noexcept;
		[[nodiscard]] constexpr Vector4 operator-(const Vector4& other) const noexcept;
		constexpr Vector4& operator-=(const Vector4& other) noexcept;
		[[nodiscard]] constexpr Vector4 operator-() const noexcept;
		[[nodiscard]] constexpr Vector4 operator*(float scale) const noexcept;
		constexpr Vector4& operator*=(float scale) noexcept;
		[[nodiscard]] constexpr Vector4 operator/(float scale) const noexcept;
		constexpr Vector4& operator/=(float scale) noexcept;
		[[nodiscard]] constexpr float& operator[](int index) noexcept;
		[[nodiscard]] constexpr const float& operator[](int index) const noexcept;

		[[nodiscard]] constexpr Vector3 toVector3() const noexcept;
	};

	inline constexpr float Vector4::lengthSquared() const noexcept {
		return x * x + y * y + z * z + w * w;
	}

	inline float Vector4::length() const noexcept {
		return std::sqrt(lengthSquared());
	}

	inline void Vector4::normalized() noexcept {
		float squared = lengthSquared();
		if (squared == 0.f)
			return;
		float len = std::sqrt(squared);
		x /= len;
		y /= len;
		z /= len;
		w /= len;
	}

	inline bool Vector4::isNormalized() const noexcept {
		float result = std::abs(lengthSquared() - 1.f);
		return result < EPSILON;
	}

	inline constexpr bool Vector4::operator==(const Vector4& other) const noexcept {
		return x == other.x && y == other.y && z == other.z && w == other.w;
	}

	inline constexpr bool Vector4::operator!=(const Vector4& other) const noexcept {
		return !(*this == other);
	}

	inline constexpr Vector4 Vector4::operator+(const Vector4& other) const noexcept {
		return { x + other.x, y + other.y, z + other.z, w + other.w };
	}

	inline constexpr Vector4& Vector4::operator+=(const Vector4& other) noexcept {
		x += other.x;
		y += other.y;
		z += other.z;
		w += other.w;
		return *this;
	}

	inline constexpr Vector4 Vector4::operator-(const Vector4& other) const noexcept {
		return { x - other.x, y - other.y, z - other.z, w - other.w };
	}

	inline constexpr Vector4& Vector4::operator-=(const Vector4& other) noexcept {
		x -= other.x;
		y -= other.y;
		z -= other.z;
		w -= other.w;
		return *this;
	}

	inline constexpr Vector4 Vector4::operator-() const noexcept {
		return { -x, -y, -z, -w };
	}

	inline constexpr Vector4 Vector4::operator*(float scale) const noexcept {
		return { x * scale, y * scale, z * scale, w * scale };
	}

	inline constexpr Vector4& Vector4::operator*=(float scale) noexcept {
		x *= scale;
		y *= scale;
		z *= scale;
		w *= scale;
		return *this;
	}

	inline constexpr Vector4 Vector4::operator/(float scale) const noexcept {
		CBK_ASSERT(scale != 0, "Trying to divide by zero!");
		return { x / scale, y / scale, z / scale, w / scale };
	}

	inline constexpr Vector4& Vector4::operator/=(float scale) noexcept {
		CBK_ASSERT(scale != 0, "Trying to divide by zero!");

		x /= scale;
		y /= scale;
		z /= scale;
		w /= scale;
		return *this;
	}

	inline constexpr float& Vector4::operator[](int index) noexcept {
		CBK_ASSERT(index >= 0 || index < 4, "Trying to acces a Vector with invalid index!");
		return coords[index];
	}

	inline constexpr const float& Vector4::operator[](int index) const noexcept {
		CBK_ASSERT(index >= 0 || index < 4, "Trying to acces a Vector with invalid index!");
		return coords[index];
	}

	inline constexpr Vector3 Vector4::toVector3() const noexcept {
		return { x, y, z };
	}

	inline constexpr float dot(const Vector4& a, const Vector4& b) noexcept {
		return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	}

	inline constexpr bool perpendicular(const Vector4& a, const Vector4& b) noexcept {
		return dot(a, b) == 0;
	}

	inline float angleBetween(const Vector4& a, const Vector4& b) noexcept {
		float denom = a.length() * b.length();
		if (denom == 0.f)
			return 0.f;
		float cosTheta = dot(a, b) / denom;
		cosTheta = std::clamp(cosTheta, -1.f, 1.f);
		return std::acos(cosTheta);
	}
}; // namespace math
