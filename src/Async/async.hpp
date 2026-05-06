#pragma once
#include "executor.hpp"
#include "traits.hpp"
#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

template <typename F, typename... Args> struct Func : public Callable {
private:
  F _fnc;
  std::tuple<Args...> _args;

public:
  Func(F &&fnc, Args... args) : _fnc(fnc), _args(args...) {}

  virtual void operator()() override {
    using ret = decltype(std::forward<F>(_fnc)(std::declval<Args &>()...));
    if constexpr (std::is_void_v<ret>) {
      if constexpr (sizeof...(Args) > 0)
        std::apply(_fnc, _args);
      else
        _fnc();
    } else {
      if constexpr (sizeof...(Args) > 0)
        make_data(std::apply(_fnc, _args));
      else
        make_data(_fnc());
    }
  }

  Func(Func &) = delete;
};

template <typename> struct Handler;

template <typename T, typename... Args> class Future : public Task {
private:
  sptr<Callable> _callable;
  std::vector<sptr<Callable>> _handlers;

public:
  template <typename F>
  Future(F &&fnc, Args... args)
      : Task(), _callable(new Func<decltype(fnc), Args...>{fnc, args...}) {}

  template <typename F>
  static sptr<Future<T, Args...>> Create(F &&fnc, Args... args) {
    return sptr<Future<T, Args...>>{new Future<T, Args...>(fnc, args...)};
  }

  template <typename F>
  static sptr<Future<T, Args...>> Run(F &&fnc, Args... args) {
    auto ptr = Create(fnc, args...);
    Scheduler::main().schedule(std::static_pointer_cast<Task>(ptr));
    return ptr;
  }

  virtual void operator()(sptr<Task> thisptr) override {
    (*_callable)();
    resolve();
  }

  template <typename F> void operator+=(F fnc) {
    auto ptr = sptr<Callable>(new Func<decltype(fnc), T>{fnc});
    _handlers.push_back(ptr);
  }

  virtual operator T() {
    wait();
    return _callable->result().reintepret<T>();
  }
};

template <typename... Args> class Future<void, Args...> : public Task {
private:
  sptr<Callable> _callable;
  std::vector<sptr<Callable>> _handlers;

public:
  template <typename F>
  Future(F &&fnc, Args... args)
      : Task(), _callable(new Func<decltype(fnc), Args...>{fnc, args...}) {}

  template <typename F>
  static sptr<Future<void, Args...>> Run(F &&fnc, Args... args) {
    auto ptr =
        sptr<Future<void, Args...>>{new Future<void, Args...>(fnc, args...)};
    Scheduler::main().schedule(std::static_pointer_cast<Task>(ptr));
    return ptr;
  }

  template <typename F>
  static sptr<Future<void, Args...>> Create(F &&fnc, Args... args) {
    return sptr<Future<void, Args...>>{new Future<void, Args...>(fnc, args...)};
  }

  void resolve(bool failed = false) override {
    for (auto &h : _handlers) {
      (*h)();
    }
    Task::resolve(failed);
  }

  virtual void operator()(sptr<Task> thisptr) override {
    (*_callable)();
    resolve();
  }

  template <typename F> void operator+=(F fnc) {
    auto ptr = sptr<Callable>(new Func<decltype(fnc)>(std::move(fnc)));
    _handlers.push_back(ptr);
  }
};

template <typename F, typename... Args> auto async(F &&fnc, Args... args) {
  using ret_t = decltype(std::forward<F>(fnc)(std::declval<Args &>()...));
  auto ptr = Future<ret_t, Args...>::Run(std::move(fnc), args...);
  return ptr;
};
