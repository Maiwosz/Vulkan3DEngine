#pragma once
//#include "..\Prerequisites.h"
#include "..\GraphicsEngine\GraphicsEngine.h"
#include "..\WindowSytem\Window.h"
#include "..\InputSystem\InputListener.h"
#include "..\InputSystem\Inputsystem.h"
#include "..\GraphicsEngine\Scene\Scene.h"
#include <chrono>

class Application : public InputListener
{
public:
	Application();
	~Application();
	void run();
	void update();
	void draw();

	// Inherited via InputListener
	virtual void onKeyDown(int key) override;
	virtual void onKeyUp(int key) override;

	virtual void onMouseMove(const glm::vec2& mouse_pos) override;
	virtual void onLeftMouseDown(const glm::vec2& mouse_pos) override;
	virtual void onLeftMouseUp(const glm::vec2& mouse_pos) override;
	virtual void onRightMouseDown(const glm::vec2& mouse_pos) override;
	virtual void onRightMouseUp(const glm::vec2& mouse_pos) override;

	static constexpr uint32_t s_window_width = 1280;
	static constexpr uint32_t s_window_height = 720;

	static float s_deltaTime;
private:
	WindowPtr m_window;

	ScenePtr m_scene;
};