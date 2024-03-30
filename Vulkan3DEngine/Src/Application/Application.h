#pragma once
#include "..\Prerequisites.h"
#include "..\GraphicsEngine\GraphicsEngine.h"
#include "..\WindowSytem\Window.h"
#include "..\Object\Object.h"

class Application
{
public:
	Application();
	~Application();
	void run();
	void update();
	void draw();

	static constexpr uint32_t s_window_width = 1280;
	static constexpr uint32_t s_window_height = 720;
private:
	WindowPtr m_window;

	Object* triangle;
};