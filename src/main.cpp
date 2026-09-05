#include <iostream>
#include <stdexcept>

#include <GLFW/glfw3.h>

#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanSwapchain.hpp"

int main()
{
    std::cout << "Starting GlacierPT...\n";

    glfwSetErrorCallback(
        [](int error, const char* description)
        {
            std::cerr
                << "GLFW error "
                << error
                << ": "
                << description
                << '\n';
        }
    );

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(
        1280,
        720,
        "GlacierPT",
        nullptr,
        nullptr
    );

    if (!window)
    {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return 1;
    }

    std::cout << "GLFW window created.\n";

    try
    {
        VulkanContext vulkan;

        vulkan.initialize(window);

        VulkanSwapchain swapchain;

        swapchain.initialize(
            vulkan.physicalDevice(),
            vulkan.device(),
            vulkan.surface(),
            1280,
            720
        );

        std::cout << "Swapchain created.\n";
        std::cout << "Swapchain images: " << swapchain.images().size() << '\n';

        std::cout << "Vulkan initialized successfully.\n";

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }
        std::cout << "Window loop exited.\n";

        swapchain.shutdown();

        std::cout << "Swapchain shutdown complete.\n";

        vulkan.shutdown();

        std::cout << "Vulkan shutdown complete.\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: "
                  << e.what()
                  << '\n';

        glfwDestroyWindow(window);
        glfwTerminate();

        return 1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "GlacierPT shutdown complete.\n";

    return 0;
}