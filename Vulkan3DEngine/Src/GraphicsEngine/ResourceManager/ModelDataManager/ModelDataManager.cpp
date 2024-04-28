#include "ModelDataManager.h"

ModelDataManager::ModelDataManager()
    : ResourceManager("Assets\\Models")
{
}

ModelDataManager::~ModelDataManager()
{
}

ModelDataPtr ModelDataManager::loadModelData(const std::string& name)
{
    return std::static_pointer_cast<ModelData>(loadResource(name));
}

Resource* ModelDataManager::createResourceFromFileConcrete(const char* file_path)
{
    ModelData* modelData = nullptr;
    try
    {
        modelData = new ModelData(file_path);
    }
    catch (...) {}

    return modelData;
}