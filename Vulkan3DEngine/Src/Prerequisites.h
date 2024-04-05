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
class Buffer;
class StagingBuffer;
class VertexBuffer;
class IndexBuffer;
class UniformBuffer;
class DescriptorPool;
class DescriptorSetLayout;
class GlobalDescriptorSetLayout;
class TextureDescriptorSetLayout;
class DescriptorSet;
class GlobalDescriptorSet;
class TextureDescriptorSet;
class Image;
class ImageView;
class TextureSampler;

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
typedef std::shared_ptr<Buffer> BufferPtr;
typedef std::shared_ptr<StagingBuffer> StagingBufferPtr;
typedef std::shared_ptr<VertexBuffer> VertexBufferPtr;
typedef std::shared_ptr<IndexBuffer> IndexBufferPtr;
typedef std::shared_ptr<UniformBuffer> UniformBufferPtr;
typedef std::shared_ptr<DescriptorPool> DescriptorPoolPtr;
typedef std::shared_ptr<DescriptorSetLayout> DescriptorSetLayoutPtr;
typedef std::shared_ptr<GlobalDescriptorSetLayout> GlobalDescriptorSetLayoutPtr;
typedef std::shared_ptr<TextureDescriptorSetLayout> TextureDescriptorSetLayoutPtr;
typedef std::shared_ptr<DescriptorSet> DescriptorSetPtr;
typedef std::shared_ptr<GlobalDescriptorSet> GlobalDescriptorSetPtr;
typedef std::shared_ptr<TextureDescriptorSet> TextureDescriptorSetPtr;
typedef std::shared_ptr<Image> ImagePtr;
typedef std::shared_ptr<ImageView> ImageViewPtr;
typedef std::shared_ptr<TextureSampler> TextureSamplerPtr;

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

