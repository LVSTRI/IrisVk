#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>
#include <iris/core/utilities.hpp>

#include <chrono>

namespace app {
    using namespace ir::literals;
    using namespace ir::types;
    namespace fs = std::filesystem;

    namespace ch = std::chrono;

    constexpr static auto frames_in_flight = 2_u32;
}
