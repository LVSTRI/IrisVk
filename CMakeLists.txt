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

set(CMAKE_CXX_STANDARD 23)

add_executable(IrisVk
    src/utilities.hpp

    src/main.cpp)

target_link_libraries(IrisVk PRIVATE
    glm
    glfw
    Vulkan::Vulkan)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(IrisVk PRIVATE -march=native -mavx2 -Wall -Wextra)
endif()
target_include_directories(IrisVk PRIVATE src)
