#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include "Application\Application.h"

int main() {
	Application app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << "Caught exception: " << e.what() << '\n';
		return EXIT_FAILURE;
	}
	catch (...) {
		std::cerr << "Caught unknown exception\n";
	}

	return EXIT_SUCCESS;
}