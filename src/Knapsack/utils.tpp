#pragma once
#include "Knapsack/knapsackcopa.hpp"
#include <algorithm>
#include <iterator>
#include <ranges>
#include <span>
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

template <std::ranges::input_range Range,
		  std::ranges::output_range<std::pair<std::span<const typename Range::value_type>, int>> OutputRange>
	requires(requires(typename Range::value_type v) {
		{ v.totalValue } -> std::convertible_to<int>;
	})
void divide_in_balanced_blocks(const Range &input, OutputRange& output, int num_threads)

{
	using ValueType = typename Range::value_type;
	int n = static_cast<int>(std::ranges::size(input));
	int k = std::max(1, num_threads);
	k = std::min(k, n);

	if (n == 0)
		return ;

	int block_size = n / k;
	int remainder = n % k;
	auto data = std::ranges::data(input);


#pragma omp parallel for num_threads(num_threads)
	for (int i = 0; i < k; ++i)
	{
		int start = i * block_size + std::min(i, remainder);
		int end = start + block_size + (i < remainder ? 1 : 0);
		if (start >= end)
		{
			output[i] = {std::span<const ValueType>{}, 0};
			continue;
		}

		auto span = std::span(data + start, static_cast<std::size_t>(end - start));
		int max_val = 0;
		for (const auto &elem : span)
		{
			if (elem.totalValue > max_val)
				max_val = elem.totalValue;
		}
		output[i] = {span, max_val};
	}

	return;
}

template <std::ranges::contiguous_range RangeA, std::ranges::contiguous_range RangeB,
		  std::ranges::output_range<
			  std::pair<std::span<const typename RangeA::value_type>, std::span<const typename RangeB::value_type>>>
			  OutputRange>
	requires(requires(typename RangeA::value_type a, typename RangeB::value_type b) {
		{ a.totalWeight } -> std::convertible_to<int>;
		{ a.totalValue } -> std::convertible_to<int>;
		{ b.totalWeight } -> std::convertible_to<int>;
		{ b.totalValue } -> std::convertible_to<int>;
	})
void prune(const RangeA &A, const RangeB &B, OutputRange &output, int capacity, int threads = 1)
{
	int k = std::max(1, threads);
	k = std::min(k, static_cast<int>(std::ranges::size(A)));
	k = std::min(k, static_cast<int>(std::ranges::size(B)));

	if (k <= 0 || capacity < 0)
	{
		return;
	}

	std::vector<std::pair<std::span<const typename RangeA::value_type>, int>> blocksA(k);
	std::vector<std::pair<std::span<const typename RangeB::value_type>, int>> blocksB(k);

	divide_in_balanced_blocks(A, blocksA, threads);
	divide_in_balanced_blocks(B, blocksB, threads);

	if (k <= 0 || capacity < 0)
	{
		return;
	}

	// Algorithm 4: Parallel pruning algorithm
	// Each processor Pi (i from 0 to k-1) checks block pairs (Ai, B_{j mod k}) for j = i to k+i-1
	std::vector<std::vector<
		std::pair<std::span<const typename RangeA::value_type>, std::span<const typename RangeB::value_type>>>>
		local_results(k);

#pragma omp parallel for num_threads(threads)
	for (int i = 0; i < k; ++i)
	{
		const auto &[Ai, maxA] = blocksA[i];
		if (Ai.empty())
			continue;

		int maxvalue_i = 0;

		for (int j = i; j < k + i; ++j)
		{
			int b_idx = j % k;
			const auto &[Bj, maxB] = blocksB[b_idx];
			if (Bj.empty())
				continue;

			int Z = Ai.front().totalWeight + Bj.back().totalWeight;
			int Y = Ai.back().totalWeight + Bj.front().totalWeight;

			if (Y <= capacity)
			{
				// All pairs in this block pair are valid; save max profit and prune
				if (maxA + maxB > maxvalue_i)
				{
					maxvalue_i = maxA + maxB;
				}
				// Prune block pair (Ai, B_{j mod k})
			}
			else if (Z <= capacity && Y > capacity)
			{
				// Some pairs may be valid; keep this block pair for further search
				local_results[i].emplace_back(Ai, Bj);
			}
			else if (Z > capacity)
			{
				// No pairs in this block pair are valid; prune
			}
		}
	}

	// Merge local results from all processors
	for (const auto &local : local_results)
	{
		output.insert(output.end(), local.begin(), local.end());
	}
}

