#include "Surface.h"
#include <stdexcept>

Surface::Surface(VkInstance instance, const Window& window)
    : m_instance(instance) {
    if (glfwCreateWindowSurface(instance, window.get(), nullptr, &m_surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
}

Surface::~Surface() {
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }
}

Surface::Surface(Surface&& other) noexcept
    : m_instance(other.m_instance), m_surface(other.m_surface) {
    other.m_surface = VK_NULL_HANDLE;
}

Surface& Surface::operator=(Surface&& other) noexcept {
    if (this != &other) {
        m_instance = other.m_instance;
        m_surface = other.m_surface;
        other.m_surface = VK_NULL_HANDLE;
    }
    return *this;
}