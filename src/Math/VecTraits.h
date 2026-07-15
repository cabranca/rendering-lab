#pragma once

#include "Vector2.h"
#include "Vector3.h"
#include "Quaternion.h"

namespace math {

	// Primary template — intentionally undefined; only specializations below are valid
	template<typename VecType>
	struct VecTraits;

	template<>
	struct VecTraits<Vector2> {
		static constexpr int Dimensions = 2;
		using OrientationType = float;
	};

	template<>
	struct VecTraits<Vector3> {
		static constexpr int Dimensions = 3;
		using OrientationType = Quaternion;
	};

} // namespace math
