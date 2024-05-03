#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

class Window
{
public:
	Window(int width, int height, const char* windowName);
	~Window();

	GLFWwindow* get() const { return m_window; } // Getter method for m_window

	bool shouldClose() { return glfwWindowShouldClose(m_window); }
	VkExtent2D getExtent() { return { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height) }; }

	bool isFocused() { return glfwGetWindowAttrib(m_window, GLFW_FOCUSED); }
	void resetWindowFocusedFlag() { m_focused = true; }

	bool isMinimized() { return glfwGetWindowAttrib(m_window, GLFW_ICONIFIED); }
	void resetWindowMinimizedFlag() { m_minimized = false; }

	bool wasWindowResized() { return m_framebufferResized; }
	void resetWindowResizedFlag() { m_framebufferResized = false; }

	
private:
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void windowFocusCallback(GLFWwindow* window, int focused);
	static void windowIconifyCallback(GLFWwindow* window, int iconified);

	GLFWwindow* m_window;

	int m_width;
	int m_height;
	std::string m_windowName;
	bool m_framebufferResized = false;
	bool m_focused = true;
	bool m_minimized = false;
};

