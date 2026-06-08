#pragma once
#include "Knapsack/knapsackcopa.hpp"
#include <algorithm>
#include <iterator>
#include <ranges>
#include <thread>
#include <vector>

template <std::ranges::input_range Range>
	requires(requires(typename Range::value_type v) {
		{ v > v } -> std::convertible_to<bool>;
		{ v >= v } -> std::convertible_to<bool>;
	})
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
		if (j > 0 && (k < n && A[j - 1] > B[k]))
		{
			auto step = (j - j_low + 1) / 2;
			k_low = k;
			j -= step;
			k += step;
		}
		else if (k > 0 && (j < m && B[k - 1] >= A[j]))
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
	}

	return {j, k};
}

template <std::ranges::input_range Range, std::ranges::output_range<const typename Range::value_type &> OutputRange>
void parallel_merge(const Range &A, const Range &B, OutputRange &output, int num_threads)
{
	using namespace std::ranges;
	int total_size = A.size() + B.size();
	#pragma parallel for num_threads(num_threads)
	for (int t = 0; t < num_threads; ++t)
	{

		int start = std::floor(t * total_size / num_threads);
		int end = std::floor((t + 1) * total_size / num_threads);
		auto [a_start, b_start] = co_rank(A, B, start);
		auto [a_end, b_end] = co_rank(A, B, end);
		std::merge(begin(A) + a_start, begin(A) + a_end, begin(B) + b_start, begin(B) + b_end,
				   std::inserter(output, begin(output) + start));
	}
}

template <std::ranges::input_range Range> std::vector<CopaSubset> generate_copa_subsets(Range &&r, int numthreads = 1)
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

		parallel_merge(subsets, shifted, newSubsets, numthreads);
		subsets = std::move(newSubsets);
		
	}
	return subsets;
}
