#pragma once
#include "Prerequisites.h"
#include "ResourceManager.h"
#include "ModelData.h"
#include <vector>

class ModelDataManager :public ResourceManager
{
public:
    ModelDataManager();
    ~ModelDataManager();

    ModelDataPtr loadModelData(const std::filesystem::path& name);
protected:
    virtual Resource* createResourceFromFileConcrete(const std::filesystem::path& file_path) override;
};