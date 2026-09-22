#include "Camera.hpp"
#include <GLFW/glfw3.h>

class CameraController {
public:
    void update(GLFWwindow* window, Camera& camera, float deltaTime) {
        handleMouseLook(window, camera);
        handleMovement(window, camera, deltaTime);
    }

private:
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

        if (glm::length(move) > 0.0001f) {
            camera.setPosition(camera.position() + glm::normalize(move) * m_speed * dt);
        }
    }

    void handleSpeedAdjust(GLFWwindow* window, float dt) {
        const float rate = 20.0f; // units/sec^2 while held

        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
            m_speed = glm::clamp(m_speed + rate * dt, m_minSpeed, m_maxSpeed);
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
            m_speed = glm::clamp(m_speed - rate * dt, m_minSpeed, m_maxSpeed);
        }
    }

    bool m_firstMouse = true;
    double m_lastX = 0.0, m_lastY = 0.0;
    float m_sensitivity = 0.08f;

    float m_speed = 25.0f;
    float m_minSpeed = 1.0f;
    float m_maxSpeed = 100.0f;
};