#pragma once
#include <chrono>
#include <future>
#include <spdlog/spdlog.h>
#ifndef ENABLE_TIMERS
#define ENABLE_TIMERS 0
#endif
#include <concepts>
#include <thread>

#define TIME_BLOCK(name, func) time_block(name, [&]() { func })

template <typename Func>
concept VoidInvocable = std::invocable<Func> && std::same_as<std::invoke_result_t<Func>, void>;

template <typename Func>
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

static constexpr std::thread async_time_block(const char *label, const VoidInvocable auto &&func)
{
	return std::thread([label, func = std::move(func)]() mutable { time_block(label, func); });
}

template <NonVoidInvocable Func>
static constexpr std::pair<std::thread, std::promise<decltype(std::declval<Func>()())>> async_time_block(
	const char *label, const Func &func)
{
	std::promise<decltype(func())> promise;
	auto future = promise.get_future();
	std::thread t([label, func = std::move(func), promise = std::move(promise)]() mutable {
		try
		{
			auto result = time_block(label, func);
			promise.set_value(result);
		}
		catch (...)
		{
			promise.set_exception(std::current_exception());
		}
	});
	return {std::move(t), std::move(promise)};
}
