#include "Mesh.h"
#include "../../Renderer/Buffer/VertexBuffer/VertexBuffer.h"
#include "../../Renderer/Buffer/IndexBuffer/IndexBuffer.h"

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

Mesh::Mesh(const wchar_t* full_path) : Resource(full_path)
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
		vkCmdDrawIndexed(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
	}
	else {
		vkCmdDraw(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), 3, 1, 0, 0);
	}
}
