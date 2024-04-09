#pragma once
#include "../../Prerequisites.h"
#include "Resource.h"
#include <unordered_map>
#include <string>
#include <filesystem>
#include <chrono>

class ResourceManager
{
public:
    ResourceManager(const std::string& directory);
    virtual ~ResourceManager();

    ResourcePtr loadResource(const std::string& name);
    void unloadResource(const std::string& name);
    void updateResources();

protected:
    virtual Resource* createResourceFromFileConcrete(const char* file_path) = 0;

private:
    void updateResourceList();

    std::string m_directory;
    std::unordered_map<std::string, ResourcePtr> m_map_resources;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_last_write_time;
};