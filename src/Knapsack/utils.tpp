#pragma once
#include "Knapsack/knapsackcopa.hpp"
#include <algorithm>
#include <iterator>
#include <ranges>
#include <span>
#include <thread>
#include <vector>

#pragma region Concepts

struct CopaBlock
{
	std::span<const CopaSubset> block;
	int maxValue;
};
template <typename T>
concept CopaItem = requires(T v) {
	{ v.totalWeight } -> std::convertible_to<int>;
	{ v.totalValue } -> std::convertible_to<int>;
};

template <typename Range>
concept CopaRange = std::ranges::input_range<Range> && CopaItem<typename Range::value_type>;

template <typename Range>
concept CopaOutputRange = std::ranges::output_range<Range, const typename Range::value_type>;

template <typename Range>
concept CopaBlockOutputRange = std::ranges::output_range<Range, CopaBlock>;

template<typename CorankComparable>
concept CoRankComparable = requires(CorankComparable a, CorankComparable b) {
	{ a > b } -> std::convertible_to<bool>;
	{ a >= b } -> std::convertible_to<bool>;
};

template<typename CorankOrderableRange>
concept CoRankOrderableRange = std::ranges::input_range<CorankOrderableRange> && CoRankComparable<typename CorankOrderableRange::value_type>;

#pragma endregion


#pragma region Generation

/**
 * @brief Computes the co-rank partition index for two sorted ranges.
 *
 * Determines the split point where the first @p i merged elements come from
 * the two input ranges while preserving sorted order.
 *
 * @tparam Range
 * @param v
 * @return
 */

std::pair<int, int> co_rank(const CoRankOrderableRange auto &A, const CoRankOrderableRange auto  &B, int i)
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

/**
 * @brief Merges two sorted ranges in parallel using co-ranking to partition the work
 *
 * @tparam Range Input range type
 * @tparam OutputRange Output range type
 * @param A First sorted range in ascending order
 * @param B Second sorted range in ascending order
 * @param output Merged output range
 * @param num_threads Number of threads for parallel execution
 */
template <std::ranges::input_range Range, std::ranges::output_range<const typename Range::value_type> OutputRange>
void parallel_merge(const Range &A, const Range &B, OutputRange &output, int num_threads)
{
	using namespace std::ranges;
	int total_size = static_cast<int>(A.size() + B.size());

	if (num_threads <= 1 || total_size == 0)
	{
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

/**
 * @brief Generate CopaSubset combinations from an input range of (weight, value) items.
 *
 * This function constructs all subsets (like a powerset) of the provided items,
 * producing a vector of CopaSubset where each entry contains the items selected
 * along with their totalWeight and totalValue. Merging of intermediate subset
 * lists can be done in parallel via the numthreads parameter.
 *
 * @tparam Range The input range type yielding pairs (weight, value).
 * @param r Input range of items (weight, value).
 * @param numthreads Number of threads to use for parallel merging (default 1).
 * @return std::vector<CopaSubset> Vector of all generated subsets.
 */
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
		newSubsets.resize(subsets.size() + shifted.size());

		parallel_merge(subsets, shifted, newSubsets, numthreads);
		subsets = std::move(newSubsets);
	}
	return subsets;
}

#pragma endregion

#pragma region Optimizers



void distribute_block_per_processor(const CopaRange auto &input, CopaBlockOutputRange auto &output, int num_threads)
{
	int n = static_cast<int>(std::ranges::size(input));
	int k = std::max(1, num_threads);
	k = std::min(k, n);

	if (n == 0)
		return;

	int block_size = n / k;
	int remainder = n % k;

#pragma omp parallel for num_threads(num_threads)
	for (int i = 0; i < k; ++i)
	{
		int start = i * block_size + std::min(i, remainder);
		int end = start + block_size + (i < remainder ? 1 : 0);
		if (start >= end)
		{
			output[i] = {std::span<const CopaSubset>{}, 0};
			continue;
		}

		auto span = std::ranges::subrange(std::ranges::begin(input) + start, std::ranges::begin(input) + end);
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



void prune(const std::ranges::input_range auto &A, const std::ranges::input_range auto &B,
		   CopaBlockOutputRange auto &Aout, CopaBlockOutputRange auto &Bout, int capacity, int threads = 1)
{
	int k = std::max(1, threads);
	k = std::min(k, static_cast<int>(std::ranges::size(A)));
	k = std::min(k, static_cast<int>(std::ranges::size(B)));

	if (k <= 0 || capacity < 0)
	{
		return;
	}

	std::vector<CopaBlock> blocksA(k);
	std::vector<CopaBlock> blocksB(k);

	distribute_block_per_processor(A, blocksA, threads);
	distribute_block_per_processor(B, blocksB, threads);

	if (k <= 0 || capacity < 0)
	{
		return;
	}

	// Algorithm 4: Parallel pruning algorithm
	// Each processor Pi (i from 0 to k-1) checks block pairs (Ai, B_{j mod k}) for j = i to k+i-1
	std::vector<std::vector<std::pair<CopaBlock, CopaBlock>>> local_results(k);

#pragma omp parallel for num_threads(threads)
	for (int i = 0; i < k; ++i)
	{
		const auto &blockA = blocksA[i];
		if (blockA.block.empty())
			continue;

		int maxvalue_i = 0;

		for (int j = i; j < k + i; ++j)
		{
			int b_idx = j % k;
			const auto &blockB = blocksB[b_idx];
			if (blockB.block.empty())
				continue;

			int Z = blockA.block.front().totalWeight + blockB.block.back().totalWeight;
			int Y = blockA.block.back().totalWeight + blockB.block.front().totalWeight;

			if (Y <= capacity)
			{
				// All pairs in this block pair are valid; save max profit and prune
				if (blockA.maxValue + blockB.maxValue > maxvalue_i)
				{
					maxvalue_i = blockA.maxValue + blockB.maxValue;
				}
				// Prune block pair (Ai, B_{j mod k})
			}
			else if (Z <= capacity && Y > capacity)
			{
				// Some pairs may be valid; keep this block pair for further search
				local_results[i].emplace_back(blockA, blockB);
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
		for (const auto &[blockA, blockB] : local)
		{
			Aout.push_back(blockA);
			Bout.push_back(blockB);
		}
	}
}

template <std::ranges::input_range Range, std::ranges::output_range<const typename Range::value_type> OutputRange>
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

#pragma endregion