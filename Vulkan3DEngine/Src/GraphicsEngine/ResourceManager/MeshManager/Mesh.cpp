#include "Mesh.h"
#include "../../GraphicsEngine.h"
#include "../../Renderer/Buffers/VertexBuffer/VertexBuffer.h"
#include "../../Renderer/Buffers/IndexBuffer/IndexBuffer.h"

Mesh::Mesh(std::vector<Vertex> vertices) :
	Resource(), m_vertices(vertices), m_hasIndexedBuffer(false)
{
	m_vertexBuffer = GraphicsEngine::get()->getRenderer()->createVertexBuffer(vertices);
}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<uint16_t> indices) :
	Resource(), m_vertices(vertices), m_indices(indices), m_hasIndexedBuffer(true)
{
	m_vertexBuffer = GraphicsEngine::get()->getRenderer()->createVertexBuffer(vertices);
	m_indexBuffer = GraphicsEngine::get()->getRenderer()->createIndexBuffer(indices);
}

Mesh::Mesh(const char* full_path) : Resource(full_path)
{
	
}

Mesh::~Mesh()
{
}

void Mesh::draw()
{
	m_vertexBuffer->bind();
	if (m_hasIndexedBuffer) {
		m_indexBuffer->bind();
	}
}
