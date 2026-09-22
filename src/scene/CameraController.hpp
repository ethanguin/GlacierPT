#include "Camera.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

class CameraController {
public:
    void update(GLFWwindow* window, Camera& camera, float deltaTime) {
        handleMouseLook(window, camera);
        handleGamepadLook(camera, deltaTime);
        handleMovement(window, camera, deltaTime);
        handleSpeedAdjust(window, deltaTime);
    }

private:
    static constexpr int GAMEPAD = GLFW_JOYSTICK_1;

    float m_sensitivity = 0.08f;
    float m_gamepadSensitivity = 120.0f;

    float m_deadzone = 0.15f;

    float m_speed = 25.0f;
    float m_minSpeed = 1.0f;
    float m_maxSpeed = 100.0f;

    void handleMouseLook(GLFWwindow* window, Camera& camera) {
        double x, y;
        glfwGetCursorPos(window, &x, &y);

        if (m_firstMouse) {
            m_lastX = x;
            m_lastY = y;
            m_firstMouse = false;
        }

        float dx = static_cast<float>(x - m_lastX);
        float dy = static_cast<float>(y - m_lastY);

        m_lastX = x;
        m_lastY = y;

        camera.addYawPitch(dx * m_sensitivity, -dy * m_sensitivity);
    }

    void handleGamepadLook(Camera& camera, float dt) {
        GLFWgamepadstate state;

        if (!glfwGetGamepadState(GAMEPAD, &state))
            return;

        // Standard GLFW gamepad mapping:
        // Axis 2 = Right Stick X
        // Axis 3 = Right Stick Y

        float lookX = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
        float lookY = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];

        lookX = applyDeadzone(lookX);
        lookY = applyDeadzone(lookY);

        camera.addYawPitch(lookX * m_gamepadSensitivity * dt, -lookY * m_gamepadSensitivity * dt);
    }

    void handleMovement(GLFWwindow* window, Camera& camera, float dt) {
        glm::vec3 forward = camera.forward();
        glm::vec3 right = camera.right();
        glm::vec3 up{0.0f, 1.0f, 0.0f};

        glm::vec3 move{0.0f};

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            move += forward;

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            move -= forward;

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            move += right;

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            move -= right;

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            move += up;

        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            move -= up;

        GLFWgamepadstate state;

        if (glfwGetGamepadState(GAMEPAD, &state)) {

            // Left Stick
            // Axis 0 = Left Stick X
            // Axis 1 = Left Stick Y

            float stickX = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
            float stickY = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];

            stickX = applyDeadzone(stickX);
            stickY = applyDeadzone(stickY);

            // Left stick horizontal movement
            move += right * stickX;

            // GLFW Y axis is negative when pushed forward
            move += forward * (-stickY);

            // Y = Move Up
            if (state.buttons[GLFW_GAMEPAD_BUTTON_Y] == GLFW_PRESS)
                move += up;

            // A = Move Down
            if (state.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS)
                move -= up;
        }

        if (glm::length(move) > 0.0001f) {
            camera.setPosition(camera.position() + glm::normalize(move) * m_speed * dt);
        }
    }

    void handleSpeedAdjust(GLFWwindow* window, float dt) {
        const float rate = 20.0f;

        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
            m_speed = glm::clamp(m_speed + rate * dt, m_minSpeed, m_maxSpeed);
        }

        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
            m_speed = glm::clamp(m_speed - rate * dt, m_minSpeed, m_maxSpeed);
        }
    }

    float applyDeadzone(float value) const {
        if (glm::abs(value) < m_deadzone)
            return 0.0f;

        float sign = value > 0.0f ? 1.0f : -1.0f;

        float normalized = (glm::abs(value) - m_deadzone) / (1.0f - m_deadzone);

        return sign * glm::clamp(normalized, 0.0f, 1.0f);
    }

    bool m_firstMouse = true;

    double m_lastX = 0.0;
    double m_lastY = 0.0;
};