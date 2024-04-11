#pragma once
#include "../Prerequisites.h"

class InputListener
{
public:
	InputListener()
	{

	}
	~InputListener()
	{

	}
	//KEYBOARD pure virtual callback functions
	virtual void onKeyDown(int key) = 0;
	virtual void onKeyUp(int key) = 0;

	//MOUSE pure virtual callback functions
	virtual void onMouseMove(const glm::vec2& mouse_pos) = 0;
	virtual void onLeftMouseDown(const glm::vec2& mouse_pos) = 0;
	virtual void onLeftMouseUp(const glm::vec2& mouse_pos) = 0;
	virtual void onRightMouseDown(const glm::vec2& mouse_pos) = 0;
	virtual void onRightMouseUp(const glm::vec2& mouse_pos) = 0;
};