#include "Mesh.h"

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<uint16_t> indices) :
	Resource(), m_vertices(vertices), m_indices(indices)
{
	m_vertexBuffer = std::make_shared<VertexBuffer>(vertices);
	m_indexBuffer = std::make_shared<IndexBuffer>(indices);
}

Mesh::Mesh(const wchar_t* full_path) : Resource(full_path)
{
	
}

Mesh::~Mesh()
{
}

void Mesh::draw()
{
	//vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	m_vertexBuffer->bind();
	m_indexBuffer->bind();
	vkCmdDrawIndexed(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
}
