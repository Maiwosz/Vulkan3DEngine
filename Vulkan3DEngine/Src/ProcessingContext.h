#pragma once
#include <memory>
#include <string>
#include <mutex>
#include "CameraRenderOrder.h"
#include "LightRenderOrder.h"
#include "MeshRenderOrder.h"
#include "SemaphoreManager.h"

class ProcessingContext {
public:
    ProcessingContext(SemaphoreManager& semaphoreManager);
    ~ProcessingContext() = default;

    // Non-copyable, non-movable for thread safety
    ProcessingContext(const ProcessingContext&) = delete;
    ProcessingContext& operator=(const ProcessingContext&) = delete;
    ProcessingContext(ProcessingContext&&) = delete;
    ProcessingContext& operator=(ProcessingContext&&) = delete;

    // Camera data - processed cameras ready for rendering
    void addProcessedCamera(std::shared_ptr<CameraRenderOrder> camera);
    const std::vector<std::shared_ptr<CameraRenderOrder>>& getProcessedCameras() const;
    size_t getProcessedCameraCount() const;

    // Light data - for use by camera stages
    void addLight(std::shared_ptr<LightRenderOrder> light);
    const std::vector<std::shared_ptr<LightRenderOrder>>& getLights() const;
    size_t getLightCount() const;

    // Mesh data - processed meshes ready for culling by cameras
    void addProcessedMesh(std::shared_ptr<MeshRenderOrder> mesh);
    const std::vector<std::shared_ptr<MeshRenderOrder>>& getProcessedMeshes() const;
    size_t getProcessedMeshCount() const;

    // Access to semaphore manager - used by processing stages
    SemaphoreManager& getSemaphoreManager() { return m_semaphoreManager; }
    const SemaphoreManager& getSemaphoreManager() const { return m_semaphoreManager; }

    // Frame lifecycle
    void reset();

private:
    mutable std::mutex m_mutex;
    SemaphoreManager& m_semaphoreManager;

    // Processed render orders ready for use
    std::vector<std::shared_ptr<CameraRenderOrder>> m_processedCameras;
    std::vector<std::shared_ptr<LightRenderOrder>> m_lights;
    std::vector<std::shared_ptr<MeshRenderOrder>> m_processedMeshes;
};