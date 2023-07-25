cmake_minimum_required(VERSION 3.25)
project(IrisVk)

# Vulkan setup
find_package(Vulkan REQUIRED)

# volk setup
add_subdirectory(deps/volk)
target_include_directories(volk PUBLIC ${Vulkan_INCLUDE_DIRS})

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
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

# cgltf setup
add_library(cgltf STATIC
    deps/cgltf/cgltf.h

    src/renderer/deps/cgltf.c
)
target_include_directories(cgltf PUBLIC deps/cgltf)
target_compile_definitions(cgltf PUBLIC
    $<$<BOOL:${WIN32}>:
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX>
)

# VMA setup
set(VMA_BUILD_SAMPLE ON CACHE BOOL "" FORCE)
set(VMA_STATIC_VULKAN_FUNCTIONS OFF CACHE BOOL "" FORCE)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON CACHE BOOL "" FORCE)
add_subdirectory(deps/VulkanMemoryAllocator)

add_library(vma STATIC src/iris/deps/vma.cpp)
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

# SPIRV-Headers setup
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/SPIRV-Headers)

# SPIRV-Tools setup
set(SPIRV_WARN_EVERYTHING OFF CACHE BOOL "" FORCE)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(deps/SPIRV-Tools)

# glslang setup
set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_JS OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/glslang)

# SPIRV-Cross setup
set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(deps/SPIRV-Cross)

# shaderc setup
set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(deps/shaderc)
# doesn't compile under clang on Windows unless I do this
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(shaderc PRIVATE -Wno-everything -std=c++20)
    target_compile_options(shaderc_util PRIVATE -Wno-everything -std=c++20)
endif()

set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VULKAN OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
add_subdirectory(deps/KTX-Software)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set_property(TARGET ktx PROPERTY CXX_STANDARD 17)
endif()
target_compile_definitions(ktx PUBLIC KHRONOS_STATIC)

# meshoptimizer setup
add_subdirectory(deps/meshoptimizer)

# unordered_dense setup
add_subdirectory(deps/unordered_dense)

# mio setup
add_subdirectory(deps/mio)

set(CMAKE_CXX_STANDARD 23)

set(IRIS_MAIN_HEADERS
    include/iris/core/enums.hpp
    include/iris/core/forwards.hpp
    include/iris/core/hash.hpp
    include/iris/core/intrusive_atomic_ptr.hpp
    include/iris/core/macros.hpp
    include/iris/core/types.hpp
    include/iris/core/utilities.hpp

    include/iris/gfx/buffer.hpp
    include/iris/gfx/cache.hpp
    include/iris/gfx/clear_value.hpp
    include/iris/gfx/command_buffer.hpp
    include/iris/gfx/command_pool.hpp
    include/iris/gfx/deletion_queue.hpp
    include/iris/gfx/descriptor_layout.hpp
    include/iris/gfx/descriptor_pool.hpp
    include/iris/gfx/descriptor_set.hpp
    include/iris/gfx/device.hpp
    include/iris/gfx/fence.hpp
    include/iris/gfx/frame_counter.hpp
    include/iris/gfx/framebuffer.hpp
    include/iris/gfx/instance.hpp
    include/iris/gfx/pipeline.hpp
    include/iris/gfx/image.hpp
    include/iris/gfx/queue.hpp
    include/iris/gfx/render_pass.hpp
    include/iris/gfx/sampler.hpp
    include/iris/gfx/semaphore.hpp
    include/iris/gfx/swapchain.hpp
    include/iris/gfx/texture.hpp

    include/iris/wsi/input.hpp
    include/iris/wsi/wsi_platform.hpp
)

set(IRIS_MAIN_SOURCES
    src/iris/gfx/command_buffer.cpp
    src/iris/gfx/command_pool.cpp
    src/iris/gfx/deletion_queue.cpp
    src/iris/gfx/descriptor_layout.cpp
    src/iris/gfx/descriptor_pool.cpp
    src/iris/gfx/descriptor_set.cpp
    src/iris/gfx/device.cpp
    src/iris/gfx/fence.cpp
    src/iris/gfx/frame_counter.cpp
    src/iris/gfx/framebuffer.cpp
    src/iris/gfx/image.cpp
    src/iris/gfx/instance.cpp
    src/iris/gfx/pipeline.cpp
    src/iris/gfx/queue.cpp
    src/iris/gfx/render_pass.cpp
    src/iris/gfx/sampler.cpp
    src/iris/gfx/semaphore.cpp
    src/iris/gfx/swapchain.cpp
    src/iris/gfx/texture.cpp

    src/iris/wsi/input.cpp
    src/iris/wsi/wsi_platform.cpp
)

add_library(IrisVk
    ${IRIS_MAIN_HEADERS}
    ${IRIS_MAIN_SOURCES}
)

target_compile_definitions(IrisVk PUBLIC
    $<$<CONFIG:Debug>:IRIS_DEBUG>

    $<$<BOOL:${WIN32}>:
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX>

    # TODO: Make these options
    IRIS_DEBUG_LOGGER
    #IRIS_DEBUG_PROFILER
    #IRIS_VALIDATION_LAYERS

    #VK_ENABLE_BETA_EXTENSIONS
    VK_NO_PROTOTYPES

    $<$<BOOL:${WIN32}>:IRIS_PLATFORM_WIN32>
    $<$<BOOL:${LINUX}>:IRIS_PLATFORM_LINUX>

    TRACY_ENABLE
)

target_link_libraries(IrisVk PUBLIC
    glm
    vma
    mio
    ktx
    volk
    glfw
    spdlog
    shaderc
    TracyClient
    unordered_dense
    spirv-cross-glsl
)

target_include_directories(IrisVk PUBLIC include ${Vulkan_INCLUDE_DIRS})
set(IRIS_VECTOR_COMPILER_OPTIONS -march=native)
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(IrisVk PUBLIC
        ${IRIS_VECTOR_COMPILER_OPTIONS}
        -Wall
        -Wextra)
endif()

set(IRIS_RENDERER_HEADERS
    include/renderer/core/application.hpp
    include/renderer/core/camera.hpp
    include/renderer/core/model.hpp
    include/renderer/core/utilities.hpp
)

set(IRIS_RENDERER_SOURCES
    src/renderer/core/application.cpp
    src/renderer/core/camera.cpp
    src/renderer/core/model.cpp

    src/main.cpp
)

add_executable(
    IrisVkRenderer
    ${IRIS_RENDERER_HEADERS}
    ${IRIS_RENDERER_SOURCES}
)

target_link_libraries(IrisVkRenderer PRIVATE IrisVk cgltf meshoptimizer)
target_compile_options(IrisVkRenderer PRIVATE ${IRIS_VECTOR_COMPILER_OPTIONS})