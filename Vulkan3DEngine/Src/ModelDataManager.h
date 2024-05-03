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

    ModelDataPtr loadModelData(const std::string& name);
protected:
    virtual Resource* createResourceFromFileConcrete(const char* file_path) override;
};