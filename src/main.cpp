#include <iostream>
#include <stdexcept>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <random>
#include <cmath>
#include <cstdlib>
#include <string>
#include <cstring>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "vulkan/VulkanRenderer.hpp"
#include "scene/Camera.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLight.hpp"
#include "utils/Color.hpp"
#include "scene/CameraController.hpp"

namespace {

void printUsage(const char* exe) {
    std::cerr << "Usage: " << exe << " [-s <scene.glb|scene.gltf>]\n"
              << "  -s, --scene <path>   Scene file to render\n"
              << "  -h, --help           Show this message\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string scenePath = "../../../PathTracerTestScenes/GlassWall.glb"; // default

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-s" || arg == "--scene") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << arg << "\n";
                printUsage(argv[0]);
                return 1;
            }
            scenePath = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
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

    scene.setAmbLight({0.05f, 0.75f, 1.0f}, 0.15f);
    glm::vec3 position = {-10.0f, 17.0f, 0.0f};
    scene.addLight(SceneLight::Point(position, {1.0f, 1.0f, 1.0f}, 0.9f, 50.0f));

    scene.addSphere({-9.43, 6.05, 4.9}, 3.0, {0.922, 0.482, 0.922}, 0.2f);

    // gltf import test

    Camera camera({-10.0f, 10.0f, 70.0f}, {0.0f, 0.0f, -1.0f}, 50.0f, 36.0f);

    CameraController cameraController;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glfwSetCursorPos(window, width / 2.0, height / 2.0);
    auto lastFrameTime = std::chrono::steady_clock::now();

    // Create Vulkan Renderer

    try {
        std::cout << "CWD: " << std::filesystem::current_path() << '\n';
        scene.loadModel(scenePath);
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