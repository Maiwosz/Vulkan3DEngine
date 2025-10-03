#include "ProcessingContext.h"
#include <spdlog/spdlog.h>

ProcessingContext::ProcessingContext(SemaphoreManager& semaphoreManager)
    : m_semaphoreManager(semaphoreManager) {
    SPDLOG_DEBUG("Created ProcessingContext with SemaphoreManager");
}

void ProcessingContext::addProcessedCamera(std::shared_ptr<CameraRenderOrder> camera) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_processedCameras.push_back(camera);
    SPDLOG_DEBUG("Added processed camera to context, entity: {}", camera->entity.id);
}

const std::vector<std::shared_ptr<CameraRenderOrder>>& ProcessingContext::getProcessedCameras() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_processedCameras;
}

size_t ProcessingContext::getProcessedCameraCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_processedCameras.size();
}

void ProcessingContext::addLight(std::shared_ptr<LightRenderOrder> light) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lights.push_back(light);
    SPDLOG_DEBUG("Added light to context, entity: {}", light->entity.id);
}

const std::vector<std::shared_ptr<LightRenderOrder>>& ProcessingContext::getLights() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lights;
}

size_t ProcessingContext::getLightCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lights.size();
}

void ProcessingContext::addProcessedMesh(std::shared_ptr<MeshRenderOrder> mesh) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_processedMeshes.push_back(mesh);
    SPDLOG_DEBUG("Added processed mesh to context, entity: {}", mesh->entity.id);
}

const std::vector<std::shared_ptr<MeshRenderOrder>>& ProcessingContext::getProcessedMeshes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_processedMeshes;
}

size_t ProcessingContext::getProcessedMeshCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_processedMeshes.size();
}

void ProcessingContext::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_processedCameras.clear();
    m_lights.clear();
    m_processedMeshes.clear();

    SPDLOG_DEBUG("ProcessingContext reset for next frame");
}