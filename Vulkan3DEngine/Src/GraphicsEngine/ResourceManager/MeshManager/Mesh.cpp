#include "Mesh.h"
#include "../../GraphicsEngine.h"
#include "../../Renderer/Buffers/VertexBuffer/VertexBuffer.h"
#include "../../Renderer/Buffers/IndexBuffer/IndexBuffer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

Mesh::Mesh(std::vector<Vertex> vertices) :
	Resource(), m_vertices(vertices), m_hasIndexedBuffer(false)
{
	m_vertexBuffer = GraphicsEngine::get()->getRenderer()->createVertexBuffer(vertices);
}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices) :
	Resource(), m_vertices(vertices), m_indices(indices), m_hasIndexedBuffer(true)
{
	m_vertexBuffer = GraphicsEngine::get()->getRenderer()->createVertexBuffer(vertices);
	m_indexBuffer = GraphicsEngine::get()->getRenderer()->createIndexBuffer(indices);
}

Mesh::Mesh(const char* full_path) : Resource(full_path), m_hasIndexedBuffer(true)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, full_path)) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = { 1.0f, 1.0f, 1.0f };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
                m_vertices.push_back(vertex);
            }

            m_indices.push_back(uniqueVertices[vertex]);
        }
    }
    m_vertexBuffer = GraphicsEngine::get()->getRenderer()->createVertexBuffer(m_vertices);
    m_indexBuffer = GraphicsEngine::get()->getRenderer()->createIndexBuffer(m_indices);
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
