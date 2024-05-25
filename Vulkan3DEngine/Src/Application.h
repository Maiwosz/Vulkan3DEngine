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
	
	static bool s_cursor_mode;
	static float s_deltaTime;
private:
	void drawFpsCounter();
	void drawExitPopup();
	void checkAndReloadResources();

	// Inherited via InputListener
	virtual void onKeyDown(int key) override;
	virtual void onKeyUp(int key) override;

	virtual void onMouseMove(const glm::vec2& mouse_pos) override;
	virtual void onLeftMouseDown(const glm::vec2& mouse_pos) override;
	virtual void onLeftMouseUp(const glm::vec2& mouse_pos) override;
	virtual void onRightMouseDown(const glm::vec2& mouse_pos) override;
	virtual void onRightMouseUp(const glm::vec2& mouse_pos) override;

	bool m_showExitPopup;
	std::atomic<bool> m_stopCheckingResources; // Flag to stop the resource checking thread
	std::thread m_checkResourcesThread; // Thread for resource checking

};