cmake_minimum_required(VERSION 3.25)

project(GlacierPT
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_Declare(
    vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG v1.4.335
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)
FetchContent_MakeAvailable(vk_bootstrap)

add_executable(GlacierPT
    src/main.cpp
    src/vulkan/VulkanContext.cpp
    src/vulkan/VulkanSwapchain.cpp
)

target_include_directories(GlacierPT
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(GlacierPT PRIVATE cxx_std_20)

target_link_libraries(GlacierPT
    PRIVATE
        Vulkan::Vulkan
        glfw
        vk-bootstrap::vk-bootstrap
)