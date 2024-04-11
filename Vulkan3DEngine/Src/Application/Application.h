#pragma once
//#include "..\Prerequisites.h"
#include "..\GraphicsEngine\GraphicsEngine.h"
#include "..\WindowSytem\Window.h"
#include "..\GraphicsEngine\ResourceManager\ModelManager\Model.h"
#include <chrono>

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

	static float s_deltaTime;
private:
	WindowPtr m_window;

	ModelInstancePtr m_statue1;
	ModelInstancePtr m_statue2;
	ModelInstancePtr m_statue3;
	ModelInstancePtr m_statue4;
	ModelInstancePtr m_vikingRoom;
	ModelInstancePtr m_castle;
	ModelInstancePtr m_hygieia;
};