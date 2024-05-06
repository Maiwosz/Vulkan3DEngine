#include "Mesh.h"
#include "GraphicsEngine.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

Mesh::Mesh(std::vector<Vertex> vertices) :
	Resource(), m_vertices(vertices), m_hasIndexBuffer(false)
{
	m_vertexBuffer = GraphicsEngine::get()->getRenderer()->createVertexBuffer(vertices);
}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices) :
	Resource(), m_vertices(vertices), m_indices(indices), m_hasIndexBuffer(true)
{
	m_vertexBuffer = GraphicsEngine::get()->getRenderer()->createVertexBuffer(vertices);
	m_indexBuffer = GraphicsEngine::get()->getRenderer()->createIndexBuffer(indices);
}

Mesh::Mesh(const char* full_path) : Resource(full_path), m_hasIndexBuffer(true)
{
    Load(full_path);
}

Mesh::~Mesh()
{
}

void Mesh::Load(const char* full_path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, full_path)) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    float minY = std::numeric_limits<float>::max();

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            minY = std::min(minY, vertex.pos.y); // znajdü najniøszy punkt y

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            if (index.normal_index >= 0) { // Sprawdü, czy normalna istnieje
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }

            vertex.color = { 1.0f, 1.0f, 1.0f };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
                m_vertices.push_back(vertex);
            }

            m_indices.push_back(uniqueVertices[vertex]);
        }
    }

    // PrzesuÒ wszystkie wierzcho≥ki o minY w gÛrÍ
    for (auto& vertex : m_vertices) {
        vertex.pos.y -= minY;
    }

    m_vertexBuffer = GraphicsEngine::get()->getRenderer()->createVertexBuffer(m_vertices);
    m_indexBuffer = GraphicsEngine::get()->getRenderer()->createIndexBuffer(m_indices);
}

void Mesh::Reload()
{
    vkDeviceWaitIdle(GraphicsEngine::get()->getDevice()->get());

    // Free the old resources
    m_vertexBuffer.reset();
    if (m_hasIndexBuffer) {
        m_indexBuffer.reset();
    }

    m_vertices.clear();
    m_indices.clear();

    // Load the new mesh
    Load(m_full_path.c_str());
}

void Mesh::draw()
{
	m_vertexBuffer->bind();
	if (m_hasIndexBuffer) {
		m_indexBuffer->bind();
	}
}
