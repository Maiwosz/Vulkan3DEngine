#include "Mesh.h"

Mesh::Mesh(std::vector<Vertex> vertices) : Resource(), m_vertices(vertices)
{
	m_vertexBuffer = std::make_shared<Buffer>(vertices);
}

Mesh::Mesh(const wchar_t* full_path) : Resource(full_path)
{
	
}

Mesh::~Mesh()
{
}
