#include "time.hpp"

#include <cmath>
#include <spdlog/spdlog.h>

TimeSectionRegister &TimeSectionRegister::instance()
{
	static TimeSectionRegister reg;
	return reg;
}

bool TimeSectionRegister::acquire_ownership()
{
	std::thread::id expected{};
	return owner_id_.compare_exchange_strong(expected, std::this_thread::get_id()) ||
		   expected == std::this_thread::get_id();
}

void TimeSectionRegister::start(const std::string &name)
{
	if (!acquire_ownership())
	{
		return;
	}
	active_[name] = std::chrono::steady_clock::now();
}

void TimeSectionRegister::end(const std::string &name)
{
	if (owner_id_.load(std::memory_order_relaxed) != std::this_thread::get_id())
	{
		return;
	}
	auto it = active_.find(name);
	if (it == active_.end())
	{
		return;
	}
	auto now = std::chrono::steady_clock::now();
	double ms = std::chrono::duration<double, std::milli>(now - it->second).count();
	active_.erase(it);
	sections_[name](ms);
}

TimeSection TimeSectionRegister::get_section(const std::string &name) const
{
	auto it = sections_.find(name);
	if (it == sections_.end())
	{
		return TimeSection{};
	}
	const auto &sink = it->second;
	TimeSection result;
	result.min = boost::accumulators::min(sink);
	result.max = boost::accumulators::max(sink);
	result.mean = boost::accumulators::mean(sink);
	result.variance = boost::accumulators::variance(sink);
	result.count = boost::accumulators::count(sink);
	return result;
}

std::unordered_map<std::string, TimeSection> TimeSectionRegister::get_all_sections() const
{
	std::unordered_map<std::string, TimeSection> result;
	result.reserve(sections_.size());
	for (const auto &[name, sink] : sections_)
	{
		TimeSection ts;
		ts.min = boost::accumulators::min(sink);
		ts.max = boost::accumulators::max(sink);
		ts.mean = boost::accumulators::mean(sink);
		ts.variance = boost::accumulators::variance(sink);
		ts.count = boost::accumulators::count(sink);
		result.emplace(name, ts);
	}
	return result;
}

void TimeSectionRegister::report()
{
	for (const auto &[name, sink] : sections_)
	{
		std::size_t count = boost::accumulators::count(sink);
		if (count == 0)
		{
			continue;
		}
		double min = boost::accumulators::min(sink);
		double max = boost::accumulators::max(sink);
		double mean = boost::accumulators::mean(sink);
		double variance = boost::accumulators::variance(sink);
		spdlog::info("[{}] count={} mean={:.3f}ms min={:.3f}ms max={:.3f}ms stddev={:.3f}ms",
					 name, count, mean, min, max, std::sqrt(variance));
	}
}