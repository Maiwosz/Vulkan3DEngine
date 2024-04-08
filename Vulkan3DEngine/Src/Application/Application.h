#pragma once
//#include "..\Prerequisites.h"
#include "..\GraphicsEngine\GraphicsEngine.h"
#include "..\WindowSytem\Window.h"
#include "..\Object\Object.h"
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

	ObjectPtr m_statue;
	MeshPtr m_statue_mesh;
	TexturePtr m_statue_texture;

	ObjectPtr m_vikingRoom;
	MeshPtr m_vikingRoom_mesh;
	TexturePtr m_vikingRoom_texture;

	ObjectPtr m_castle;
	MeshPtr m_castle_mesh;
	TexturePtr m_castle_texture;

	ObjectPtr m_hygieia;
	MeshPtr m_hygieia_mesh;
	TexturePtr m_hygieia_texture;

	//ObjectPtr rect;
	//MeshPtr rect_mesh;
	//TexturePtr m_cat_texture;
};