#pragma once

#include <iris/iris.hpp>

#include <chrono>

namespace test {
    namespace fs = std::filesystem;
    namespace ch = std::chrono;

    using namespace ir::literals;
    using namespace ir::types;

    constexpr static auto frames_in_flight = 2_u32;
}
