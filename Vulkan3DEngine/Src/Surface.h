#pragma once
#include <vulkan/vulkan.h>
#include "Window.h"

class Surface {
public:
    Surface(VkInstance instance, const Window& window);
    ~Surface();

    // Usuwamy kopiowanie
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    // Przenoszenie
    Surface(Surface&& other) noexcept;
    Surface& operator=(Surface&& other) noexcept;

    VkSurfaceKHR get() const { return m_surface; }

private:
    VkInstance m_instance;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
};