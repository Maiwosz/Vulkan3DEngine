#pragma once
#include <memory>

#include <glm/glm.hpp>
#include <array>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Application;

class Window;
class GraphicsEngine;
class Device;
class Renderer;
class SwapChain;
class GraphicsPipeline;
class Resource;
class ResourceManager;
class Texture;
class TextureManager;
class Mesh;
class MeshManager;
class Object;
class VertexBuffer;
class IndexBuffer;

typedef std::shared_ptr<Window> WindowPtr;
typedef std::shared_ptr<Device> DevicePtr;
typedef std::shared_ptr<Renderer> RendererPtr;
typedef std::shared_ptr<SwapChain> SwapChainPtr;
typedef std::shared_ptr<GraphicsPipeline> GraphicsPipelinePtr;
typedef std::shared_ptr<Resource> ResourcePtr;
typedef std::shared_ptr<ResourceManager> ResourceManagerPtr;
typedef std::shared_ptr<Texture> TexturePtr;
typedef std::shared_ptr<TextureManager> TextureManagerPtr;
typedef std::shared_ptr<Mesh> MeshPtr;
typedef std::shared_ptr<MeshManager> MeshManagerPtr;
typedef std::shared_ptr<Object> ObjectPtr;
typedef std::shared_ptr<VertexBuffer> VertexBufferPtr;
typedef std::shared_ptr<IndexBuffer> IndexBufferPtr;

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};
