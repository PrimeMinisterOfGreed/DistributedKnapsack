#include "MultiThread/autoresetevent.hpp"
#include <future>
#include <gtest/gtest.h>
#include <unistd.h>

TEST(TestResettableEvents, test_wait_any) {
  AutoResetEvent e1{false};
  AutoResetEvent e2{false};
  auto result = std::async([&e1, &e2] { return WaitAny({&e1, &e2}); });
  e2.Set();
  ASSERT_EQ(1, result.get());
  e1.Set();
}

TEST(TestResettableEvents, test_wait_all) {
  int a = 0;
  AutoResetEvent e1{false};
  AutoResetEvent e2{false};
  std::thread t{[&e1, &e2, &a]() {
    e1.Set();
    sleep(1);
    a = 1;
    e2.Set();
  }};
  t.detach();
  WaitAll({&e1, &e2});
  ASSERT_EQ(1, a);
}