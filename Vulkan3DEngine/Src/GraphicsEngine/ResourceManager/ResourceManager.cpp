#include "ResourceManager.h"
#include <filesystem>
#include <iostream>

ResourceManager::ResourceManager(const std::string& directory)
    : m_directory(directory)
{
    updateResourceList();
}

ResourceManager::~ResourceManager() {}

ResourcePtr ResourceManager::loadResource(const std::string& name)
{
    auto it = m_map_resources.find(name);
    if (it != m_map_resources.end())
    {
        return it->second;
    }

    std::string full_path = m_directory + "/" + name;
    if (!std::filesystem::exists(full_path))
    {
        return nullptr;
    }

    Resource* raw_res = createResourceFromFileConcrete(full_path.c_str());
    if (raw_res)
    {
        ResourcePtr res(raw_res);
        m_map_resources[name] = res;
        m_last_write_time[name] = std::filesystem::last_write_time(full_path);
        return res;
    }

    return nullptr;
}

void ResourceManager::unloadResource(const std::string& name)
{
    m_map_resources.erase(name);
    m_last_write_time.erase(name);
}

void ResourceManager::updateResources()
{
    for (auto it = m_map_resources.begin(); it != m_map_resources.end();)
    {
        std::string full_path = m_directory + "/" + it->first;
        if (!std::filesystem::exists(full_path))
        {
            it = m_map_resources.erase(it);
        }
        else
        {
            auto last_write_time = std::filesystem::last_write_time(full_path);
            if (last_write_time != m_last_write_time[it->first])
            {
                // If the file has been updated, call the Reload method
                it->second->Reload();
                m_last_write_time[it->first] = last_write_time;

                // Print a message to the console
                std::cout << "Resource has been reloaded: " << it->first << ", Path: " << full_path << std::endl;
            }
            ++it;
        }
    }

    updateResourceList();
}


void ResourceManager::updateResourceList()
{
    for (const auto& entry : std::filesystem::directory_iterator(m_directory))
    {
        if (entry.is_regular_file())
        {
            std::string name = entry.path().filename().replace_extension().string();
            if (m_map_resources.find(name) == m_map_resources.end())
            {
                m_map_resources[name] = nullptr;
                m_last_write_time[name] = std::filesystem::last_write_time(entry.path());
            }
        }
    }
}