#pragma once
#include "Prerequisites.h"
#include "SceneObject.h"
#include "Descriptors.h"
#include <mutex>

class Model : public SceneObject
{
public:
    Model(ModelDataPtr modelData, Scene* scene);
    ~Model();

    void update() override;
    void draw() override;
    void saveModelData();
    void writeModelDataToFile(const std::filesystem::path& full_path);

    void setMesh(MeshPtr mesh);
    void setTexture(TexturePtr texture);

private:
    MeshPtr m_mesh;
    TexturePtr m_texture;

    //std::string m_name;
    float m_scale = 1.0f;
    float m_shininess = 1.0f;
    float m_kd = 0.8f;
    float m_ks = 0.2f;

    glm::mat4 m_initialScale;
    glm::mat4 m_initialPosition;
    glm::mat4 m_initialRotation;

    std::vector<UniformBufferPtr> m_uniformBuffers;

    static bool m_descriptorAllocatorInitialized;
    static DescriptorAllocatorGrowable m_descriptorAllocator;

    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> m_sizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
    };

    std::vector<VkDescriptorSet> m_descriptorSets;

    ModelUBO ubo{};

    std::mutex m_mutex; // Add a mutex for thread safety

    friend class SceneObjectManager;
    friend class Application;
    friend class Renderer;
};
