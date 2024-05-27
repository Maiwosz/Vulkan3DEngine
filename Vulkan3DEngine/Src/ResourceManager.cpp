#include "ResourceManager.h"
#include <filesystem>
#include <iostream>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <ranges>
#include <thread>
#include <chrono>
#include "ThreadPool.h"

bool isFileStable(const std::filesystem::path& path, const std::chrono::milliseconds& waitTime = std::chrono::milliseconds(100), int retries = 5)
{
    try
    {
        auto initialSize = std::filesystem::file_size(path);
        fmt::print("Initial file size for '{}': {}\n", path.string(), initialSize);

        for (int i = 0; i < retries; ++i)
        {
            std::this_thread::sleep_for(waitTime);

            auto newSize = std::filesystem::file_size(path);
            fmt::print("Retry {}: New file size for '{}': {}\n", i + 1, path.string(), newSize);

            if (newSize == initialSize)
            {
                fmt::print("File size is stable after {} retries.\n", i + 1);
                return true; // File size is stable
            }

            initialSize = newSize;
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        fmt::print(stderr, "Error checking file stability '{}': {}\n", path.string(), e.what());
        return false;
    }

    fmt::print("File size changed during all retries. File is still being written.\n");
    return false; // File size changed, so it's still being written
}


ResourceManager::ResourceManager(const std::filesystem::path& directory)
    : m_directory(directory)
{
    try
    {
        updateResourceList();
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        fmt::print(stderr, "Error initializing ResourceManager: {}\n", e.what());
    }
}

ResourceManager::~ResourceManager() {}

ResourcePtr ResourceManager::loadResource(const std::filesystem::path& name)
{
    fmt::print("Attempting to load resource: '{}'\n", name.string());
    //fmt::print("Attempting to acquire lock for resource: '{}'\n", name.string());
    std::lock_guard lock(m_individual_resource_mutexes[name]);
    //fmt::print("Lock acquired for resource: '{}'\n", name.string());

    fmt::print("Loading resource: '{}'\n", name.string());

    {
        //fmt::print("Attempting to acquire lock for resource map\n");
        std::lock_guard  lock(m_map_resources_mutex);
        //fmt::print("Lock acquired for resource map\n");

        if (auto it = m_map_resources.find(name); it != m_map_resources.end() && it->second != nullptr)
        {
            fmt::print("Resource '{}' found in cache\n", name.string());
            return it->second;
        }
    }

    std::filesystem::path full_path = std::filesystem::absolute(m_directory / name);
    //fmt::print("Full path for resource '{}': '{}'\n", name.string(), full_path.string());

    if (!std::filesystem::exists(full_path))
    {
        fmt::print(stderr, "Error: Resource '{}' not found in directory '{}'\n", name.string(), m_directory.string());
        return nullptr;
    }

    Resource* raw_res = nullptr;
    try
    {
        fmt::print("Attempting to create resource from file '{}'\n", full_path.string());
        raw_res = createResourceFromFileConcrete(full_path);
        fmt::print("Resource created from file '{}'\n", full_path.string());
    }
    catch (const std::exception& e)
    {
        fmt::print(stderr, "Error loading resource '{}': {}\n", full_path.string(), e.what());
        return nullptr;
    }

    if (raw_res)
    {
        ResourcePtr res(raw_res);
        {
            //fmt::print("Attempting to acquire lock for resource map\n");
            std::unique_lock lock(m_map_resources_mutex);
            //fmt::print("Lock acquired for resource map\n");

            m_map_resources[name] = res;
            m_last_write_time[name] = std::filesystem::last_write_time(full_path);
        }
        fmt::print("Resource '{}' has been successfully loaded\n", name.string()); // Dodana wiadomoœæ
        return res;
    }
    else
    {
        fmt::print(stderr, "Error: Failed to create resource from file '{}'\n", full_path.string());
        return nullptr;
    }
}



void ResourceManager::unloadResource(const std::filesystem::path& name)
{
    std::lock_guard lock(m_individual_resource_mutexes[name]);
    fmt::print("Unloading resource: '{}'\n", name.string());

    {
        std::unique_lock lock(m_map_resources_mutex);
        if (m_map_resources.erase(name) == 0)
        {
            fmt::print(stderr, "Warning: Tried to unload non-existing resource '{}'\n", name.string());
        }
        m_last_write_time.erase(name);
    }
}

void ResourceManager::updateResources()
{
    std::shared_lock all_resources_lock(m_all_resources_mutex);

    for (const auto& [name, resource] : m_map_resources)
    {
        if (resource) // Sprawdzanie tylko za³adowanych zasobów
        {
            std::lock_guard resource_lock(m_individual_resource_mutexes[name]);
            std::filesystem::path full_path = std::filesystem::absolute(m_directory / name);

            if (!std::filesystem::exists(full_path))
            {
                fmt::print("Resource '{}' has been removed\n", name.string());
                std::unique_lock map_lock(m_map_resources_mutex);
                m_map_resources.erase(name);
                m_last_write_time.erase(name);
            }
            else
            {
                try
                {
                    auto last_write_time = std::filesystem::last_write_time(full_path);

                    if (last_write_time != m_last_write_time[name])
                    {
                        if (isFileStable(full_path))
                        {
                            std::unique_lock map_lock(m_map_resources_mutex);
                            if (m_map_resources[name])
                            {
                                fmt::print("Reloading resource '{}'\n", name.string());
                                m_map_resources[name]->Reload();
                                m_last_write_time[name] = last_write_time;
                                fmt::print("Resource has been reloaded: '{}', Path: '{}'\n", name.string(), full_path.string());
                            }
                        }
                        else
                        {
                            fmt::print("Resource '{}' is still being written. Skipping reload.\n", name.string());
                        }
                    }
                }
                catch (const std::filesystem::filesystem_error& e)
                {
                    fmt::print(stderr, "Error updating resource '{}': {}\n", full_path.string(), e.what());
                }
            }
        }
    }

    all_resources_lock.unlock();
    updateResourceList();
}

void ResourceManager::reloadAllResources()
{
    std::vector<std::future<void>> futures;
    {
        std::lock_guard<std::mutex> lock(m_map_resources_mutex);
        for (auto& [path, resource] : m_map_resources) {
            if (resource) {
                futures.push_back(ThreadPool::get()->enqueue([resource]() -> void {
                    resource->Reload();
                    }));
            }
        }
    }

    for (auto& future : futures) {
        future.get();
    }
}



void ResourceManager::updateResourceList()
{
    // Lista rozszerzeñ plików do zignorowania
    std::set<std::string> ignoredExtensions = { ".tmp", ".bak" };

    //fmt::print("Updating resource list...\n");
    std::unique_lock lock(m_all_resources_mutex);

    try
    {
        auto entries = std::filesystem::directory_iterator(m_directory);
        auto files = entries
            | std::views::filter([&](const auto& entry) {
                // Sprawdzanie, czy rozszerzenie pliku jest na liœcie ignorowanych (z kropk¹)
                auto extension = entry.path().extension().string(); // Pobranie pe³nej nazwy rozszerzenia
                return entry.is_regular_file() && ignoredExtensions.find(extension) == ignoredExtensions.end();
                })
            | std::views::transform([](const auto& entry) { return entry.path().filename(); });

        for (const auto& name : files)
        {
            //fmt::print("Adding resource '{}' to the list\n", name.string());
            m_all_resources.insert(name);

            std::unique_lock map_lock(m_map_resources_mutex);
            if (m_map_resources.find(name) == m_map_resources.end())
            {
                m_map_resources[name] = nullptr;
                m_last_write_time[name] = std::filesystem::last_write_time(m_directory / name);
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        fmt::print(stderr, "Error updating resource list: {}\n", e.what());
    }
}
