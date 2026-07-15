#pragma once

#include "Mat4.h"

namespace math {

    constexpr Mat4 zeroMat() noexcept;
	constexpr Mat4 identityMat() noexcept;
    constexpr Mat4 scaleUniform(float uniform) noexcept;
	constexpr Mat4 scaleXYZ(const Vector3&) noexcept;
	constexpr Mat4 translation(const Vector3&) noexcept;
	Mat4 rotateX(float angle) noexcept;
	Mat4 rotateY(float angle) noexcept;
	Mat4 rotateZ(float angle) noexcept;
	Mat4 rotateXYZ(const Vector3& euler) noexcept;
	Mat4 ortho(float left, float right, float bottom, float top, float nearZ, float farZ) noexcept;
	Mat4 lookAt(const Vector3& eye, const Vector3& target, const Vector3& up) noexcept;
	Mat4 inverseAffine(const Mat4& mat) noexcept;
	Mat4 fromTransform(const Vector3& pos, const Vector3& rot, const Vector3& scale) noexcept;

	inline constexpr Mat4 zeroMat() noexcept {
		return {};
	}

	inline constexpr Mat4 identityMat() noexcept {
		return { 
			1.f, 0.f, 0.f, 0.f, 
			0.f, 1.f, 0.f, 0.f, 
			0.f, 0.f, 1.f, 0.f, 
			0.f, 0.f, 0.f, 1.f 
		};
	}

	inline constexpr Mat4 scaleUniform(float uniform) noexcept {
		Mat4 res = identityMat();
		res[0][0] = res[1][1] = res[2][2] = uniform;
		return res;
	}

	inline constexpr Mat4 scaleXYZ(const Vector3& vec) noexcept {
		Mat4 res = identityMat();
		res[0][0] = vec.x;
		res[1][1] = vec.y;
		res[2][2] = vec.z;
		return res;
	}

	inline constexpr Mat4 translation(const Vector3& vec) noexcept {
		Mat4 res = identityMat();
		res[3] = Vector4(vec);
		return res;
	}

    inline Mat4 rotateX(float angle) noexcept {
		float angleInRadians = angle * PI / 180.f;
		return { 1.f, 0.f, 0.f, 0.f, 0.f, cosf(angleInRadians), sinf(angleInRadians), 0.f, 0.f, -sinf(angleInRadians), cosf(angleInRadians),
			     0.f, 0.f, 0.f, 0.f, 1.f
		};
	}

	inline Mat4 rotateY(float angle) noexcept {
		float angleInRadians = angle * PI / 180.f;
		return { cosf(angleInRadians), 0.f, -sinf(angleInRadians), 0.f, 0.f, 1.f, 0.f, 0.f,
			     sinf(angleInRadians), 0.f, cosf(angleInRadians), 0.f, 0.f, 0.f, 0.f, 1.f
		};
	}

	inline Mat4 rotateZ(float angle) noexcept {
		float angleInRadians = angle * PI / 180.f;
		return { cosf(angleInRadians), sinf(angleInRadians), 0.f,
			     0.f,
			     -sinf(angleInRadians),
			     cosf(angleInRadians),
			     0.f,
			     0.f,
			     0.f,
			     0.f,
			     1.f,
			     0.f,
			     0.f,
			     0.f,
			     0.f, 1.f
		};
	}

	inline Mat4 rotateXYZ(const Vector3& euler) noexcept {
		Mat4 rx = rotateX(euler.x);
		Mat4 ry = rotateY(euler.y);
		Mat4 rz = rotateZ(euler.z);

		return rz * rx * ry;
	}

	inline Mat4 ortho(float left, float right, float bottom, float top, float nearZ, float farZ) noexcept {
		float rl = right - left;
		float tb = top - bottom;
		float fn = farZ - nearZ;
		Mat4 res = identityMat();
		res[0][0] = 2.f / rl;
		res[1][1] = 2.f / tb;
		res[2][2] = -2.f / fn;
		res[3][0] = -(right + left) / rl;
		res[3][1] = -(top + bottom) / tb;
		res[3][2] = -(farZ + nearZ) / fn;

		return res;
	}

	inline Mat4 perspective(float fovy, float aspect, float nearZ, float farZ) noexcept {
		float f = 1.0f / std::tan(fovy * 0.5f);
		float nf = nearZ - farZ;
		Mat4 res = zeroMat();
		res[0][0] = f / aspect;
		res[1][1] = f;
		res[2][2] = (farZ + nearZ) / nf;
		res[2][3] = -1.f;
		res[3][2] = (2.f * farZ * nearZ) / nf;

		return res;
	}

	// Right-handed view matrix: the camera sits at eye, looks towards target and views down
	// its own -Z. Equivalent to inverseAffine() of the camera's world matrix, but built
	// directly from the basis so it skips the transpose.
	inline Mat4 lookAt(const Vector3& eye, const Vector3& target, const Vector3& up) noexcept {
		const Vector3 forward = (target - eye).normalize();
		const Vector3 right = cross(forward, up).normalize();
		// Re-derived so the basis stays orthonormal even when up is not perpendicular to forward
		const Vector3 trueUp = cross(right, forward);

		return {
			right.x,           trueUp.x,           -forward.x,        0.f,
			right.y,           trueUp.y,           -forward.y,        0.f,
			right.z,           trueUp.z,           -forward.z,        0.f,
			-dot(right, eye),  -dot(trueUp, eye),  dot(forward, eye), 1.f
		};
	}

	// TODO: Make a Mat3 struct to call its transpose function to simplify this
    inline Mat4 inverseAffine(const Mat4& m) noexcept {
		// Transpose rotation part.
		Mat4 inv{};
		inv.elements[0] = Vector4{ m.elements[0].x, m.elements[1].x, m.elements[2].x, 0.f };
		inv.elements[1] = Vector4{ m.elements[0].y, m.elements[1].y, m.elements[2].y, 0.f };
		inv.elements[2] = Vector4{ m.elements[0].z, m.elements[1].z, m.elements[2].z, 0.f };

		// Invert translation
		const Vector4& t = m.elements[3];
		inv.elements[3].x = -(t.x * inv.elements[0].x + t.y * inv.elements[1].x + t.z * inv.elements[2].x);
		inv.elements[3].y = -(t.x * inv.elements[0].y + t.y * inv.elements[1].y + t.z * inv.elements[2].y);
		inv.elements[3].z = -(t.x * inv.elements[0].z + t.y * inv.elements[1].z + t.z * inv.elements[2].z);
		inv.elements[3].w = 1;

		return inv;
	}

	inline Mat4 fromTransform(const Vector3& pos, const Vector3& rot, const Vector3& scale) noexcept {
		return scaleXYZ(scale) * rotateXYZ(rot) * translation(pos);
	}
} // namespace math
