#pragma once
#include "..\GraphicsEngine\GraphicsEngine.h"
#include "..\GraphicsEngine\WindowSytem\WindowSystem.h"

class Application
{
public:
	Application();
	~Application();
	void run();

	const uint32_t m_window_width = 800;
	const uint32_t m_window_height = 600;
private:
	std::unique_ptr<WindowSystem> m_window;
};