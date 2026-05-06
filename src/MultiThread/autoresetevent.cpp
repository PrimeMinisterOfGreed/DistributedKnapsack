#include "autoresetevent.hpp"
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

void ResettableEvent::Set() {
  std::lock_guard<std::mutex> l(_lock);
  _flag = true;
  _var.notify_one();
  if (_setHandler != nullptr) {
    (*_setHandler)();
    _setHandler = nullptr;
  }
}

void ResettableEvent::Reset() {
  std::lock_guard<std::mutex> l(_lock);
  _flag = false;
}

void ResettableEvent::WaitOne() {
  std::unique_lock<std::mutex> l(_lock);
  while (!_flag) {
    _var.wait(l);
  }
  if (_autoreset)
    _flag = false;
}

void ResettableEvent::OnSet(std::function<void()> handler) {
  std::lock_guard l(_lock);
  if (_flag) {
    handler();
  } else {
    _setHandler =
        sptr<std::function<void()>>(new std::function<void()>(handler));
  }
}

ResettableEvent::ResettableEvent(bool Autoreset, bool initialState) {
  _autoreset = Autoreset;
  _flag = initialState;
}

int WaitAny(std::vector<WaitHandle *> handles) {
  auto result = std::make_shared<int>(-1);
  auto op = std::make_shared<size_t>(handles.size());
  AutoResetEvent onresolve{false};
  auto lock = std::make_shared<std::mutex>();
  for (int i = 0; i < handles.size(); i++) {
    handles[i]->OnSet([result, lock, i, &onresolve, op]() {
      lock->lock();
      (*op)--;
      if (result != nullptr && *result == -1) {
        onresolve.Set();
        *result = i;
      }
      lock->unlock();
    });
  }
  onresolve.WaitOne();
  return *result;
};

void WaitAll(std::vector<WaitHandle *> handles) {
  auto op = handles.size();
  AutoResetEvent onresolve{false};
  std::mutex lock{};
  for (auto h : handles) {
    h->OnSet([&op, &onresolve, &lock] {
      std::lock_guard l{lock};
      op--;
      if (op == 0) {
        onresolve.Set();
      }
    });
  }
  onresolve.WaitOne();
}
