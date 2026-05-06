#include "Async/async.hpp"
#include "Async/executor.hpp"
#include "traits.hpp"
#include "TestEnvironment.hpp"


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <md5.hpp>

class TestPromise : public TestEnvironment {};

Scheduler &sched() { return Scheduler::main(); }



int calc(int p) { return p + 1; }
