#pragma once
#include <chrono>
#include <future>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#ifndef ENABLE_TIMERS
#define ENABLE_TIMERS 0
#endif
#include <concepts>
#include <thread>

struct TimeBlock
{
	std::string name;
	std::chrono::milliseconds ms;
	TimeBlock(std::string name);
	void finalize();
};

struct TimeBlockRegister
{
  private:
	std::vector<TimeBlock> _blocks;
	TimeBlockRegister();

  public:
	TimeBlockRegister &instance();
	std::vector<TimeBlock> &blocks()
	{
		return _blocks;
	}
};
