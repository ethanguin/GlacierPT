#include "VulkanContext.hpp"

#include <iostream>
#include <stdexcept>
#include <VkBootstrap.h>

void VulkanContext::initialize(GLFWwindow* window)
{
    if (!window)
    {
        throw std::runtime_error("VulkanContext requires a valid GLFW window.");
    }

    // Vulkan instance

    vkb::InstanceBuilder instanceBuilder;

    auto instanceResult = instanceBuilder
        .set_app_name("GlacierPT")
        .set_engine_name("GlacierPT")
        .require_api_version(1, 3, 0)
        .use_default_debug_messenger()
        .request_validation_layers()
        .build();

    if (!instanceResult)
    {
        throw std::runtime_error(
            "Failed to create Vulkan instance: " +
            instanceResult.error().message()
        );
    }

    m_vkbInstance = instanceResult.value();

    m_instance = m_vkbInstance.instance;
    m_debugMessenger = m_vkbInstance.debug_messenger;

    // GLFW surface

    VkResult surfaceResult = glfwCreateWindowSurface(
        m_instance,
        window,
        nullptr,
        &m_surface
    );

    if (surfaceResult != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan surface.");
    }

    // Physical device

    vkb::PhysicalDeviceSelector selector{ m_vkbInstance };

    auto physicalDeviceResult = selector
        .set_surface(m_surface)
        .select();

    if (!physicalDeviceResult)
    {
        throw std::runtime_error(
            "Failed to select physical device: " +
            physicalDeviceResult.error().message()
        );
    }

    vkb::PhysicalDevice physicalDevice =
        physicalDeviceResult.value();

    m_physicalDevice = physicalDevice.physical_device;

    // Logical device

    vkb::DeviceBuilder deviceBuilder{ physicalDevice };

    auto deviceResult = deviceBuilder.build();

    if (!deviceResult)
    {
        throw std::runtime_error(
            "Failed to create logical device: " +
            deviceResult.error().message()
        );
    }

    m_vkbDevice = deviceResult.value();

    m_device = m_vkbDevice.device;

    // Queues

    auto graphicsQueueResult =
        m_vkbDevice.get_queue(vkb::QueueType::graphics);

    if (!graphicsQueueResult)
    {
        throw std::runtime_error(
            "Failed to get graphics queue: " +
            graphicsQueueResult.error().message()
        );
    }

    m_graphicsQueue = graphicsQueueResult.value();

    auto presentQueueResult =
        m_vkbDevice.get_queue(vkb::QueueType::present);

    if (!presentQueueResult)
    {
        throw std::runtime_error(
            "Failed to get present queue: " +
            presentQueueResult.error().message()
        );
    }

    m_presentQueue = presentQueueResult.value();

    // Report

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(
        m_physicalDevice,
        &properties
    );

    std::cout << "Vulkan instance created.\n";
    std::cout << "Vulkan surface created.\n";
    std::cout << "Selected GPU: "
              << properties.deviceName
              << '\n';
    std::cout << "Logical device created.\n";
    std::cout << "Graphics and present queues acquired.\n";
    
    // TODO
    // need to select an RT capable device
}

void VulkanContext::shutdown()
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_device);
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(
            m_instance,
            m_surface,
            nullptr
        );

        m_surface = VK_NULL_HANDLE;
    }

    if (m_debugMessenger != VK_NULL_HANDLE)
    {
        vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(
            m_instance,
            nullptr
        );

        m_instance = VK_NULL_HANDLE;
    }

    m_physicalDevice = VK_NULL_HANDLE;
    m_graphicsQueue = VK_NULL_HANDLE;
    m_presentQueue = VK_NULL_HANDLE;
}