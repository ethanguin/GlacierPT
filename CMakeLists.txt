cmake_minimum_required(VERSION 3.25)

project(GlacierPT
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_program(GLSLC glslc REQUIRED)
find_program(DXC dxc REQUIRED)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG v1.4.335
)

FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(glfw)
FetchContent_MakeAvailable(vk_bootstrap)
FetchContent_MakeAvailable(vma)

file(GLOB_RECURSE GLACIERPT_SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(GlacierPT
    ${GLACIERPT_SRC_FILES}
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
        GPUOpen::VulkanMemoryAllocator
)

# SHADER COMPILATION

set(SHADER_SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders
)

set(SHADER_BINARY_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/shaders
)

find_program(GLSLC glslc REQUIRED)
find_program(DXC dxc REQUIRED)

# Find shader source files.
file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/*.vert"
    "${SHADER_SOURCE_DIR}/*.frag"
    "${SHADER_SOURCE_DIR}/*.comp"
    "${SHADER_SOURCE_DIR}/*.hlsl"
)

set(SHADER_OUTPUTS)

foreach(SHADER_SOURCE ${SHADER_SOURCES})

    get_filename_component(SHADER_FILENAME
        "${SHADER_SOURCE}"
        NAME
    )
    
    get_filename_component(SHADER_EXTENSION
        "${SHADER_SOURCE}"
        EXT
    )

    if(SHADER_EXTENSION STREQUAL ".hlsl")

        string(REGEX REPLACE
            "\\.hlsl$"
            ".spv"
            SHADER_OUTPUT_NAME
            "${SHADER_FILENAME}"
        )

    else()

        set(SHADER_OUTPUT_NAME
            "${SHADER_FILENAME}.spv"
        )

    endif()

    set(SHADER_OUTPUT
        "${SHADER_BINARY_DIR}/${SHADER_OUTPUT_NAME}"
    )

    # GLSL

    if(SHADER_EXTENSION STREQUAL ".vert"
       OR SHADER_EXTENSION STREQUAL ".frag"
       OR SHADER_EXTENSION STREQUAL ".comp")

        add_custom_command(
            OUTPUT "${SHADER_OUTPUT}"

            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${SHADER_BINARY_DIR}"

            COMMAND ${GLSLC}
                    "${SHADER_SOURCE}"
                    -o "${SHADER_OUTPUT}"

            DEPENDS "${SHADER_SOURCE}"

            COMMENT "Compiling GLSL shader ${SHADER_FILENAME}"
            VERBATIM
        )

    # HLSL

    elseif(SHADER_EXTENSION STREQUAL ".hlsl")

        add_custom_command(
            OUTPUT "${SHADER_OUTPUT}"

            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${SHADER_BINARY_DIR}"

            COMMAND ${DXC}
                    -spirv
                    -T lib_6_3
                    -fspv-target-env=vulkan1.3
                    -Fo "${SHADER_OUTPUT}"
                    "${SHADER_SOURCE}"

            DEPENDS "${SHADER_SOURCE}"

            COMMENT "Compiling HLSL shader ${SHADER_FILENAME}"
            VERBATIM
        )

    endif()

    list(APPEND SHADER_OUTPUTS
        "${SHADER_OUTPUT}"
    )

endforeach()

add_custom_target(GlacierPTShaders
    DEPENDS ${SHADER_OUTPUTS}
)

add_dependencies(GlacierPT GlacierPTShaders)