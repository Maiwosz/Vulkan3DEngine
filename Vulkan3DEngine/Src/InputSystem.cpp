#include "InputSystem.h"

InputSystem* InputSystem::m_inputSystem = nullptr;

InputSystem::InputSystem()
{
	p_window = Window::get()->getWindow();
	glfwSetKeyCallback(p_window, key_callback);
	glfwSetCursorPosCallback(p_window, cursor_position_callback);
	glfwSetMouseButtonCallback(p_window, mouse_button_callback);
}

InputSystem::~InputSystem()
{
	InputSystem::m_inputSystem = nullptr;
}

void InputSystem::update()
{
	double xpos, ypos;
	glfwGetCursorPos(p_window, &xpos, &ypos);
	glm::vec2 current_mouse_pos = glm::vec2(xpos, ypos);

	if (m_first_time)
	{
		m_old_mouse_pos = current_mouse_pos;
		m_first_time = false;
	}

	if (current_mouse_pos.x != m_old_mouse_pos.x || current_mouse_pos.y != m_old_mouse_pos.y)
	{
		//THERE IS MOUSE MOVE EVENT
		std::unordered_set<InputListener*>::iterator it = m_set_listeners.begin();

		while (it != m_set_listeners.end())
		{
			(*it)->onMouseMove(current_mouse_pos);
			++it;
		}
	}
	m_old_mouse_pos = current_mouse_pos;

	for (unsigned int i = GLFW_KEY_SPACE; i < GLFW_KEY_LAST; i++)
	{
		// KEY IS DOWN
		if (glfwGetKey(p_window, i) == GLFW_PRESS)
		{
			std::unordered_set<InputListener*>::iterator it = m_set_listeners.begin();

			while (it != m_set_listeners.end())
			{
				if (i == GLFW_MOUSE_BUTTON_LEFT)
				{
					if (m_keys_state[i] != m_old_keys_state[i])
					{
						(*it)->onLeftMouseDown(current_mouse_pos);
					}
				}
				else if (i == GLFW_MOUSE_BUTTON_RIGHT)
				{
					if (m_keys_state[i] != m_old_keys_state[i])
					{
						(*it)->onRightMouseDown(current_mouse_pos);
					}
				}
				else
					(*it)->onKeyDown(i);

				++it;
			}
		}
		else // KEY IS UP
		{
			if (m_keys_state[i] != m_old_keys_state[i])
			{
				std::unordered_set<InputListener*>::iterator it = m_set_listeners.begin();

				while (it != m_set_listeners.end())
				{
					if (i == GLFW_MOUSE_BUTTON_LEFT) {
						(*it)->onLeftMouseUp(current_mouse_pos);
					}
					else if (i == GLFW_MOUSE_BUTTON_RIGHT) {
						(*it)->onRightMouseUp(current_mouse_pos);
					}
					else {
						(*it)->onKeyUp(i);
					}
					++it;
				}
			}

		}

	}
	// store current keys state to old keys state buffer
	::memcpy(m_old_keys_state, m_keys_state, sizeof(unsigned char) * GLFW_KEY_LAST);
}

void InputSystem::addListener(InputListener* listener)
{
	m_set_listeners.insert(listener);
}

void InputSystem::removeListener(InputListener* listener)
{
	m_set_listeners.erase(listener);
}

void InputSystem::setCursorPosition(const glm::vec2& pos)
{
	glfwSetCursorPos(p_window, pos.x, pos.y);
}

void InputSystem::showCursor(bool show)
{
	if (show)
	{
		glfwSetInputMode(p_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else
	{
		glfwSetInputMode(p_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
}

InputSystem* InputSystem::get()
{
	return m_inputSystem;
}

void InputSystem::create()
{
	if (InputSystem::m_inputSystem) throw std::exception("InputSystem already created");
	InputSystem::m_inputSystem = new InputSystem();
}

void InputSystem::release()
{
	if (!InputSystem::m_inputSystem)
	{
		return;
	}
	delete InputSystem::m_inputSystem;
}
