#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Prerequisites.h"
#include <string>
#include <map>

class Window
{
public:
	enum class Mode {
		Windowed,
		Fullscreen,
		Borderless
	};
	enum class Resolution {
		// Format 4:3
		R_640x480,
		R_800x600,
		R_1024x768,

		// Format 16:9
		R_1280x720,
		R_1366x768,
		R_1600x900,
		R_1920x1080
	};

	struct ResolutionDetails {
		int width;
		int height;
	};

	static const std::map<Resolution, ResolutionDetails> resolutionMap;

	Window(Resolution resolution, const char* windowName, Mode mode);
	~Window();

	GLFWwindow* get() const { return m_window; } // Getter method for m_window
	VkSurfaceKHR& getSurface() { return m_surface; }
	void createSurface();

	bool shouldClose() { return glfwWindowShouldClose(m_window); }
	VkExtent2D getExtent() {
		ResolutionDetails resolutionDetails = resolutionMap.at(m_resolution);
		return { static_cast<uint32_t>(resolutionDetails.width), static_cast<uint32_t>(resolutionDetails.height) };
	}

	bool isFocused() { return glfwGetWindowAttrib(m_window, GLFW_FOCUSED); }
	void resetWindowFocusedFlag() { m_focused = true; }

	bool isMinimized() { return glfwGetWindowAttrib(m_window, GLFW_ICONIFIED); }
	void resetWindowMinimizedFlag() { m_minimized = false; }

	bool wasWindowResized() { return m_framebufferResized; }
	void resetWindowResizedFlag() { m_framebufferResized = false; }

	void setMode(Mode mode);
private:
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void windowFocusCallback(GLFWwindow* window, int focused);
	static void windowIconifyCallback(GLFWwindow* window, int iconified);

	GLFWwindow* m_window;
	VkSurfaceKHR m_surface;
	Mode m_mode;
	Resolution m_resolution;
	std::string m_windowName;
	bool m_framebufferResized = false;
	bool m_focused = true;
	bool m_minimized = false;
};

