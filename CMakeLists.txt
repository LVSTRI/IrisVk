cmake_minimum_required(VERSION 3.25)
project(IrisVk)

find_package(Vulkan REQUIRED)

# GLFW setup
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
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

# VMA setup
add_subdirectory(deps/VulkanMemoryAllocator)
add_library(vma STATIC src/deps/vma.cpp)
target_include_directories(vma PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    deps/VulkanMemoryAllocator/include)

# SPDLOG setup
set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "" FORCE)
add_subdirectory(deps/spdlog)
target_compile_options(spdlog PRIVATE $<$<BOOL:${MSVC}>:/w>)
target_compile_definitions(spdlog PRIVATE FMT_EXCEPTIONS=0)

# Tracy setup
set(TRACY_CALLSTACK ON CACHE BOOL "" FORCE)
add_subdirectory(deps/tracy)

set(CMAKE_CXX_STANDARD 23)

set(IRIS_MAIN_HEADERS
    include/iris/core/macros.hpp
    include/iris/core/platform.hpp
    include/iris/core/types.hpp)

set(IRIS_MAIN_SOURCES)

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

    $<$<BOOL:${WIN32}>:IRIS_PLATFORM_WIN32>
    $<$<BOOL:${LINUX}>:IRIS_PLATFORM_LINUX>

    TRACY_ENABLE)

target_link_libraries(IrisVk PUBLIC
    glm
    vma
    glfw
    spdlog
    TracyClient
    Vulkan::Vulkan)

target_include_directories(IrisVk PUBLIC
    include)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(IrisVk PRIVATE -march=native -mavx2 -Wall -Wextra)
endif()
