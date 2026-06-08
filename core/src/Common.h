#pragma once

#include <glm/glm.hpp>

namespace lab {

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
	};
} // namespace lab