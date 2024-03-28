#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

class Window
{
public:
	Window(int width, int height, const char* windowName);
	~Window();

	bool shouldClose();
	VkExtent2D getExtent() { return { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height) }; }
	bool wasWindowResized() { return m_framebufferResized; }
	void resetWindowResizedFlag() { m_framebufferResized = false; }
	GLFWwindow* get() const { return m_window; } // Getter method for m_window
private:
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

	GLFWwindow* m_window;

	int m_width;
	int m_height;
	std::string m_windowName;
	bool m_framebufferResized = false;
};

