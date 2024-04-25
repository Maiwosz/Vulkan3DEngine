#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <memory>
#include <array>

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
class ModelData;
class ModelInstance;
class ModelManager;
class Buffer;
class StagingBuffer;
class VertexBuffer;
class IndexBuffer;
class UniformBuffer;
class DescriptorPool;
class DescriptorSetLayout;
class GlobalDescriptorSetLayout;
class TextureDescriptorSetLayout;
class TransformDescriptorSetLayout;
class DescriptorSet;
class GlobalDescriptorSet;
class TextureDescriptorSet;
class TransformDescriptorSet;
class Image;
class ImageView;
class TextureSampler;
class DepthBuffer;
class Scene;
class Camera;

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
typedef std::shared_ptr<ModelData> ModelDataPtr;
typedef std::shared_ptr<ModelInstance> ModelInstancePtr;
typedef std::shared_ptr<ModelManager> ModelManagerPtr;
typedef std::shared_ptr<Buffer> BufferPtr;
typedef std::shared_ptr<StagingBuffer> StagingBufferPtr;
typedef std::shared_ptr<VertexBuffer> VertexBufferPtr;
typedef std::shared_ptr<IndexBuffer> IndexBufferPtr;
typedef std::shared_ptr<UniformBuffer> UniformBufferPtr;
typedef std::shared_ptr<DescriptorPool> DescriptorPoolPtr;
typedef std::shared_ptr<DescriptorSetLayout> DescriptorSetLayoutPtr;
typedef std::shared_ptr<GlobalDescriptorSetLayout> GlobalDescriptorSetLayoutPtr;
typedef std::shared_ptr<TextureDescriptorSetLayout> TextureDescriptorSetLayoutPtr;
typedef std::shared_ptr<TransformDescriptorSetLayout> TransformDescriptorSetLayoutPtr;
typedef std::shared_ptr<DescriptorSet> DescriptorSetPtr;
typedef std::shared_ptr<GlobalDescriptorSet> GlobalDescriptorSetPtr;
typedef std::shared_ptr<TextureDescriptorSet> TextureDescriptorSetPtr;
typedef std::shared_ptr<TransformDescriptorSet> TransformDescriptorSetPtr;
typedef std::shared_ptr<Image> ImagePtr;
typedef std::shared_ptr<ImageView> ImageViewPtr;
typedef std::shared_ptr<TextureSampler> TextureSamplerPtr;
typedef std::shared_ptr<DepthBuffer> DepthBufferPtr;
typedef std::shared_ptr<Scene> ScenePtr;
typedef std::shared_ptr<Camera> CameraPtr;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    glm::vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, normal);


        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
    }
};

struct DirectionalLight {
    alignas(16) glm::vec3 direction = glm::vec3(0.0f, 0.0f, 0.0f);
    alignas(16) glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    alignas(4) float ka = 0.05f; // Ambient coefficient
    alignas(4) float kd = 0.5f; // Diffuse coefficient
    alignas(4) float ks = 0.3f; // Specular coefficient
    alignas(4) float intensity = 1.0f;
};

struct PointLight {
    alignas(16) glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    alignas(16) glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    alignas(4) float ka = 0.05f; // Ambient coefficient
    alignas(4) float kd = 0.5f; // Diffuse coefficient
    alignas(4) float ks = 0.3f; // Specular coefficient
    alignas(4) float intensity = 1.0f;
    alignas(4) float constant = 1.0f;
    alignas(4) float linear = 0.009f;
    alignas(4) float quadratic = 0.0032f;
};

struct GlobalUBO {
    //alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 cameraPosition;
    DirectionalLight directionalLight;
    alignas(16) PointLight pointLights[1024];
    alignas(4) int activePointLightCount;
};

struct ModelUBO {
    glm::mat4 model;
    float shininess;
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}
