#pragma once
#include "../../../Prerequisites.h"
#include "../Resource.h"
#include "../../Renderer/Buffer/Buffer.h"
#include <vector>


class Mesh : public Resource
{
public:
    Mesh(std::vector<Vertex> vertices);
	Mesh(const wchar_t* full_path);
	~Mesh();

private:
	std::vector<Vertex> m_vertices;
    BufferPtr m_vertexBuffer;

	friend class Object;
};