template <std::ranges::input_range Range, std::ranges::output_range<const typename Range::value_type &> OutputRange>
void parallel_merge(const Range &A, const Range &B, OutputRange &output, int num_threads)
{
	using namespace std::ranges;
	int total_size = static_cast<int>(A.size() + B.size());

	if (num_threads <= 1 || total_size == 0)
	{
		std::merge(begin(A), end(A), begin(B), end(B), std::back_inserter(output));
		return;
	}

	// Pre-size output for safe random-access parallel writes
	if constexpr (requires(OutputRange &out, std::size_t sz) { out.resize(sz); })
	{
		output.resize(static_cast<std::size_t>(total_size));
	}
	else
	{
		// Fallback to sequential merge if output cannot be resized
		std::merge(begin(A), end(A), begin(B), end(B), std::back_inserter(output));
		return;
	}

#pragma omp parallel for num_threads(num_threads)
	for (int t = 0; t < num_threads; ++t)
	{
		int start = t * total_size / num_threads;
		int end = (t + 1) * total_size / num_threads;
		auto [a_start, b_start] = co_rank(A, B, start);
		auto [a_end, b_end] = co_rank(A, B, end);
		std::merge(begin(A) + a_start, begin(A) + a_end, begin(B) + b_start, begin(B) + b_end, begin(output) + start);
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

template <std::ranges::input_range Range, std::ranges::output_range<const typename Range::value_type &> OutputRange>
void parallel_save_max_value(Range &input, OutputRange &output, int num_threads)
{
	using ValueType = typename Range::value_type;
	int n = static_cast<int>(std::ranges::size(input));

	if (n == 0)
		return;

	int threads = std::max(1, num_threads);
	threads = std::min(threads, n);

	if (threads == 1)
	{
		// Sequential backward scan for suffix max
		std::vector<ValueType> result;
		result.reserve(n);

		auto it = std::ranges::begin(input);
		std::advance(it, n - 1);
		ValueType current_max = *it;
		result.push_back(current_max);

		for (int j = n - 2; j >= 0; --j)
		{
			--it;
			if (it->totalValue > current_max.totalValue)
			{
				current_max = *it;
			}
			result.push_back(current_max);
		}

		std::reverse(result.begin(), result.end());
		std::ranges::copy(result, std::back_inserter(output));
		return;
	}

	// Parallel version: divide into chunks and compute local suffix max
	std::vector<ValueType> result(n);
	std::vector<ValueType> chunk_maxes(threads);

	int chunk_size = n / threads;
	int remainder = n % threads;

#pragma omp parallel for num_threads(threads)
	for (int t = 0; t < threads; ++t)
	{
		int start = t * chunk_size + std::min(t, remainder);
		int end = start + chunk_size + (t < remainder ? 1 : 0);

		if (start >= end)
		{
			chunk_maxes[t] = ValueType{};
			continue;
		}

		auto it = std::ranges::begin(input);
		std::advance(it, end - 1);
		result[end - 1] = *it;
		ValueType local_max = *it;

		for (int j = end - 2; j >= start; --j)
		{
			--it;
			if (it->totalValue > local_max.totalValue)
			{
				local_max = *it;
			}
			result[j] = local_max;
		}

		chunk_maxes[t] = local_max;
	}

	// Compute suffix max of chunk maxes
	for (int t = threads - 2; t >= 0; --t)
	{
		if (chunk_maxes[t + 1].totalValue > chunk_maxes[t].totalValue)
		{
			chunk_maxes[t] = chunk_maxes[t + 1];
		}
	}

	// Adjust each chunk with the suffix max of subsequent chunks
	for (int t = threads - 2; t >= 0; --t)
	{
		int start = t * chunk_size + std::min(t, remainder);
		int end = start + chunk_size + (t < remainder ? 1 : 0);

		if (start >= end)
			continue;

		for (int j = start; j < end; ++j)
		{
			if (chunk_maxes[t + 1].totalValue > result[j].totalValue)
			{
				result[j] = chunk_maxes[t + 1];
			}
		}
	}

	std::ranges::copy(result, std::back_inserter(output));
}