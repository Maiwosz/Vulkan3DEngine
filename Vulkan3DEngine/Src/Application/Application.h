#pragma once
//#include "..\Prerequisites.h"
#include "..\GraphicsEngine\GraphicsEngine.h"
#include "..\WindowSytem\Window.h"
#include "..\InputSystem\InputListener.h"
#include "..\GraphicsEngine\Scene\Scene.h"
#include <chrono>

class Application
{
public:
	Application();
	~Application();
	void run();
	void update();
	void draw();

	static uint32_t s_window_width;
	static uint32_t s_window_height;

	static float s_deltaTime;
private:
	WindowPtr m_window;
	ScenePtr m_scene;
};