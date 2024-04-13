#pragma once
#include "InputListener.h"
#include <unordered_set>

class InputSystem
{
private:
	InputSystem();
	~InputSystem();
public:
	void update();
	void addListener(InputListener* listener);
	void removeListener(InputListener* listener);

	void setCursorPosition(const glm::vec2& pos);
	void showCursor(bool show);

	glm::vec2 getOldMousePos() { return m_old_mouse_pos; }

	static InputSystem* get();
	static void create();
	static void release();
private:
	std::unordered_set<InputListener*> m_set_listeners;
	unsigned char m_keys_state[256] = {};
	unsigned char m_old_keys_state[256] = {};
	glm::vec2 m_old_mouse_pos;
	bool m_first_time = true;
	static InputSystem* m_inputSystem;
};

