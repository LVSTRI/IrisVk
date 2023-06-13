cmake_minimum_required(VERSION 3.25)
project(IrisVk)

if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
    # TODO: Defines for Linux and MacOS
endif()

# Vulkan setup
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/Vulkan-Headers)

# volk setup
add_subdirectory(deps/volk)
target_link_libraries(volk PUBLIC Vulkan::Headers)

# GLFW setup
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/glfw)

# GLM setup
add_subdirectory(deps/glm)
target_compile_definitions(glm INTERFACE
    GLM_FORCE_AVX2
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE)

# cgltf setup
add_library(cgltf STATIC
    deps/cgltf/cgltf.h

    src/deps/cgltf.c)
target_include_directories(cgltf PUBLIC deps/cgltf)

# VMA setup
set(VMA_STATIC_VULKAN_FUNCTIONS OFF CACHE BOOL "" FORCE)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/VulkanMemoryAllocator)

add_library(vma STATIC src/deps/vma.cpp)
target_include_directories(vma PUBLIC deps/VulkanMemoryAllocator/include)
target_link_libraries(vma PUBLIC volk)
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(vma PRIVATE -Wno-everything)
endif()

# SPDLOG setup
set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "" FORCE)
add_subdirectory(deps/spdlog)
target_compile_options(spdlog PRIVATE $<$<BOOL:${MSVC}>:/w>)
target_compile_definitions(spdlog PRIVATE FMT_EXCEPTIONS=0)

# Tracy setup
set(TRACY_CALLSTACK ON CACHE BOOL "" FORCE)
add_subdirectory(deps/tracy)
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(TracyClient PRIVATE -Wno-everything)
endif()

set(CMAKE_CXX_STANDARD 23)

set(IRIS_MAIN_HEADERS
    include/iris/core/intrusive_atomic_ptr.hpp
    include/iris/core/macros.hpp
    include/iris/core/types.hpp

    include/iris/gfx/instance.hpp

    include/iris/wsi/wsi_platform.hpp include/iris/core/utilities.hpp)

set(IRIS_MAIN_SOURCES
    src/core/intrusive_atomic_ptr.cpp

    src/gfx/instance.cpp

    src/wsi/wsi_platform.cpp)

add_executable(IrisVk
    ${IRIS_MAIN_HEADERS}
    ${IRIS_MAIN_SOURCES}

    src/main.cpp)

target_compile_definitions(IrisVk PUBLIC
    $<$<CONFIG:Debug>:IRIS_DEBUG>

    $<$<BOOL:${WIN32}>:
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX>

    # TODO: Make these options
    IRIS_DEBUG_LOGGER
    IRIS_DEBUG_PROFILER

    VK_ENABLE_BETA_EXTENSIONS
    VK_NO_PROTOTYPES

    $<$<BOOL:${WIN32}>:IRIS_PLATFORM_WIN32>
    $<$<BOOL:${LINUX}>:IRIS_PLATFORM_LINUX>

    TRACY_ENABLE)

target_link_libraries(IrisVk PUBLIC
    glm
    vma
    volk
    glfw
    cgltf
    spdlog
    TracyClient
    Vulkan::Headers)

target_include_directories(IrisVk PUBLIC include)

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(IrisVk PRIVATE -march=native -mavx2 -Wall -Wextra)
endif()
