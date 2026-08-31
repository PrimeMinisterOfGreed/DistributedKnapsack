#pragma once
#include "Knapsack/knapsackcopa.hpp"
#include "concepts.hxx"
#include "panic.hpp"
#include "time.hpp"
#include <algorithm>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <thread>
#include <vector>

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

std::pair<int, int> co_rank(const CoRankOrderableRange auto &A, const CoRankOrderableRange auto &B, int i)
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
	constexpr int MIN_PARALLEL_SIZE = 1024;
	using namespace std::ranges;
	int total_size = static_cast<int>(A.size() + B.size());

	if (total_size < MIN_PARALLEL_SIZE || num_threads <= 1)
	{
		std::merge(begin(A), end(A), begin(B), end(B), begin(output));
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
std::vector<CopaSubset> generate_copa_subsets(std::ranges::input_range auto &&r, int numthreads = 1,
											  bool reverseorder = false)
{
	// Materialize the (weight, value) items so we can preallocate buffers sized
	// to the final powerset (2^num_items) before generation begins.
	std::vector<std::pair<int, int>> items;
	for (auto &&item : r)
	{
		auto [w, v] = item;
		items.emplace_back(w, v);
	}

	const std::size_t num_items = items.size();
	if (num_items == 0)
		return {CopaSubset{}};

	constexpr std::size_t MIN_PARALLEL_SIZE = 1024;
	const std::size_t total = std::size_t{1} << num_items;

	// Ping-pong buffers sized to the final result plus a scratch buffer reused
	// for the "shifted" list, eliminating per-item allocations.
	std::vector<CopaSubset> buf0(total);
	std::vector<CopaSubset> buf1(total);
	std::vector<CopaSubset> scratch(total / 2);
	std::vector<CopaSubset> *cur = &buf0;
	std::vector<CopaSubset> *nxt = &buf1;
	(*cur)[0] = CopaSubset{};
	std::size_t size = 1;

	// Per-item step (two passes): build the "take" list shifted[k] = cur[k]+delta,
	// then merge cur with shifted. Adding the same constant (w,v) to every subset
	// preserves sorted order, so both lists remain sorted by totalWeight.
	for (std::size_t item_idx = 0; item_idx < num_items; ++item_idx)
	{
		const int w = items[item_idx].first;
		const int v = items[item_idx].second;

		// Pass 1: parallel add -> shifted.
		START_BLOCK("GenerateCopaSubset::AddItem");
		{
			std::span<CopaSubset> shifted(scratch.data(), size);
			if (size >= MIN_PARALLEL_SIZE && numthreads > 1)
			{
#pragma omp parallel for num_threads(numthreads)
				for (int i = 0; i < static_cast<int>(size); ++i)
				{
					shifted[static_cast<std::size_t>(i)] = (*cur)[static_cast<std::size_t>(i)];
					shifted[static_cast<std::size_t>(i)].addItem(static_cast<int>(item_idx), w, v);
				}
			}
			else
			{
				for (std::size_t i = 0; i < size; ++i)
				{
					shifted[i] = (*cur)[i];
					shifted[i].addItem(static_cast<int>(item_idx), w, v);
				}
			}
		}
		END_BLOCK("GenerateCopaSubset::AddItem");

		// Pass 2: parallel merge -> nxt (coarse co-rank slices, one per thread).
		START_BLOCK("GenerateCopaSubset::Merge");
		{
			auto cur_span = std::span<CopaSubset>(cur->data(), size);
			auto shifted_span = std::span<CopaSubset>(scratch.data(), size);
			auto out_span = std::span<CopaSubset>(nxt->data(), 2 * size);
			if (2 * size >= MIN_PARALLEL_SIZE && numthreads > 1)
			{
				parallel_merge(cur_span, shifted_span, out_span, numthreads);
			}
			else
			{
				std::merge(cur_span.begin(), cur_span.end(), shifted_span.begin(), shifted_span.end(),
						   out_span.begin());
			}
		}
		END_BLOCK("GenerateCopaSubset::Merge");

		std::swap(cur, nxt);
		size *= 2;
	}

	if (reverseorder)
	{
		std::reverse(cur->begin(), cur->end());
	}
	for (std::size_t i = 0; i < size; ++i)
	{
		(*cur)[i].index = static_cast<int>(i);
	}
	return std::move(*cur);
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

/**
 * @brief Prunes pairs of blocks based on capacity constraints.
 *
 * @param blockA  blockA to evaluate
 * @param blocksB ranges of blocks B to evaluate
 * @param remaining output range of remaining block pairs after pruning
 * @param capacity capacity
 * @param i optional parameter for mt functions, can be used to avoid parallel access to the same memory cell
 */
constexpr void prune_block_pair(const CopaBlock &blockA, const CopaBlockInputRange auto &blocksB,
								CopaBlockPairingOutRange auto &remaining, int capacity, int i = 0)
{
	int k = static_cast<int>(blocksB.size());
	int best_value = 0;
	for (int j = i; j < k + i; ++j)
	{
		int b_idx = j % k;
		const auto &blockB = blocksB[b_idx];
		if (blockB.block.empty())
			continue;
		int Z = blockA.block.front().totalWeight + blockB.block.back().totalWeight;
		int Y = blockA.block.back().totalWeight + blockB.block.front().totalWeight;

		// Note: prune is done by not adding the pair to local_results
		if (Y <= capacity)
		{
			// All pairs in this block pair are valid; save max profit and prune
			if (blockA.maxValue + blockB.maxValue > best_value)
			{
				best_value = blockA.maxValue + blockB.maxValue;
			}
			remaining.emplace_back(blockA, blockB);
		}
		else if (Z <= capacity && Y > capacity)
		{
			remaining.emplace_back(blockA, blockB);
		}
		else if (Z > capacity)
		{
			// No pairs in this block pair are valid; prune
		}
	}
}

/**
 * @brief Compute suffix max values and their global B-indices for a B block.
 *
 * Implements Algorithm 5 from the COPA paper (second parallel saving max-value stage).
 * For each position j in the block (0-indexed), computes the maximum totalValue
 * from position j to the end, and records the global B-array index where that max
 * is achieved.
 *
 * @param block The B block span (sorted in nonincreasing order of weight)
 * @param globalStartIdx The starting index of this block in the full B array
 * @param suffixMaxValues Output: suffix max totalValue at each position
 * @param suffixMaxIndices Output: global B-index achieving that suffix max at each position
 * @warning This function is intended to be called by each thread on its assigned block; it does not perform any
 * parallelization itself.
 */
constexpr inline void block_suffix_max_values(const CopaRange auto &block,
											  std::ranges::output_range<int> auto &suffixMaxValues,
											  std::ranges::output_range<int> auto &suffixMaxIndices)
{
	int e = static_cast<int>(block.size());
	if (e == 0)
		return;

	suffixMaxValues[e - 1] = block[e - 1].totalValue;
	suffixMaxIndices[e - 1] = block[e - 1].index;

	for (int j = e - 2; j >= 0; --j)
	{
		if (block[j].totalValue > suffixMaxValues[j + 1])
		{
			suffixMaxValues[j] = block[j].totalValue;
			suffixMaxIndices[j] = block[j].index;
		}
		else
		{
			suffixMaxValues[j] = suffixMaxValues[j + 1];
			suffixMaxIndices[j] = suffixMaxIndices[j + 1];
		}
	}
}

struct BlockPairSearchResult
{
	int bestVal;
	int bestAIdx;
	int bestBIdx;

	BlockPairSearchResult() : bestVal(0), bestAIdx(0), bestBIdx(0)
	{
	}

	BlockPairSearchResult(int val, int aIdx, int bIdx) : bestVal(val), bestAIdx(aIdx), bestBIdx(bIdx)
	{
	}

	template <typename Archive> void serialize(Archive &ar, const unsigned int)
	{
		ar & bestVal;
		ar & bestAIdx;
		ar & bestBIdx;
	}

	virtual bool operator>(const BlockPairSearchResult &other) const
	{
		return bestVal > other.bestVal;
	}

	virtual bool operator<(const BlockPairSearchResult &other) const
	{
		return bestVal < other.bestVal;
	}
};

constexpr BlockPairSearchResult block_pair_pointer_search(const CopaBlock &blockA, const CopaBlock &blockB,
														  int capacity)
{

	int blocka_size = static_cast<int>(blockA.block.size());
	int blockb_size = static_cast<int>(blockB.block.size());
	// Stage 4: Suffix max for this B block
	std::vector<int> suffixMaxVal(blockb_size);
	std::vector<int> suffixMaxIdx(blockb_size);
	block_suffix_max_values(blockB.block, suffixMaxVal, suffixMaxIdx);

	int x = 0, y = 0;
	int bestVal = 0, bestA = 0, bestB = 0;
	while (x < blocka_size && y < blockb_size)
	{
		if (blockA.block[x].totalWeight + blockB.block[y].totalWeight > capacity)
		{
			y++;
			continue;
		}
		int candidate = blockA.block[x].totalValue + suffixMaxVal[y];
		if (candidate > bestVal)
		{
			bestVal = candidate;
			bestA = blockA.block[x].index;
			bestB = suffixMaxIdx[y];
		}
		x++;
	}
	return {bestVal, bestA, bestB};
}

#pragma endregion

#pragma region MTFunctions

void prune(const CopaBlockInputRange auto &blocksA, const CopaBlockInputRange auto &blocksB,
		   CopaBlockPairingOutRange auto &remaining, int capacity, int threads = 1)
{

	// Algorithm 4: Parallel pruning algorithm
	// Each processor Pi (i from 0 to k-1) checks block pairs (Ai, B_{j mod k}) for j = i to k+i-1
	std::vector<std::vector<std::pair<CopaBlock, CopaBlock>>> local_results(threads);
#pragma omp parallel for num_threads(threads)
	for (int i = 0; i < threads; ++i)
	{
		const auto &blockA = blocksA[i];
		prune_block_pair(blockA, blocksB, local_results[i], capacity, i);
	}

	// Merge local results from all processors
	for (const auto &local : local_results)
	{
		remaining.insert(remaining.end(), local.begin(), local.end());
	}
}

void parallel_save_max(const CopaBlockPairingOutRange auto &remainingPairs,
					   std::ranges::output_range<int> auto &processBestVal,
					   std::ranges::output_range<int> auto &processBestAIdx,
					   std::ranges::output_range<int> auto &processBestBIdx, int capacity)
{
	int m = static_cast<int>(remainingPairs.size());

#pragma omp parallel for
	for (int i = 0; i < m; ++i)
	{
		const auto &[blockA, blockB] = remainingPairs[i];

		// Stage 5: Two-pointer search within this block pair
		BlockPairSearchResult result = block_pair_pointer_search(blockA, blockB, capacity);
		processBestVal[i] = result.bestVal;
		processBestAIdx[i] = result.bestAIdx;
		processBestBIdx[i] = result.bestBIdx;
	}
}

#pragma endregion
