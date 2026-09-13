#include <iostream>
#include <stdexcept>

#include <GLFW/glfw3.h>

#include "vulkan/VulkanRenderer.hpp"

int main() {
    std::cout << "Starting GlacierPT...\n";

    glfwSetErrorCallback([](int error, const char* description) { std::cerr << "GLFW error " << error << ": " << description << '\n'; });

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "GlacierPT", nullptr, nullptr);

    if (!window) {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return 1;
    }

    std::cout << "GLFW window created.\n";

    try {
        VulkanRenderer renderer;

        renderer.initialize(window);

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
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