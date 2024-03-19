#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include "Application.h"

int main() {
	try
	{
		GraphicsEngine::create();
	}
	catch (...) { return -1; }

	Application app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		GraphicsEngine::release();  // Release before returning
		return EXIT_FAILURE;
	}

	GraphicsEngine::release();  // Release before returning
	return EXIT_SUCCESS;
}