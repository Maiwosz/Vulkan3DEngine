#include "LuaBindingsInput.h"
#include "Engine.h"
#include <GLFW/glfw3.h>

namespace LuaBindings {
    void registerInputSystem(sol::state& state) {
        // Register MouseButton enum
        state.new_enum<InputSystem::MouseButton>("MouseButton",
            {
                {"Left", InputSystem::MouseButton::Left},
                {"Right", InputSystem::MouseButton::Right},
                {"Middle", InputSystem::MouseButton::Middle}
            }
        );

        // Register KeyState enum
        state.new_enum<InputSystem::KeyState>("KeyState",
            {
                {"Pressed", InputSystem::KeyState::Pressed},
                {"Released", InputSystem::KeyState::Released},
                {"Repeated", InputSystem::KeyState::Repeated}
            }
        );

        // Create Input namespace in Lua
        auto inputNS = state.create_named_table("Input");

        // Register key codes as constants
        // GLFW key codes - common keys
        inputNS["KEY_SPACE"] = GLFW_KEY_SPACE;
        inputNS["KEY_APOSTROPHE"] = GLFW_KEY_APOSTROPHE;
        inputNS["KEY_COMMA"] = GLFW_KEY_COMMA;
        inputNS["KEY_MINUS"] = GLFW_KEY_MINUS;
        inputNS["KEY_PERIOD"] = GLFW_KEY_PERIOD;
        inputNS["KEY_SLASH"] = GLFW_KEY_SLASH;

        // Numbers
        for (int i = 0; i <= 9; i++) {
            inputNS["KEY_" + std::to_string(i)] = GLFW_KEY_0 + i;
        }

        // Letters
        for (int i = 0; i < 26; i++) {
            char letter = 'A' + i;
            std::string key = "KEY_";
            key += letter;
            inputNS[key] = GLFW_KEY_A + i;
        }

        // Function keys
        for (int i = 1; i <= 12; i++) {
            inputNS["KEY_F" + std::to_string(i)] = GLFW_KEY_F1 + (i - 1);
        }

        // Navigation keys
        inputNS["KEY_ESCAPE"] = GLFW_KEY_ESCAPE;
        inputNS["KEY_ENTER"] = GLFW_KEY_ENTER;
        inputNS["KEY_TAB"] = GLFW_KEY_TAB;
        inputNS["KEY_BACKSPACE"] = GLFW_KEY_BACKSPACE;
        inputNS["KEY_INSERT"] = GLFW_KEY_INSERT;
        inputNS["KEY_DELETE"] = GLFW_KEY_DELETE;
        inputNS["KEY_RIGHT"] = GLFW_KEY_RIGHT;
        inputNS["KEY_LEFT"] = GLFW_KEY_LEFT;
        inputNS["KEY_DOWN"] = GLFW_KEY_DOWN;
        inputNS["KEY_UP"] = GLFW_KEY_UP;
        inputNS["KEY_PAGE_UP"] = GLFW_KEY_PAGE_UP;
        inputNS["KEY_PAGE_DOWN"] = GLFW_KEY_PAGE_DOWN;
        inputNS["KEY_HOME"] = GLFW_KEY_HOME;
        inputNS["KEY_END"] = GLFW_KEY_END;

        // Modifier keys
        inputNS["KEY_CAPS_LOCK"] = GLFW_KEY_CAPS_LOCK;
        inputNS["KEY_SCROLL_LOCK"] = GLFW_KEY_SCROLL_LOCK;
        inputNS["KEY_NUM_LOCK"] = GLFW_KEY_NUM_LOCK;
        inputNS["KEY_PRINT_SCREEN"] = GLFW_KEY_PRINT_SCREEN;
        inputNS["KEY_PAUSE"] = GLFW_KEY_PAUSE;
        inputNS["KEY_LEFT_SHIFT"] = GLFW_KEY_LEFT_SHIFT;
        inputNS["KEY_LEFT_CONTROL"] = GLFW_KEY_LEFT_CONTROL;
        inputNS["KEY_LEFT_ALT"] = GLFW_KEY_LEFT_ALT;
        inputNS["KEY_LEFT_SUPER"] = GLFW_KEY_LEFT_SUPER;
        inputNS["KEY_RIGHT_SHIFT"] = GLFW_KEY_RIGHT_SHIFT;
        inputNS["KEY_RIGHT_CONTROL"] = GLFW_KEY_RIGHT_CONTROL;
        inputNS["KEY_RIGHT_ALT"] = GLFW_KEY_RIGHT_ALT;
        inputNS["KEY_RIGHT_SUPER"] = GLFW_KEY_RIGHT_SUPER;
        inputNS["KEY_MENU"] = GLFW_KEY_MENU;

        state.new_enum<InputSystem::CursorMode>("CursorMode",
            {
                {"Normal", InputSystem::CursorMode::Normal},
                {"Hidden", InputSystem::CursorMode::Hidden},
                {"Disabled", InputSystem::CursorMode::Disabled}
            }
        );

        // Register keyboard functions
        inputNS.set_function("isKeyPressed", [](int key) -> bool {
            return Engine::get().inputSystem().isKeyPressed(key);
            });

        inputNS.set_function("isKeyReleased", [](int key) -> bool {
            return Engine::get().inputSystem().isKeyReleased(key);
            });

        inputNS.set_function("isKeyHeld", [](int key) -> bool {
            return Engine::get().inputSystem().isKeyHeld(key);
            });

        inputNS.set_function("wasKeyPressed", [](int key) -> bool {
            return Engine::get().inputSystem().wasKeyPressed(key);
            });

        inputNS.set_function("wasKeyReleased", [](int key) -> bool {
            return Engine::get().inputSystem().wasKeyReleased(key);
            });

        // Register mouse functions
        inputNS.set_function("isMouseButtonPressed", [](InputSystem::MouseButton button) -> bool {
            return Engine::get().inputSystem().isMouseButtonPressed(button);
            });

        inputNS.set_function("isMouseButtonHeld", [](InputSystem::MouseButton button) -> bool {
            return Engine::get().inputSystem().isMouseButtonHeld(button);
            });

        inputNS.set_function("wasMouseButtonPressed", [](InputSystem::MouseButton button) -> bool {
            return Engine::get().inputSystem().wasMouseButtonPressed(button);
            });

        inputNS.set_function("wasMouseButtonReleased", [](InputSystem::MouseButton button) -> bool {
            return Engine::get().inputSystem().wasMouseButtonReleased(button);
            });

        inputNS.set_function("getMousePosition", []() -> glm::vec2 {
            return Engine::get().inputSystem().getMousePosition();
            });

        inputNS.set_function("getMouseDelta", []() -> glm::vec2 {
            return Engine::get().inputSystem().getMouseDelta();
            });

        inputNS.set_function("getMouseScrollDelta", []() -> float {
            return Engine::get().inputSystem().getMouseScrollDelta();
            });

        inputNS.set_function("setCursorMode", [](InputSystem::CursorMode mode) {
            Engine::get().inputSystem().setCursorMode(mode);
            });
    }
}