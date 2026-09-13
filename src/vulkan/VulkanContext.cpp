#include "VulkanContext.hpp"

#include <iostream>
#include <stdexcept>

void VulkanContext::initialize(GLFWwindow* window) {
    if (!window) {
        throw std::runtime_error("VulkanContext requires a valid GLFW window.");
    }

    std::cout << "\nVULKAN INITIALIZATION:\n";

    createInstance();
    std::cout << "Vulkan instance created.\n";

    createSurface(window);
    std::cout << "Vulkan surface created.\n";

    configureDeviceFeatures();
    selectPhysicalDevice();
    queryDeviceProperties();

    std::cout << "Selected GPU: " << m_deviceProperties.deviceName << '\n';

    createLogicalDevice();
    std::cout << "Logical device created.\n";

    getQueues();
    std::cout << "Graphics and present queues acquired.\n";

    createVMA();
    std::cout << "Vulkan Memory Allocator created.\n";
}

void VulkanContext::shutdown() {
    std::cout << "\nVULKAN SHUTDOWN\n";

    if (m_device != VK_NULL_HANDLE) {
        std::cout << "Waiting for device\n";

        VkResult result = vkDeviceWaitIdle(m_device);

        if (result != VK_SUCCESS) {
            std::cerr << "Warning: vkDeviceWaitIdle failed during shutdown.\n";
        }
    }

    std::cout << "Destroying VMA allocator\n";
    m_allocator.shutdown();

    if (m_surface != VK_NULL_HANDLE) {
        std::cout << "Destroying surface\n";

        vkb::destroy_surface(m_vkbInstance, m_surface);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_vkbDevice.device != VK_NULL_HANDLE) {
        std::cout << "Destroying device\n";

        vkb::destroy_device(m_vkbDevice);

        m_vkbDevice = {};
        m_device = VK_NULL_HANDLE;
    }

    if (m_vkbInstance.instance != VK_NULL_HANDLE) {
        std::cout << "Destroying instance\n";

        vkb::destroy_instance(m_vkbInstance);

        m_vkbInstance = {};
        m_instance = VK_NULL_HANDLE;
    }

    m_physicalDevice = VK_NULL_HANDLE;
    m_graphicsQueue = VK_NULL_HANDLE;
    m_presentQueue = VK_NULL_HANDLE;
    m_graphicsQueueFamilyIndex = 0;

    std::cout << "Vulkan shutdown complete\n";
}

void VulkanContext::createInstance() {
    vkb::InstanceBuilder instanceBuilder;

    auto instanceResult = instanceBuilder.set_app_name("GlacierPT")
                              .set_engine_name("GlacierPT")
                              .require_api_version(1, 3, 0)
                              .use_default_debug_messenger()
                              .request_validation_layers()
                              .build();

    if (!instanceResult) {
        throw std::runtime_error("Failed to create Vulkan instance: " + instanceResult.error().message());
    }

    m_vkbInstance = instanceResult.value();

    m_instance = m_vkbInstance.instance;
    m_debugMessenger = m_vkbInstance.debug_messenger;
}

void VulkanContext::createSurface(GLFWwindow* window) {
    VkResult result = glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan surface.");
    }
}

void VulkanContext::configureDeviceFeatures() {
    m_features12 = {};
    m_features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    m_features12.bufferDeviceAddress = VK_TRUE;

    m_features13 = {};
    m_features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    m_features13.dynamicRendering = VK_TRUE;
    m_features13.synchronization2 = VK_TRUE;

    m_accelerationStructureFeatures = {};
    m_accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    m_accelerationStructureFeatures.accelerationStructure = VK_TRUE;

    m_rayTracingPipelineFeatures = {};
    m_rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    m_rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
}

void VulkanContext::selectPhysicalDevice() {
    vkb::PhysicalDeviceSelector selector{m_vkbInstance};

    auto physicalDeviceResult = selector.set_surface(m_surface)
                                    .set_minimum_version(1, 3)

                                    .add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
                                    .add_required_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
                                    .add_required_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)

                                    .set_required_features_12(m_features12)
                                    .set_required_features_13(m_features13)

                                    .add_required_extension_features(m_accelerationStructureFeatures)
                                    .add_required_extension_features(m_rayTracingPipelineFeatures)

                                    .select();

    if (!physicalDeviceResult) {
        throw std::runtime_error("Failed to select physical device: " + physicalDeviceResult.error().message());
    }

    m_vkbPhysicalDevice = physicalDeviceResult.value();
    m_physicalDevice = m_vkbPhysicalDevice.physical_device;
}

void VulkanContext::createLogicalDevice() {
    vkb::DeviceBuilder deviceBuilder{m_vkbPhysicalDevice};

    auto deviceResult = deviceBuilder.build();

    if (!deviceResult) {
        throw std::runtime_error("Failed to create logical device: " + deviceResult.error().message());
    }

    m_vkbDevice = deviceResult.value();
    m_device = m_vkbDevice.device;
}

void VulkanContext::getQueues() {
    auto graphicsQueueResult = m_vkbDevice.get_queue(vkb::QueueType::graphics);

    if (!graphicsQueueResult) {
        throw std::runtime_error("Failed to get graphics queue: " + graphicsQueueResult.error().message());
    }

    m_graphicsQueue = graphicsQueueResult.value();

    auto graphicsQueueIndexResult = m_vkbDevice.get_queue_index(vkb::QueueType::graphics);

    if (!graphicsQueueIndexResult) {
        throw std::runtime_error("Failed to get graphics queue family index: " + graphicsQueueIndexResult.error().message());
    }

    m_graphicsQueueFamilyIndex = graphicsQueueIndexResult.value();

    auto presentQueueResult = m_vkbDevice.get_queue(vkb::QueueType::present);

    if (!presentQueueResult) {
        throw std::runtime_error("Failed to get present queue: " + presentQueueResult.error().message());
    }

    m_presentQueue = presentQueueResult.value();
}

void VulkanContext::createVMA() {
    m_allocator.initialize(m_instance, m_physicalDevice, m_device);
}

void VulkanContext::queryDeviceProperties() {
    vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties);
}