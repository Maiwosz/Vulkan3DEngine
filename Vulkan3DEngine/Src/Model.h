#pragma once
#include "Prerequisites.h"
#include "SceneObject.h"
#include "Descriptors.h"
#include <mutex>

class Model : public SceneObject
{
public:
    Model(ModelDataPtr modelData, Scene* scene);
    Model(std::string name, std::string meshName, std::string textureName, float scale, float shininess, float kd, float ks,
        float scaleOffset, glm::vec3 positionOffset, glm::vec3 rotationOffset, Scene* scene);
    Model(const nlohmann::json& j, Scene* scene);
    ~Model();

    void update() override;
    void draw() override;
    void saveModelData();
    void writeModelDataToFile(const std::filesystem::path& full_path);

    void setMesh(MeshPtr mesh);
    void setTexture(TexturePtr texture);

    float getScaleOffset();
    glm::vec3 getPositionOffset();
    glm::vec3 getRotationOffset();

    void setScaleOffset(float scale);
    void setPositionOffset(glm::vec3 position);
    void setRotationOffset(glm::vec3 rotation);

    void to_json(nlohmann::json& j) override;
    void from_json(const nlohmann::json& j) override;
private:
    MeshPtr m_mesh;
    TexturePtr m_texture;

    float m_shininess = 1.0f;
    float m_kd = 0.8f;
    float m_ks = 0.2f;

    glm::mat4 m_scaleOffset;
    glm::mat4 m_positionOffset;
    glm::mat4 m_rotationOffset;

    std::vector<UniformBufferPtr> m_uniformBuffers;

    static int m_modelCount;
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
