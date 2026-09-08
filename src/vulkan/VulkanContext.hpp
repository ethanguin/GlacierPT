#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <VkBootstrap.h>

class VulkanContext
{
public:
    void initialize(GLFWwindow* window);
    void shutdown();

    VkInstance instance() const { return m_instance; }
    VkSurfaceKHR surface() const { return m_surface; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice device() const { return m_device; }

    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkQueue presentQueue() const { return m_presentQueue; }
    uint32_t graphicsQueueFamilyIndex() const { return m_graphicsQueueFamilyIndex; }

private:
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
    
    void createInstance();
    void createSurface(GLFWwindow* window);
    void selectPhysicalDevice();
    void createLogicalDevice();
    void getQueues();

    void setupDeviceFeatures(); // helper function for setting up physical/logical device
    void queryDeviceProperties();
};