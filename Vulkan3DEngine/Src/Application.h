#pragma once
#include "ThreadPool.h"
#include "InputSystem.h"
#include "GraphicsEngine.h"
#include "Window.h"
#include "Scene.h"
#include <chrono>

class Application : public InputListener
{
public:
	Application();
	~Application();
	void run();
	void update();
	void draw();
	void drawUI();

	static bool s_cursor_mode;
	static float s_deltaTime;
private:
	// Inherited via InputListener
	virtual void onKeyDown(int key) override;
	virtual void onKeyUp(int key) override;

	virtual void onMouseMove(const glm::vec2& mouse_pos) override;
	virtual void onLeftMouseDown(const glm::vec2& mouse_pos) override;
	virtual void onLeftMouseUp(const glm::vec2& mouse_pos) override;
	virtual void onRightMouseDown(const glm::vec2& mouse_pos) override;
	virtual void onRightMouseUp(const glm::vec2& mouse_pos) override;

	ScenePtr m_scene;
};