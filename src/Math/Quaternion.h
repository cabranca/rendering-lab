#pragma once

#include <cmath>
#include <array>

#include "Common.h"
#include "Vector3.h"

namespace math {

	struct Quaternion {
		float w, x, y, z;

		constexpr Quaternion() noexcept : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {}
		constexpr Quaternion(float w, float x, float y, float z) noexcept : w(w), x(x), y(y), z(z) {}

		// Construct from axis (must be normalized) and angle in radians
		[[nodiscard]] static Quaternion fromAxisAngle(const Vector3& axis, float angleRad) noexcept {
			float half = angleRad * 0.5f;
			float s = std::sin(half);
			float c = std::cos(half);
			return { c, axis.x * s, axis.y * s, axis.z * s };
		}

		// Construct from Euler angles (radians), rotation order: X -> Y -> Z
		[[nodiscard]] static Quaternion fromEuler(const Vector3& euler) noexcept {
			float cx = std::cos(euler.x * 0.5f);
			float sx = std::sin(euler.x * 0.5f);
			float cy = std::cos(euler.y * 0.5f);
			float sy = std::sin(euler.y * 0.5f);
			float cz = std::cos(euler.z * 0.5f);
			float sz = std::sin(euler.z * 0.5f);

			return { cx * cy * cz + sx * sy * sz, sx * cy * cz - cx * sy * sz, cx * sy * cz + sx * cy * sz, cx * cy * sz - sx * sy * cz };
		}

		[[nodiscard]] constexpr float lengthSquared() const noexcept {
			return w * w + x * x + y * y + z * z;
		}

		[[nodiscard]] float length() const noexcept {
			return std::sqrt(lengthSquared());
		}

		[[nodiscard]] Quaternion normalize() const noexcept {
			float len = length();
			if (len < EPSILON)
				return Quaternion{};
			return { w / len, x / len, y / len, z / len };
		}

		[[nodiscard]] constexpr Quaternion conjugate() const noexcept {
			return { w, -x, -y, -z };
		}

		// Quaternion multiplication (combines rotations: this * other)
		[[nodiscard]] constexpr Quaternion operator*(const Quaternion& q) const noexcept {
			return { w * q.w - x * q.x - y * q.y - z * q.z, w * q.x + x * q.w + y * q.z - z * q.y, w * q.y - x * q.z + y * q.w + z * q.x,
				     w * q.z + x * q.y - y * q.x + z * q.w };
		}

		// Rotate a vector by this quaternion: q * v * q^-1
		[[nodiscard]] Vector3 rotate(const Vector3& v) const noexcept {
			// Optimized: t = 2 * cross(q.xyz, v); result = v + w*t + cross(q.xyz, t)
			Vector3 qv{ x, y, z };
			Vector3 t = cross(qv, v) * 2.0f;
			return v + t * w + cross(qv, t);
		}

		// Extract 3 orthonormal local axes: [right, up, forward]
		[[nodiscard]] std::array<Vector3, 3> toAxes() const noexcept {
			float xx = x * x, yy = y * y, zz = z * z;
			float xy = x * y, xz = x * z, yz = y * z;
			float wx = w * x, wy = w * y, wz = w * z;

			return { { { 1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy) },
				       { 2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx) },
				       { 2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy) } } };
		}
	};

} // namespace math
