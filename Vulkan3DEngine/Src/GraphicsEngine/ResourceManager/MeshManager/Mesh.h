#pragma once
#include "../../../Prerequisites.h"
#include "../Resource.h"
#include "../../Renderer/VertexBuffer/VertexBuffer.h"
#include "../../Renderer/IndexBuffer/IndexBuffer.h"
#include <vector>


class Mesh : public Resource
{
public:
    Mesh(std::vector<Vertex> vertices, std::vector<uint16_t> indices);
	Mesh(const wchar_t* full_path);
	~Mesh();

	void draw();
private:
	std::vector<Vertex> m_vertices;
	std::vector<uint16_t> m_indices;

    VertexBufferPtr m_vertexBuffer;
	IndexBufferPtr m_indexBuffer;

	friend class Object;
};

