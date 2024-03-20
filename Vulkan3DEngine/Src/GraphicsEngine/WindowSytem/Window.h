#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Window
{
public:
	Window(int width, int height, const char* name);
	~Window();

	bool shouldClose();
	GLFWwindow* get() const { return m_window; } // Getter method for m_window
private:
	GLFWwindow* m_window;
};

