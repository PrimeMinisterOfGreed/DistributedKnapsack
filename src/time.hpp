#pragma once

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <unordered_map>

#ifndef ENABLE_TIMERS
#define ENABLE_TIMERS 0
#endif

struct TimeSection
{
	double min{0.0};
	double max{0.0};
	double mean{0.0};
	double variance{0.0};
	std::size_t count{0};
};

struct TimeSectionRegister
{
	using sink_type = boost::accumulators::accumulator_set<
		double, boost::accumulators::stats<boost::accumulators::tag::count,
										   boost::accumulators::tag::mean,
										   boost::accumulators::tag::min,
										   boost::accumulators::tag::max,
										   boost::accumulators::tag::variance>>;

	static TimeSectionRegister &instance();

	void start(const std::string &name);
	void end(const std::string &name);
	void report();
	TimeSection get_section(const std::string &name) const;
	std::unordered_map<std::string, TimeSection> get_all_sections() const;

private:
	bool acquire_ownership();
	std::unordered_map<std::string, sink_type> sections_;
	std::unordered_map<std::string, std::chrono::steady_clock::time_point> active_;
	std::atomic<std::thread::id> owner_id_{std::thread::id{}};
};

#if ENABLE_TIMERS
#define START_BLOCK(name) TimeSectionRegister::instance().start(name)
#define END_BLOCK(name) TimeSectionRegister::instance().end(name)
#else
#define START_BLOCK(name) ((void)0)
#define END_BLOCK(name) ((void)0)
#endif