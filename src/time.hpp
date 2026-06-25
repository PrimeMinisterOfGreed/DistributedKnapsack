#pragma once
#include <chrono>
#include <spdlog/spdlog.h>
#ifndef ENABLE_TIMERS
#define ENABLE_TIMERS 1
#endif
#include <concepts>

template<typename Func>
concept VoidInvocable = std::invocable<Func> && std::same_as<std::invoke_result_t<Func>, void>;

template<typename Func>
concept NonVoidInvocable = std::invocable<Func> && !std::same_as<std::invoke_result_t<Func>, void>;

static constexpr void time_block(const char *label, const VoidInvocable auto &func)
{
    if constexpr (ENABLE_TIMERS)
    {
        auto start = std::chrono::steady_clock::now();
        func();
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        fmt::println("{}: {} ms", label, ms);
    }
    else
    {
        func();
    }
}

static constexpr auto time_block(const char *label, const NonVoidInvocable auto &func) -> decltype(func())
{
    if constexpr (ENABLE_TIMERS)
    {
        auto start = std::chrono::steady_clock::now();
        auto result = func();
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        fmt::println("{}: {} ms", label, ms);
        return result;
    }
    else
    {
        return func();
    }
}
