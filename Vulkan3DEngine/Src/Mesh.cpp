#include "Mesh.h"
#include "GraphicsEngine.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

#include <ranges>
#include <unordered_map>
#include <algorithm>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

Mesh::Mesh(std::vector<Vertex> vertices) :
    Resource(), m_vertices(std::move(vertices)), m_hasIndexBuffer(false)
{
    m_vertexBuffer = GraphicsEngine::get()->getRenderer()->createVertexBuffer(m_vertices);
}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices) :
    Resource(), m_vertices(std::move(vertices)), m_indices(std::move(indices)), m_hasIndexBuffer(true)
{
    m_vertexBuffer = GraphicsEngine::get()->getRenderer()->createVertexBuffer(m_vertices);
    m_indexBuffer = GraphicsEngine::get()->getRenderer()->createIndexBuffer(m_indices);
}

Mesh::Mesh(const std::filesystem::path& full_path) : Resource(full_path), m_hasIndexBuffer(true)
{
    Load(full_path);
}

Mesh::~Mesh()
{
}

void Mesh::Load(const std::filesystem::path& full_path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, full_path.string().c_str())) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    float minY = std::numeric_limits<float>::max();

    // Rezerwuj miejsce na wierzcho≥ki i indeksy
    m_vertices.reserve(attrib.vertices.size() / 3);
    for (const auto& shape : shapes) {
        m_indices.reserve(m_indices.size() + shape.mesh.indices.size());
    }

    auto processVertex = [&](const tinyobj::index_t& index) {
        Vertex vertex{};

        vertex.pos = {
            attrib.vertices[3 * index.vertex_index + 0],
            attrib.vertices[3 * index.vertex_index + 1],
            attrib.vertices[3 * index.vertex_index + 2]
        };

        minY = std::min(minY, vertex.pos.y); // Znajdü najniøszy punkt y

        if (index.texcoord_index >= 0) { // Sprawdü, czy wspÛ≥rzÍdne tekstury istniejπ
            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };
        }

        if (index.normal_index >= 0) { // Sprawdü, czy normalna istnieje
            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            };
        }

        vertex.color = { 1.0f, 1.0f, 1.0f };

        if (uniqueVertices.find(vertex) == uniqueVertices.end()) {
            uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
            m_vertices.push_back(vertex);
        }

        m_indices.push_back(uniqueVertices[vertex]);
        };

    for (const auto& shape : shapes) {
        std::ranges::for_each(shape.mesh.indices, processVertex);
    }

    // PrzesuÒ wszystkie wierzcho≥ki o minY w gÛrÍ
    std::ranges::for_each(m_vertices, [minY](Vertex& vertex) {
        vertex.pos.y -= minY;
        });

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
    Load(m_full_path);
}

