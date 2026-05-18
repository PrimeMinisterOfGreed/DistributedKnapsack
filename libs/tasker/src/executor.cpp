#include "Async/executor.hpp"
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

Scheduler *Scheduler::_instance = new Scheduler();

int Scheduler::WaitAnyExecutorIdle() {
  std::vector<WaitHandle *> _handlers{};
  for (auto e : _executors)
    _handlers.push_back(&e->onCompleted);
  auto res = WaitAny(_handlers);
  return res;
}

bool Scheduler::assign(sptr<Task> task) {
  for (auto e : _executors) {
    if (e->assign(task))
      return true;
  }
  return false;
}

sptr<Task> Scheduler::post(std::function<void()> f) {
  std::lock_guard lock{schedLock};
  auto alloc = sptr<Task>{new PostableTask{f}};
  mq.push(alloc);
  return alloc;
}

void Scheduler::schedule(sptr<Task> task) {
  std::lock_guard lock{schedLock};
  mq.push(task);
}

std::optional<sptr<Task>> Scheduler::take() {
  std::lock_guard lock{schedLock};
  if (mq.size() == 0)
    return {};
  auto v = mq.front();
  mq.pop();
  return {v};
}

bool Scheduler::empty() const {
  if (mq.empty()) {
    for (auto &exec : _executors) {
      if (exec->state() != Executor::IDLE) {
        return false;
      }
    }
  }
  return false;
}

void Scheduler::start(bool newThread) {
  if (!newThread)
    routine();
  if (_executionThread == nullptr) {
    _executionThread = new std::thread{[this]() { routine(); }};
    _executionThread->detach();
  }
}

void Scheduler::routine() {
  while (!_end) {
    onThreadTerminated.Reset();
    // TODO introduce an optimization to decide if disband or allocate new
    // executors
    auto ecount = _executors.size();
    if (ecount == 0) {
      _executors.push_back(sptr<Executor>(new Executor()));
      _executors.at(0)->start();
    }
    auto task = take();
    if (!task.has_value())
      continue;
    if (task.value()->state() == Task::RESOLVED)
      continue;
    else if (assign(task.value())) {
      continue;
    } else if (ecount < _maxExecutors) {
      auto executor = sptr<Executor>(new Executor());
      executor->start();
      _executors.push_back(executor);
      executor->assign(task.value());
    } else {
      auto free = WaitAnyExecutorIdle();
      _executors.at(free)->assign(task.value());
    }
  }

  onThreadTerminated.Set();
}

void Scheduler::stop() { _end = true; }

void Scheduler::reset() {
  std::lock_guard lock{schedLock};
  while (!mq.empty()) {
    mq.pop();
  }
  for (auto &&ex : _executors) {
    ex->stop();
    ex->wait_termination();
  }
}

bool Scheduler::next() {
  if (!mq.empty()) {
    auto t = take();
    (*t.value())(t.value());
  }
  return mq.empty();
}

void Scheduler::run_to_empty() {
  while (next())
    ;
}

Scheduler &Scheduler::main() { return *_instance; }

Scheduler::Scheduler() {}

Scheduler::~Scheduler() {
  reset();
  stop();
}

Executor::Executor() {}

bool Executor::assign(sptr<Task> task) {
  std::lock_guard _{_lock};
  if (status == PROCESSING || status == WAITING_EXECUTION)
    return false;
  _currentExecution = task;
  status = WAITING_EXECUTION;
  onAssigned.Set();
  return true;
}

void Executor::start() {

  if (_executingThread != nullptr)
    return;
  _executingThread = new std::thread{[this]() {
    onCompleted.Reset();
    while (!_end) {
      auto ct = take();
      if (ct == nullptr) {
        status = IDLE;
        onAssigned.WaitOne();
        ct = take();
      }
      if (_end)
        break;
      onAssigned.Reset();
      status = PROCESSING;
      if (ct != nullptr) // accrocchio, controllare perchè
        (*ct)(ct);
      reset();
    }
    status = IDLE;
    onCompleted.Set();
  }};
  _executingThread->detach();
}

void Executor::wait_termination() { onCompleted.WaitOne(); }

void Executor::stop() {
  _end = true;
  onAssigned.Set();
  wait_termination();
}

Executor::~Executor() { _end = true; }

void intrusive_ptr_add_ref(Task *p) {}

void intrusive_ptr_add_ref(Executor *p) {}

void intrusive_ptr_release(Executor *p) {}
void intrusive_ptr_release(Task *p) {}

void Task::cancel() { resolve(); }

void Task::resolve(bool failed) {
  std::lock_guard l(_lock);
  _state = failed ? FAILED : RESOLVED;
  _executed.Set();
  if (failed && _onFail != nullptr)
    Scheduler::main().schedule(_onFail);
  else if (_onComplete != nullptr) {
    Scheduler::main().schedule(_onComplete);
  }
}
void Task::onComplete(sptr<Task> task) {
  std::lock_guard l(_lock);
  _onComplete = task;
  if (_state == RESOLVED) {
    Scheduler::main().schedule(_onComplete);
  }
}
void Task::onFail(sptr<Task> task) {
  std::lock_guard l(_lock);
  _onFail = task;
  if (_state == FAILED) {
    Scheduler::main().schedule(_onFail);
  }
}
