#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "Common.h"

namespace lab {

	class ModelImporter {
	  public:
		void import(std::string_view path);

		std::span<Vertex> getVertices() {
			return std::span<Vertex>(m_Vertices);
		}
		std::span<uint16_t> getIndices() {
			return std::span(m_Indices);
		}
		uint32_t getIndexCount() const {
			return m_IndexCount;
		}

	  private:
		std::vector<Vertex> m_Vertices;
		std::vector<uint16_t> m_Indices;
		uint32_t m_IndexCount = 0;
	};
} // namespace lab