#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include "Application.h"

int main() {
	Application app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << "Caught exception: " << e.what() << '\n';
		
		if (ThreadPool::get()) {
			ThreadPool::release();
		}
		if (InputSystem::get()) {
			InputSystem::release();
		}
		if (GraphicsEngine::get()) {
			GraphicsEngine::release();
		}
		
		return EXIT_FAILURE;
	}
	catch (...) {
		std::cerr << "Caught unknown exception\n";
	}

	return EXIT_SUCCESS;
}