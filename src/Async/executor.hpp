#pragma once
#include "traits.hpp"
#include "MultiThread/autoresetevent.hpp"
#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/make_shared_array.hpp>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

class DynData {
private:
  void *ptr = nullptr;
  size_t size{};

public:
  template <typename T> void emplace(T &&value) {
    size = sizeof(T);
    ptr = malloc(size);
    std::memcpy(ptr, &value, sizeof(T));
  }

  template <typename T> void emplace(T &&value, size_t size) {
    this->size = size;
    ptr = malloc(size);
    std::memcpy(ptr, &value, size);
  }

  template <typename T> T &reintepret() {
    return reinterpret_cast<T &>(*((T *)ptr));
  }

  bool has_value() const { return ptr != nullptr; }

  ~DynData() {
    if (ptr != nullptr) {
      free(ptr);
      ptr = nullptr;
    }
  }
};

struct Callable {
  DynData _result{};

public:
  virtual void operator()() = 0;
  DynData &result() { return this->_result; }
  template <typename T> void make_data(T &&data) {
    _result.emplace<T>(std::move(data));
  }
};

class Executor;
class Task {
  friend class Executor;
  ManualResetEvent _executed{false};

public:
  enum AsyncState { WAITING_EXECUTION, RESOLVED, FAILED };

protected:
  enum AsyncType { START, THEN, RESULT };
  AsyncState _state = WAITING_EXECUTION;
  std::mutex _lock{};
  sptr<Task> _onComplete{};
  sptr<Task> _onFail{};
  virtual void resolve(bool failed = false);

public:
  Task() {}
  virtual void operator()(sptr<Task> thisptr) = 0;
  virtual ~Task() = default;
  Task(Task &) = delete;
  AsyncState state() const { return _state; }

  void wait() { _executed.WaitOne(); }
  void onComplete(sptr<Task> task);
  void onFail(sptr<Task> task);
  void cancel();
};

class PostableTask : public Task {
private:
  std::function<void()> _fnc;

public:
  PostableTask(std::function<void()> fnc) : Task(), _fnc(fnc) {}
  virtual void operator()(sptr<Task> thisptr) {
    _fnc();
    resolve();
  }
};

class Executor {
  friend class Scheduler;

public:
  enum State { IDLE, PROCESSING, WAITING_EXECUTION };

protected:
  std::thread *_executingThread = nullptr;
  State status = IDLE;
  bool _end = false;
  std::mutex _lock;
  sptr<Task> _currentExecution{};
  void push(sptr<Task> task);
  ManualResetEvent onCompleted{false};
  ManualResetEvent onAssigned{false};

  inline sptr<Task> take() {
    std::lock_guard l{_lock};
    return _currentExecution;
  }

  inline void reset() {
    std::lock_guard l{_lock};
    _currentExecution = nullptr;
  }

public:
  Executor();
  bool assign(sptr<Task> task);
  sptr<Task> post(std::function<void()> f);
  void start();
  void wait_termination();
  State state() const { return status; }
  void stop();
  ~Executor();
};

class Scheduler {
private:
  std::vector<sptr<Executor>> _executors{};
  int _previousCount = 0;
  bool _end = false;
  std::thread *_executionThread = nullptr;
  std::mutex schedLock{};
  int _maxExecutors = 10;
  int WaitAnyExecutorIdle();
  void forExecutors(std::function<void(sptr<Executor> &e)>);

protected:
  bool assign(sptr<Task> task);
  ManualResetEvent onThreadTerminated{false};

public:
  sptr<Task> post(std::function<void()> f);
  void schedule(sptr<Task> task);
  void start(bool newThread = true);
  void stop();
  void reset();

  bool next();
  void run_to_empty();

  inline Scheduler &setMaxExecutors(int maxThreads) {
    _maxExecutors = maxThreads;
    return *this;
  }
  static Scheduler &main();
  std::optional<sptr<Task>> take();
  bool empty() const;

protected:
  std::queue<sptr<Task>> mq{};
  Scheduler();
  ~Scheduler();
  static Scheduler *_instance;
  void routine();
};

