#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class WindowSystem
{
public:
	WindowSystem(int width, int height, const char* name);
	~WindowSystem();

	bool shouldClose();
private:
	
	GLFWwindow* window;
};

