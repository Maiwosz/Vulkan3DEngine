#pragma once
#include "ThreadPool.h"
#include "InputSystem.h"
#include "GraphicsEngine.h"
#include "Window.h"
#include "Scene.h"
#include <chrono>

class Application
{
public:
	Application();
	~Application();
	void run();
	void update();
	void draw();

	//static uint32_t s_window_width;
	//static uint32_t s_window_height;

	static float s_deltaTime;
private:
	//WindowPtr m_window;
	ScenePtr m_scene;
};