#pragma once
#include "Prerequisites.h"
#include "Resource.h"
#include <vector>

class Mesh : public Resource
{
public:
	Mesh(std::vector<Vertex> vertices);
	Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
	Mesh(const std::filesystem::path& full_path);
	~Mesh();


	void Reload() override;

	bool hasIndexBuffer() const { return m_hasIndexBuffer; }

	size_t getIndicesSize() const { return m_indices.size(); }
private:
	void Load(const std::filesystem::path& full_path) override;

	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;

	VertexBufferPtr m_vertexBuffer;
	IndexBufferPtr m_indexBuffer;

	bool m_hasIndexBuffer;

	friend class Renderer;
};

