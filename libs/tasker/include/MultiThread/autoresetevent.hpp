#pragma once
#include "traits.hpp"
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

class WaitHandle {
  friend int WaitAny(std::vector<WaitHandle *> handles);

protected:
  virtual void Reset() = 0;
  virtual void Set() = 0;
  virtual std::mutex &GetMutex() = 0;

public:
  virtual void WaitOne() = 0;
  virtual void OnSet(std::function<void()> handler) = 0;
};
class ResettableEvent : public WaitHandle {

private:
  std::mutex _lock;
  std::condition_variable _var;
  bool _flag;
  bool _autoreset = false;
  std::shared_ptr<std::function<void()>> _setHandler = nullptr;

protected:
  virtual std::mutex &GetMutex() override { return _lock; }

public:
  virtual void Set() override;
  virtual void Reset() override;
  virtual void WaitOne() override;
  virtual void OnSet(std::function<void()> handler) override;
  ResettableEvent(bool Autoreset, bool initialState);
  ~ResettableEvent() { Set(); }
};

class ManualResetEvent : public ResettableEvent {
public:
  ManualResetEvent(bool initialState) : ResettableEvent(false, initialState) {}
};

class AutoResetEvent : public ResettableEvent {
public:
  AutoResetEvent(bool initialState) : ResettableEvent(true, initialState) {}
};

int WaitAny(std::vector<WaitHandle *> handles);
void WaitAll(std::vector<WaitHandle *> handles);