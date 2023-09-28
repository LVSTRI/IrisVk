cmake_minimum_required(VERSION 3.26)
project(IrisVkTests)

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

add_executable(IrisVkTests
    src/application.hpp
    src/application.cpp
    src/camera.hpp
    src/camera.cpp
    src/model.hpp
    src/model.cpp
    src/utilities.hpp

    src/main.cpp)

target_link_libraries(IrisVkTests PRIVATE IrisVk cgltf meshoptimizer)

target_include_directories(IrisVkTests PRIVATE src)