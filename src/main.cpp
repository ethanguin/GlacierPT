#include <iostream>
#include <stdexcept>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "vulkan/VulkanRenderer.hpp"
#include "scene/Camera.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLight.hpp"
#include "utils/Color.hpp"
#include "scene/CameraController.hpp"

int main() {
    uint32_t renderWidth = 2560;
    uint32_t renderHeight = 1440;
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

    // // set to fullscreen borderless windowed
    // glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
    // GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    // int monitorX, monitorY;
    // int monitorWidth, monitorHeight;

    // glfwGetMonitorWorkarea(monitor, &monitorX, &monitorY, &monitorWidth, &monitorHeight);

    // glfwSetWindowPos(window, monitorX, monitorY);
    // glfwSetWindowSize(window, monitorWidth, monitorHeight);

    std::cout << "GLFW window created.\n";

    // Create Scene description

    Scene scene;

    scene.addSphere({-12.0f, 5.0f, -28.0f}, 6.0f, srgbToLinear({0.08f, 0.25f, 0.75f}));
    scene.addSphere({0.0f, -1.0f, -34.0f}, 8.0f, srgbToLinear({0.75f, 0.10f, 0.08f}));
    scene.addSphere({13.0f, 4.0f, -30.0f}, 5.5f, srgbToLinear({0.10f, 0.65f, 0.35f}));
    scene.addSphere({-17.0f, -4.0f, -20.0f}, 3.0f, srgbToLinear({0.95f, 0.65f, 0.05f}));
    scene.addSphere({-7.0f, 3.0f, -19.0f}, 4.0f, srgbToLinear({0.65f, 0.08f, 0.75f}));
    scene.addSphere({4.5f, -3.0f, -21.0f}, 3.5f, srgbToLinear({0.05f, 0.55f, 0.85f}));
    scene.addSphere({15.0f, -2.0f, -18.0f}, 4.5f, srgbToLinear({0.95f, 0.20f, 0.35f}));
    scene.addSphere({-13.0f, -6.0f, -12.0f}, 2.2f, srgbToLinear({0.95f, 0.30f, 0.05f}));
    scene.addSphere({-4.0f, -5.0f, -11.0f}, 1.5f, srgbToLinear({0.10f, 0.85f, 0.70f}));
    scene.addSphere({2.5f, 5.0f, -13.0f}, 2.0f, srgbToLinear({0.85f, 0.15f, 0.60f}));
    scene.addSphere({9.0f, 5.0f, -12.0f}, 2.8f, srgbToLinear({0.20f, 0.40f, 0.95f}));
    scene.addSphere({17.0f, -5.0f, -11.0f}, 1.8f, srgbToLinear({0.95f, 0.80f, 0.10f}));
    scene.addSphere({-9.0f, 8.0f, -10.0f}, 0.9f, srgbToLinear({0.95f, 0.95f, 0.95f}));
    scene.addSphere({-1.0f, 7.0f, -16.0f}, 1.1f, srgbToLinear({0.20f, 0.95f, 0.95f}));
    scene.addSphere({7.0f, -7.0f, -15.0f}, 1.0f, srgbToLinear({0.95f, 0.15f, 0.10f}));
    scene.addSphere({12.0f, 8.0f, -20.0f}, 1.3f, srgbToLinear({0.70f, 0.20f, 0.95f}));

    scene.setAmbLight({0.9f, 0.6f, 0.9f}, 0.2f);
    scene.addLight(SceneLight::Directional({1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, 2.0f));

    Camera camera({0.0f, 0.0f, 60.0f}, {0.0f, 0.0f, -1.0f}, 50.0f, 36.0f);

    CameraController cameraController;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glfwSetCursorPos(window, width / 2.0, height / 2.0);
    auto lastFrameTime = std::chrono::steady_clock::now();

    // Create Vulkan Renderer

    try {
        VulkanRenderer renderer;

        renderer.initialize(window, scene, camera, renderWidth, renderHeight);

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - lastFrameTime).count();
            lastFrameTime = now;

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
                auto now = std::chrono::system_clock::now();
                std::time_t time = std::chrono::system_clock::to_time_t(now);

                std::tm localTime{};

#ifdef _WIN32
                localtime_s(&localTime, &time);
#else
                localtime_r(&time, &localTime);
#endif

                std::filesystem::path screenshotDir = std::filesystem::current_path() / "Screenshots";

                std::filesystem::create_directories(screenshotDir);

                std::ostringstream filename;
                filename << "GlacierPT_" << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S");

                std::filesystem::path screenshotPath = screenshotDir / filename.str();

                renderer.requestScreenshot(screenshotPath.string());
            }

            cameraController.update(window, camera, dt);
            renderer.setCamera(camera);
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