#pragma once
#include <concepts>
#include <spdlog/spdlog.h>
#ifndef DEBUG
#define DEBUG 0
#endif

#define DBG_ASSERT(predicate, errorfmt, ...) dbg_assert(predicate, errorfmt, ##__VA_ARGS__)

template <typename... Args>
constexpr void dbg_assert(std::invocable auto && predicate, const char* errorfmt, Args&&... args)
{
    if constexpr (DEBUG)
    {
        if (!predicate())
        {
            spdlog::error(errorfmt, std::forward<Args>(args)...);
            std::terminate();
        }
    }    
}
template <typename... Args>
constexpr void dbg_assert(bool predicate, const char* errorfmt, Args&&... args)
{
    if constexpr (DEBUG)
    {
        if (!predicate)
        {
            spdlog::error(errorfmt, std::forward<Args>(args)...);
            std::terminate();
        }
    }
}