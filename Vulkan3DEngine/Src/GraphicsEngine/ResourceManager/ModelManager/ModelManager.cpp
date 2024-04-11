#include "ModelManager.h"

ModelManager::ModelManager()
    : ResourceManager("Assets\\Models")
{
}

ModelManager::~ModelManager()
{
}

ModelDataPtr ModelManager::loadModelData(const std::string& name)
{
    return std::static_pointer_cast<ModelData>(loadResource(name));
}

ModelInstancePtr ModelManager::createModelInstance(const std::string& name)
{
    return std::make_shared<ModelInstance>(loadModelData(name));
}

Resource* ModelManager::createResourceFromFileConcrete(const char* file_path)
{
    ModelData* modelData = nullptr;
    try
    {
        modelData = new ModelData(file_path);
    }
    catch (...) {}

    return modelData;
}