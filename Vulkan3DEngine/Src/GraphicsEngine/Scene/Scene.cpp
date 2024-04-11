#include "Scene.h"
#include "../ResourceManager/ModelManager/ModelManager.h"

Scene::Scene() : m_modelManager(std::make_shared<ModelManager>())
{
}