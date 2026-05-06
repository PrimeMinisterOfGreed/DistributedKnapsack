#pragma once
#include <concepts>
#include <functional>
#include <future>
#include <string>
#include <type_traits>
#include <vector>

template <typename T> using sptr = std::shared_ptr<T>;

template <typename T, typename F, typename... Args>
concept is_function_return = requires(T ret, F a, Args... args) {
  { a.operator()(args...) } -> std::convertible_to<T>;
};

template <typename F, typename... Args>
concept is_void_function = requires(F f, Args... args) {
  { f.operator()(args...) } -> std::same_as<void>;
};
