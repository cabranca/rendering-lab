#pragma once

#include <array>

#include "Vector4.h"

namespace math {

	// Our 4x4 Matrix representation is row-major and it stores values as Mat4[row][col]
	// TODO: add inversion.
	// TODO: add accesor operator.
	struct Mat4 {
		std::array<Vector4, 4> elements;

		constexpr Mat4() noexcept : elements{ Vector4(), Vector4(), Vector4(), Vector4() } {}

		constexpr Mat4(float ix, float iy, float iz, float iw, float jx, float jy, float jz, float jw, float kx, float ky, float kz,
		               float kw, float tx, float ty, float tz, float tw) noexcept
		    : elements{ Vector4(ix, iy, iz, iw), Vector4(jx, jy, jz, jw), Vector4(kx, ky, kz, kw), Vector4(tx, ty, tz, tw) } {}

		constexpr Mat4(Vector3 row0, Vector3 row1, Vector3 row2, Vector3 row3) : elements{ row0, row1, row2, row3 } {}

		[[nodiscard]] constexpr const Vector4& operator[](int index) const noexcept;
		[[nodiscard]] constexpr Vector4& operator[](int index) noexcept;
		[[nodiscard]] constexpr bool operator==(const Mat4& other) const noexcept;
		[[nodiscard]] constexpr bool operator!=(const Mat4& other) const noexcept;
		[[nodiscard]] constexpr Mat4 operator+(const Mat4& other) const noexcept;
		[[nodiscard]] constexpr Mat4 operator*(const Mat4& other) const noexcept;
		[[nodiscard]] constexpr Mat4 transpose() const noexcept;
	};

	constexpr const Vector4& Mat4::operator[](int index) const noexcept {
		CBK_ASSERT(index >= 0 || index < 4, "Trying to acces a Vector with invalid index!");
		return elements[index];
	}

	constexpr Vector4& Mat4::operator[](int index) noexcept {
		CBK_ASSERT(index >= 0 || index < 4, "Trying to acces a Vector with invalid index!");
		return elements[index];
	}

	constexpr bool Mat4::operator==(const Mat4& other) const noexcept {
		return elements == other.elements;
	}

	constexpr bool Mat4::operator!=(const Mat4& other) const noexcept {
		return elements != other.elements;
	}

	constexpr Mat4 Mat4::operator+(const Mat4& other) const noexcept {
		Mat4 res{};

		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j)
				res[i][j] = elements[i][j] + other[i][j];
		}
		return res;
	}

	// Generalized for every matrix
	constexpr Mat4 Mat4::operator*(const Mat4& other) const noexcept {
		Mat4 transposed = other.transpose();
		Mat4 res{};

		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j)
				res[i][j] = dot(elements[i], transposed[j]);
		}
		return res;
	}

	inline constexpr Mat4 Mat4::transpose() const noexcept {
		auto& e = elements;
		return { e[0].x, e[1].x, e[2].x, e[3].x, e[0].y, e[1].y, e[2].y, e[3].y,
			     e[0].z, e[1].z, e[2].z, e[3].z, e[0].w, e[1].w, e[2].w, e[3].w };
	}

	// TODO: I think this only works for points and not for directions, do a double check
	inline constexpr Vector4 operator*(const Vector4& v, const Mat4& m) noexcept {
		Vector4 res{};
		const auto transposed = m.transpose();
		for (int i = 0; i < 4; i++) {
			res[i] = dot(v, transposed[i]);
		}
		return res;
	}

	// For this product we convert the Vector3 to Vector4 adding w = 1
	// TODO: I think this only works for points and not for directions, do a double check
	inline constexpr Vector3 operator*(const Vector3& v, const Mat4& m) noexcept {
		const Vector4 aux{ v };
		const auto result = aux * m;
		return Vector3{ result.x, result.y, result.z };
	}
} // namespace math
