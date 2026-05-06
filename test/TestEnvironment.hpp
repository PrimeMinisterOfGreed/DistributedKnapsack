
#include "Async/executor.hpp"
#include <gtest/gtest.h>

class TestEnvironment : public testing::Test {
public:
  virtual void SetUp() {}

  virtual void TearDown() { Scheduler::main().reset(); }
};