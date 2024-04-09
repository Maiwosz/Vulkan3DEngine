#pragma once
#include "../../../Prerequisites.h"
#include "../Resource.h"
#include <vector>

class Mesh : public Resource
{
public:
	Mesh(std::vector<Vertex> vertices);
    Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
	Mesh(const char* full_path);
	~Mesh();

	void Reload() override;

	void draw();
private:
	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;

    VertexBufferPtr m_vertexBuffer;
	IndexBufferPtr m_indexBuffer;

	bool m_hasIndexedBuffer;

	friend class Object;
};

