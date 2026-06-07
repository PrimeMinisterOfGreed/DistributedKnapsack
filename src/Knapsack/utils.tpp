#pragma once
#include "Knapsack/knapsackcopa.hpp"
#include <algorithm>
#include <iterator>
#include <ranges>
#include <thread>
#include <vector>

template <typename It> struct overwrite_iterator
{
	It it;

	overwrite_iterator &operator=(const auto &value)
	{
		*it = value;
		return *this;
	}

	overwrite_iterator &operator*()
	{
		return *this;
	}
	overwrite_iterator &operator++()
	{
		++it;
		return *this;
	}
};

template <std::ranges::input_range Range>
	requires(std::totally_ordered<typename Range::value_type>)
std::pair<int, int> co_rank(const Range &A, const Range &B, int i)
{
	const int m = static_cast<int>(A.size());
	const int n = static_cast<int>(B.size());

	int j = std::min(i, m);
	int k = i - j;
	int j_low = std::max(0, i - n);
	int k_low = std::max(0, i - m);
	bool active = true;
	while (active)
	{
		if (j > 0 && (k < n && A[j-1]>B[k]))
		{
			auto step = (j - j_low + 1) / 2;
			k_low = k;
			j -= step;
			k += step;
		}
		else if (k > 0 && (j < m && B[k-1]>=A[j]))
		{
			auto step = (k - k_low + 1) / 2;
			j_low = j;
			k -= step;
			j += step;
		}
		else
		{
			active = false;
			continue;
		}

		return {j, k};
	}

	return {j, k};
}

template <std::ranges::input_range Range, std::ranges::output_range<const typename Range::value_type &> OutputRange>
void parallel_merge(const Range &A, const Range &B, OutputRange &output, int num_threads)
{
	using namespace std::ranges;
	int total_size = A.size() + B.size();
	for (int t = 0; t < num_threads; ++t)
	{

		int start = t * total_size / num_threads;
		int end = (t + 1) * total_size / num_threads;

		auto [a_start, b_start] = co_rank(A, B, start);
		auto [a_end, b_end] = co_rank(A, B, end);
		std::printf("Thread %d: A[%d:%d], B[%d:%d]\n", t, a_start, a_end, b_start, b_end);
		std::merge(A.begin() + a_start, A.begin() + a_end, B.begin() + b_start, B.begin() + b_end,
				   std::inserter(output, std::next(output.begin(), start)));
	}
}

template <std::ranges::input_range Range> std::vector<CopaSubset> generate_copa_subsets(Range &&r)
{
	std::vector<CopaSubset> subsets{CopaSubset{}};
	for (const auto &item : r)
	{
		std::vector<CopaSubset> shifted = subsets;
		auto [w, v] = item;

		for (auto &subset : shifted)
		{
			subset.items.push_back(v);
			subset.totalWeight += w;
			subset.totalValue += v;
		}

		std::vector<CopaSubset> newSubsets;
		newSubsets.reserve(subsets.size() + shifted.size());

		std::ranges::merge(subsets, shifted, std::back_inserter(newSubsets),
						   [](const CopaSubset &a, const CopaSubset &b) { return a.totalWeight < b.totalWeight; });

		subsets = std::move(newSubsets);
	}
	return subsets;
}
