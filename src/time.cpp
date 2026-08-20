#include "time.hpp"
#include <chrono>

TimeBlockRegister::TimeBlockRegister()
{
}

TimeBlockRegister &TimeBlockRegister::instance()
{
	static TimeBlockRegister _instance{};
	return _instance;
}

TimeBlock::TimeBlock(std::string name) : name(name)
{
}

void TimeBlock::finalize()
{
}
