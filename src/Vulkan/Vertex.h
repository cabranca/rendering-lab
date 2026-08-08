#pragma once

#include <array>
#include <cstddef>
#include <functional>

#include <volk/volk.h>

#include <Math/MatrixFactory.h>

namespace lab::vk {

	struct Vertex {
		math::Vector3 Position;
		math::Vector2 TexCoords;

		static VkVertexInputBindingDescription getBindingDescription() {
			return { .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
		}

		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
			return { { { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, Position) },
				       { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, TexCoords) } } };
		}

		bool operator==(const Vertex& other) const {
			return Position == other.Position && TexCoords == other.TexCoords;
		}
	};

} // namespace lab::vk

template <>
struct std::hash<lab::vk::Vertex> {
	size_t operator()(const lab::vk::Vertex& vertex) const noexcept {
		return std::hash<math::Vector3>()(vertex.Position) ^ (std::hash<math::Vector2>()(vertex.TexCoords) << 1);
	}
};
