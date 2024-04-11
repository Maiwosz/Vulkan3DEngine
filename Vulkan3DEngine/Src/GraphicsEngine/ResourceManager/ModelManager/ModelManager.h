#pragma once
#include "../../../Prerequisites.h"
#include "../ResourceManager.h"
#include "Model.h"
#include <vector>

class ModelManager :public ResourceManager
{
public:
    ModelManager();
    ~ModelManager();

    ModelDataPtr loadModelData(const std::string& name);
    ModelInstancePtr createModelInstance(const std::string& name);

protected:
    virtual Resource* createResourceFromFileConcrete(const char* file_path) override;
};