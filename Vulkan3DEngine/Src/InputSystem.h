#pragma once
#include "Prerequisites.h"
#include "Window.h"
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
	GLFWwindow* p_window = nullptr;
	std::unordered_set<InputListener*> m_set_listeners;
	unsigned char m_keys_state[GLFW_KEY_LAST] = {};
	unsigned char m_old_keys_state[GLFW_KEY_LAST] = {};
	glm::vec2 m_old_mouse_pos;
	bool m_first_time = true;
	static InputSystem* m_inputSystem;

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        InputSystem* inputSystem = InputSystem::get();
        if (action == GLFW_PRESS) {
            for (InputListener* listener : inputSystem->m_set_listeners) {
                listener->onKeyDown(key);
            }
        }
        else if (action == GLFW_RELEASE) {
            for (InputListener* listener : inputSystem->m_set_listeners) {
                listener->onKeyUp(key);
            }
        }
    }

    static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
    {
        glm::vec2 pos(xpos, ypos);
        InputSystem* inputSystem = InputSystem::get();
        for (InputListener* listener : inputSystem->m_set_listeners) {
            listener->onMouseMove(pos);
        }
    }

    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        glm::vec2 pos(xpos, ypos);
        InputSystem* inputSystem = InputSystem::get();
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                for (InputListener* listener : inputSystem->m_set_listeners) {
                    listener->onLeftMouseDown(pos);
                }
            }
            else if (action == GLFW_RELEASE) {
                for (InputListener* listener : inputSystem->m_set_listeners) {
                    listener->onLeftMouseUp(pos);
                }
            }
        }
        else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                for (InputListener* listener : inputSystem->m_set_listeners) {
                    listener->onRightMouseDown(pos);
                }
            }
            else if (action == GLFW_RELEASE) {
                for (InputListener* listener : inputSystem->m_set_listeners) {
                    listener->onRightMouseUp(pos);
                }
            }
        }
    }
};

