#pragma once

#define IR_DECLARE_COPY(T)  \
    T(const T&) noexcept;   \
    auto operator =(const T&) noexcept -> T&

#define IR_DEFAULT_COPY(T)          \
    T(const T&) noexcept = default; \
    auto operator =(const T&) noexcept -> T& = default

#define IR_DELETE_COPY(T)           \
    T(const T&) noexcept = delete;  \
    auto operator =(const T&) noexcept -> T& = delete

#define IR_DECLARE_MOVE(T)  \
    T(T&&) noexcept;        \
    auto operator =(T&&) noexcept -> T&

#define IR_DEFAULT_MOVE(T)      \
    T(T&&) noexcept = default;  \
    auto operator =(T&&) noexcept -> T& = default

#define IR_DELETE_MOVE(T)       \
    T(T&&) noexcept = delete;   \
    auto operator =(T&&) noexcept -> T& = delete

#define IR_QUALIFIED_DECLARE_COPY(T, ...)   \
    __VA_ARGS__ T(const T&) noexcept;       \
    __VA_ARGS__ auto operator =(const T&) noexcept -> T&

#define IR_QUALIFIED_DEFAULT_COPY(T, ...)       \
    __VA_ARGS__ T(const T&) noexcept = default; \
    __VA_ARGS__ auto operator =(const T&) noexcept -> T& = default

#define IR_QUALIFIED_DELETE_COPY(T, ...)        \
    __VA_ARGS__ T(const T&) noexcept = delete;  \
    __VA_ARGS__ auto operator =(const T&) noexcept -> T& = delete

#define IR_QUALIFIED_DECLARE_MOVE(T, ...)   \
    __VA_ARGS__ T(T&&) noexcept;            \
    __VA_ARGS__ auto operator =(T&&) noexcept -> T&

#define IR_QUALIFIED_DEFAULT_MOVE(T, ...)   \
    __VA_ARGS__ T(T&&) noexcept = default;  \
    __VA_ARGS__ auto operator =(T&&) noexcept -> T& = default

#define IR_QUALIFIED_DELETE_MOVE(T, ...)    \
    __VA_ARGS__ T(T&&) noexcept = delete;   \
    __VA_ARGS__ auto operator =(T&&) noexcept -> T& = delete

#define IR_NODISCARD [[nodiscard]]
#define IR_NORETURN [[noreturn]]
#define IR_FALLTHROUGH [[fallthrough]]
#define IR_MAYBE_UNUSED [[maybe_unused]]
#define IR_LIKELY [[likely]]
#define IR_UNLIKELY [[unlikely]]

#if defined(__clang__) || defined(__GNUC__)
    #define IR_CPP_VERSION __cplusplus
    #define IR_UNREACHABLE() __builtin_unreachable()
    #define IR_SIGNATURE() __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
    #define IR_CPP_VERSION _MSVC_LANG
    #define IR_UNREACHABLE() __assume(false)
    #define IR_SIGNATURE() __FUNCSIG__
#else
    #error "unsupported"
#endif

#if defined(IR_DEBUG)
    #include <cassert>
    #define IR_ASSERT(expr, msg) assert((expr) && (msg))
    #define IR_VULKAN_CHECK(logger, expr)                                        \
        do {                                                                     \
            const auto __result = (expr);                                        \
            if (__result != VK_SUCCESS) {                                        \
                logger->critical("vulkan error: {}", string_VkResult(__result)); \
            }                                                                    \
        } while (false)
#else
    #define IR_ASSERT(expr, msg) (void)(expr)
    #define IR_VULKAN_CHECK(logger, expr) \
    do {                                  \
        (void)(logger);                   \
        (void)(expr);                     \
    } while (false)
#endif

#if defined(IRIS_DEBUG_LOGGER)
    #define IR_LOG_DEBUG(logger, ...) (logger)->debug(__VA_ARGS__)
    #define IR_LOG_INFO(logger, ...) (logger)->info(__VA_ARGS__)
    #define IR_LOG_WARN(logger, ...) (logger)->warn(__VA_ARGS__)
    #define IR_LOG_ERROR(logger, ...) (logger)->error(__VA_ARGS__)
    #define IR_LOG_CRITICAL(logger, ...) (logger)->critical(__VA_ARGS__)
#else
    #define IR_LOG_DEBUG(logger, ...) (void)(logger)
    #define IR_LOG_INFO(logger, ...) (void)(logger)
    #define IR_LOG_WARN(logger, ...) (void)(logger)
    #define IR_LOG_ERROR(logger, ...) (void)(logger)
    #define IR_LOG_CRITICAL(logger, ...) (void)(logger)
#endif

#if defined(IRIS_DEBUG_PROFILER)
    #include <tracy/Tracy.hpp>

    #define IR_PROFILE_NAMED_SCOPE(name) ZoneScopedN(name)
    #define IR_PROFILE_SCOPED() ZoneScoped
    #define IR_MARK_FRAME() FrameMark
#else
    #define IR_PROFILE_NAMED_SCOPE(name) (void)(name)
    #define IR_PROFILE_SCOPED()
    #define IR_MARK_FRAME()
#endif
