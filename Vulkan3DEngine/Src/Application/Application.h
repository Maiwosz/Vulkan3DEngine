#pragma once
#include "..\GraphicsEngine\GraphicsEngine.h"
#include "..\GraphicsEngine\WindowSytem\Window.h"

class Application
{
public:
	Application();
	~Application();
	void run();

	static constexpr  uint32_t s_window_width = 800;
	static constexpr  uint32_t s_window_height = 600;
private:
	std::unique_ptr<WindowSystem> m_window;
};