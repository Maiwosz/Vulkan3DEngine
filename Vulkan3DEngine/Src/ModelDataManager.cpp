#include "ModelDataManager.h"

ModelDataManager::ModelDataManager()
    : ResourceManager("Assets/Models")
{
}

ModelDataManager::~ModelDataManager()
{
}

ModelDataPtr ModelDataManager::loadModelData(const std::filesystem::path& name)
{
    return std::static_pointer_cast<ModelData>(loadResource(name));
}

Resource* ModelDataManager::createResourceFromFileConcrete(const std::filesystem::path& file_path)
{
    ModelData* modelData = nullptr;
    try
    {
        modelData = new ModelData(file_path);
    }
    catch (...) {}

    return modelData;
}