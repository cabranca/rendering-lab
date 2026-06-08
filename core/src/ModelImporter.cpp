#include "ModelImporter.h"

// The TINYOBJLOADER_IMPLEMENTATION lives in external/tinyobj/tiny_obj_loader.cc
// (compiled separately), so this file only needs the declarations.
#include <tiny_obj_loader.h>

#include "Check.h"

namespace lab {

	void ModelImporter::import(std::string_view path) {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		chk(tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, nullptr, path.data()));
		m_IndexCount = shapes[0].mesh.indices.size();

		for (auto& index: shapes[0].mesh.indices) {
			Vertex v{ .pos = { attrib.vertices[index.vertex_index * 3], -attrib.vertices[index.vertex_index * 3 + 1],
				               attrib.vertices[index.vertex_index * 3 + 2] },
				      .normal = { attrib.normals[index.normal_index * 3], -attrib.normals[index.normal_index * 3 + 1],
				                  attrib.normals[index.normal_index * 3 + 2] },
				      .uv = { attrib.texcoords[index.texcoord_index * 2], 1.0 - attrib.texcoords[index.texcoord_index * 2 + 1] } };
			m_Vertices.push_back(v);
			m_Indices.push_back(m_Indices.size());
		}
	}
} // namespace lab
