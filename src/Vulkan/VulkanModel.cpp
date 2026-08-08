#include "VulkanModel.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "Vertex.h"

namespace lab::vk {

	VulkanModel VulkanModel::loadOBJ(const VulkanDevice& device, std::string_view path) {
		// string_view is not guaranteed null-terminated; tinyobj wants a C string.
		const std::string filePath(path);

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
			throw std::runtime_error(warn + err);
		}

		VulkanModel model;
		model.m_Meshes.reserve(shapes.size());

		for (const auto& shape: shapes) {
			std::vector<Vertex> vertices;
			std::vector<uint32_t> indices;
			std::unordered_map<Vertex, uint32_t> uniqueVertices;

			for (const auto& index: shape.mesh.indices) {
				Vertex vertex{ .Position = { attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
					                         attrib.vertices[3 * index.vertex_index + 2] },
					           .TexCoords = { attrib.texcoords[2 * index.texcoord_index + 0],
					                          1.0f - attrib.texcoords[2 * index.texcoord_index + 1] } };

				auto [it, inserted] = uniqueVertices.insert({ vertex, static_cast<uint32_t>(vertices.size()) });
				if (inserted)
					vertices.push_back(vertex);
				indices.emplace_back(it->second);
			}

			model.m_Meshes.emplace_back(device, vertices, indices);
		}

		return model;
	}

	void VulkanModel::draw(VkCommandBuffer cmdBuffer) const {
		for (const auto& mesh: m_Meshes) {
			mesh.bind(cmdBuffer);
			mesh.draw(cmdBuffer);
		}
	}
} // namespace lab::vk
