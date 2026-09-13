#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VulkanAllocator.hpp"
#include <VkBootstrap.h>

class VulkanContext {
public:
    void initialize(GLFWwindow* window);
    void shutdown();

    VkInstance instance() const {
        return m_instance;
    }
    VkSurfaceKHR surface() const {
        return m_surface;
    }
    VkPhysicalDevice physicalDevice() const {
        return m_physicalDevice;
    }
    VkDevice device() const {
        return m_device;
    }

    VkQueue graphicsQueue() const {
        return m_graphicsQueue;
    }
    VkQueue presentQueue() const {
        return m_presentQueue;
    }
    uint32_t graphicsQueueFamilyIndex() const {
        return m_graphicsQueueFamilyIndex;
    }

    // Allocator
    VulkanAllocator& allocator() {
        return m_allocator;
    }

private:
    VkPhysicalDeviceVulkan12Features m_features12{};
    VkPhysicalDeviceVulkan13Features m_features13{};

    VkPhysicalDeviceAccelerationStructureFeaturesKHR m_accelerationStructureFeatures{};

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_rayTracingPipelineFeatures{};

    vkb::Instance m_vkbInstance{};
    vkb::Device m_vkbDevice{};

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    vkb::PhysicalDevice m_vkbPhysicalDevice;
    VkDevice m_device = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties m_deviceProperties{};
    VkPhysicalDeviceFeatures m_deviceFeatures{};

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    uint32_t m_graphicsQueueFamilyIndex = 0;

    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

    // Memory Allocators
    VulkanAllocator m_allocator;

    void createInstance();
    void createSurface(GLFWwindow* window);
    void configureDeviceFeatures();
    void selectPhysicalDevice();
    void createLogicalDevice();
    void getQueues();
    void createVMA();

    void queryDeviceProperties();
};