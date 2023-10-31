cmake_minimum_required(VERSION 3.26)
project(IrisVkTests)

set(IRIS_ENABLE_DEBUG_PROFILER OFF CACHE BOOL "" FORCE)
set(IRIS_ENABLE_NVIDIA_DLSS ON CACHE BOOL "" FORCE)
add_subdirectory(deps/IrisVk)

set(CMAKE_CXX_STANDARD 23)

# cgltf setup
add_library(cgltf STATIC
    deps/cgltf/cgltf.h

    src/deps/cgltf.c
)
target_include_directories(cgltf PUBLIC deps/cgltf)
target_compile_definitions(cgltf PUBLIC
    $<$<BOOL:${WIN32}>:
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX>
)

# meshoptimizer setup
add_subdirectory(deps/meshoptimizer)

# imgui setup
add_library(imgui
    deps/imgui/imconfig.h
    deps/imgui/imgui.cpp
    deps/imgui/imgui.h
    deps/imgui/imgui_demo.cpp
    deps/imgui/imgui_draw.cpp
    deps/imgui/imgui_internal.h
    deps/imgui/imgui_tables.cpp
    deps/imgui/imgui_widgets.cpp
    deps/imgui/imstb_rectpack.h
    deps/imgui/imstb_textedit.h
    deps/imgui/imstb_truetype.h
    deps/imgui/backends/imgui_impl_vulkan.cpp
    deps/imgui/backends/imgui_impl_vulkan.h
    deps/imgui/backends/imgui_impl_glfw.cpp
    deps/imgui/backends/imgui_impl_glfw.h
)
target_include_directories(imgui PUBLIC deps/imgui)
target_link_libraries(imgui PUBLIC glfw volk)
target_compile_definitions(imgui PUBLIC VK_NO_PROTOTYPES)

add_library(implot
    deps/implot/implot.cpp
    deps/implot/implot.h
    deps/implot/implot_demo.cpp
    deps/implot/implot_internal.h
    deps/implot/implot_items.cpp
)
target_include_directories(implot PUBLIC
    deps/imgui
    deps/implot
)

add_executable(IrisVkTests
    src/application.hpp
    src/application.cpp
    src/camera.hpp
    src/camera.cpp
    src/model.hpp
    src/model.cpp
    src/utilities.hpp

    src/main.cpp)

target_link_libraries(IrisVkTests PRIVATE IrisVk cgltf meshoptimizer imgui implot)

target_include_directories(IrisVkTests PRIVATE src)
