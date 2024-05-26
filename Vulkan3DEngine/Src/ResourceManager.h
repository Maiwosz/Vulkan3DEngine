#pragma once
#include "Prerequisites.h"
#include "Resource.h"
#include <unordered_map>
#include <string>
#include <filesystem>
#include <chrono>
#include <shared_mutex>
#include <mutex>
#include <set>

class ResourceManager
{
public:
    ResourceManager(const std::filesystem::path& directory);
    virtual ~ResourceManager();
    ResourcePtr loadResource(const std::filesystem::path& name);
    void unloadResource(const std::filesystem::path& name);
    void updateResources();
    std::set<std::filesystem::path> getAllResources() {
        std::shared_lock lock(m_all_resources_mutex);
        return m_all_resources;
    }
    void reloadAllResources();
protected:
    virtual Resource* createResourceFromFileConcrete(const std::filesystem::path& file_path) = 0;

private:
    void updateResourceList();

    std::filesystem::path m_directory;
    std::unordered_map<std::filesystem::path, ResourcePtr> m_map_resources;
    std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> m_last_write_time;
    std::set<std::filesystem::path> m_all_resources;

    std::shared_mutex m_all_resources_mutex;
    std::mutex m_map_resources_mutex;
    std::unordered_map<std::filesystem::path, std::mutex> m_individual_resource_mutexes;
};
