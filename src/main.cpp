#include <iostream>
#include <stdexcept>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "vulkan/VulkanRenderer.hpp"
#include "scene/Camera.hpp"
#include "scene/Scene.hpp"
#include "utils/Color.hpp"

int main() {
    uint32_t renderWidth = 1920;
    uint32_t renderHeight = 1080;
    std::cout << "Starting GlacierPT...\n";

    // Create GLFW window

    glfwSetErrorCallback([](int error, const char* description) { std::cerr << "GLFW error " << error << ": " << description << '\n'; });

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(renderWidth, renderHeight, "GlacierPT", nullptr, nullptr);

    if (!window) {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return 1;
    }

    glfwSetWindowAspectRatio(window, static_cast<int>(renderWidth), static_cast<int>(renderHeight));

    // set to fullscreen borderless windowed
    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    int monitorX, monitorY;
    int monitorWidth, monitorHeight;

    glfwGetMonitorWorkarea(monitor, &monitorX, &monitorY, &monitorWidth, &monitorHeight);

    glfwSetWindowPos(window, monitorX, monitorY);
    glfwSetWindowSize(window, monitorWidth, monitorHeight);

    std::cout << "GLFW window created.\n";

    // Create Scene description

    Scene scene;

    scene.addSphere({-10.0f, 0.0f, -10.0f}, 3.5f, srgbToLinear({0.631f, 0.557f, 0.0f}));
    scene.addSphere({0.0f, 0.0f, -10.0f}, 3.5f, srgbToLinear({0.541f, 0.071f, 0.51f}));
    scene.addSphere({10.0f, 0.0f, -10.0f}, 3.5f, srgbToLinear({1.0f, 0.463f, 0.561f}));

    // Create Vulkan Renderer

    try {
        VulkanRenderer renderer;

        renderer.initialize(window, scene, renderWidth, renderHeight);

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                auto now = std::chrono::system_clock::now();
                std::time_t time = std::chrono::system_clock::to_time_t(now);

                std::tm localTime{};
                localtime_s(&localTime, &time);

                std::ostringstream filename;
                filename << "C:\\Users\\ethan\\Pictures\\Screenshots\\" << "GlacierPT_" << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S");
                renderer.requestScreenshot(filename.str());
            }

            renderer.drawFrame();
        }

        renderer.shutdown();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';

        glfwDestroyWindow(window);
        glfwTerminate();

        return 1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "GlacierPT shutdown complete.\n";

    return 0;
}