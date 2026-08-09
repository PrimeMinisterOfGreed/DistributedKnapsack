#include "Knapsack/utils.hxx"
#include <algorithm>
#include <gtest/gtest.h>
#include <vector>

TEST(ParallelMerge, MergesAdjacentSortedRangesCorrectlySingleThread)
{
	std::vector<int> a{1, 4, 9};
	std::vector<int> b{2, 3, 8, 10};

	// put both ranges into a single contiguous container as expected by parallel_merge
	std::vector<int> data;
	data.resize(a.size() + b.size());
	parallel_merge(a, b, data, 1);

	std::vector<int> expected{1, 2, 3, 4, 8, 9, 10};
	EXPECT_EQ(data, expected);
}

TEST(ParallelMerge, MergesAdjacentSortedRangesCorrectlyMultiThread)
{
	std::vector<int> a{0, 5, 6, 11};
	std::vector<int> b{1, 2, 3, 7, 12};

	std::vector<int> data;
	data.resize(a.size() + b.size());
	parallel_merge(a, b, data, 2);

	std::vector<int> expected{0, 1, 2, 3, 5, 6, 7, 11, 12};
	EXPECT_EQ(data, expected);
}

TEST(DivideInBalancedBlocks, SingleThreadDividesCorrectly)
{
	std::vector<CopaSubset> input{CopaSubset{{}, 1, 10}, CopaSubset{{}, 2, 5}, CopaSubset{{}, 3, 20},
								  CopaSubset{{}, 4, 15}};

	std::vector<CopaBlock> blocks(1);
	distribute_block_per_processor(input, blocks, 1);

	ASSERT_EQ(blocks.size(), 1u);
	EXPECT_EQ(blocks[0].block.size(), 4u);
	EXPECT_EQ(blocks[0].maxValue, 20);
}

TEST(DivideInBalancedBlocks, MultiThreadDividesCorrectly)
{
	std::vector<CopaSubset> input{CopaSubset{{}, 1, 10}, CopaSubset{{}, 2, 5}, CopaSubset{{}, 3, 20},
								  CopaSubset{{}, 4, 15}};

	std::vector<CopaBlock> blocks(2);
	distribute_block_per_processor(input, blocks, 2);

	ASSERT_EQ(blocks.size(), 2u);
	EXPECT_EQ(blocks[0].block.size(), 2u);
	EXPECT_EQ(blocks[1].block.size(), 2u);
	EXPECT_EQ(blocks[0].maxValue, 10);
	EXPECT_EQ(blocks[1].maxValue, 20);
}

TEST(DivideInBalancedBlocks, EmptyInputDoesNotModifyOutput)
{
	std::vector<CopaSubset> input;
	std::vector<CopaBlock> blocks(2);
	blocks[0] = {std::span<const CopaSubset>{}, 42};
	blocks[1] = {std::span<const CopaSubset>{}, 99};
	distribute_block_per_processor(input, blocks, 2);
	EXPECT_EQ(blocks[0].maxValue, 42);
	EXPECT_EQ(blocks[1].maxValue, 99);
}

TEST(DivideInBalancedBlocks, UnevenDivisionHandlesRemainder)
{
	std::vector<CopaSubset> input{CopaSubset{{}, 1, 10}, CopaSubset{{}, 2, 20}, CopaSubset{{}, 3, 30},
								  CopaSubset{{}, 4, 40}, CopaSubset{{}, 5, 50}};

	std::vector<CopaBlock> blocks(2);
	distribute_block_per_processor(input, blocks, 2);

	ASSERT_EQ(blocks.size(), 2u);
	EXPECT_EQ(blocks[0].block.size(), 3u);
	EXPECT_EQ(blocks[1].block.size(), 2u);
	EXPECT_EQ(blocks[0].maxValue, 30);
	EXPECT_EQ(blocks[1].maxValue, 50);
}

// ============================================================================
// Performance and correctness tests for generate_copa_subsets
// ============================================================================

/**
 * @brief Verify that a vector of CopaSubset is sorted by totalWeight ascending.
 */
bool is_sorted_by_weight(const std::vector<CopaSubset> &subsets)
{
	for (size_t i = 1; i < subsets.size(); ++i)
	{
		if (subsets[i - 1].totalWeight > subsets[i].totalWeight)
			return false;
	}
	return true;
}

/**
 * @brief Compute total weight from an exhaustive enumeration for small n (reference).
 */
int64_t reference_total_weight(const std::vector<std::pair<int, int>> &items)
{
	int64_t total = 0;
	int n = static_cast<int>(items.size());
	for (int mask = 0; mask < (1 << n); ++mask)
	{
		int w = 0;
		for (int i = 0; i < n; ++i)
		{
			if (mask & (1 << i))
				w += items[i].first;
		}
		total += w;
	}
	return total;
}

TEST(GenerateCopaSubsets_Correctness, TwoItemsProducesFourSubsets)
{
	std::vector<std::pair<int, int>> items{{2, 5}, {3, 7}};
	auto subsets = generate_copa_subsets(items, 1);

	EXPECT_EQ(subsets.size(), 4u);

	// Check sorted order by weight
	EXPECT_TRUE(is_sorted_by_weight(subsets));

	// Weights: {}, {2}, {3}, {2+3=5}
	EXPECT_EQ(subsets[0].totalWeight, 0);
	EXPECT_EQ(subsets[1].totalWeight, 2);
	EXPECT_EQ(subsets[2].totalWeight, 3);
	EXPECT_EQ(subsets[3].totalWeight, 5);

	// Values: {}, {5}, {7}, {5+7=12}
	EXPECT_EQ(subsets[0].totalValue, 0);
	EXPECT_EQ(subsets[1].totalValue, 5);
	EXPECT_EQ(subsets[2].totalValue, 7);
	EXPECT_EQ(subsets[3].totalValue, 12);
}

TEST(GenerateCopaSubsets_Correctness, ThreeItemsProducesEightSubsets)
{
	std::vector<std::pair<int, int>> items{{1, 10}, {2, 20}, {4, 30}};
	auto subsets = generate_copa_subsets(items, 1);

	EXPECT_EQ(subsets.size(), 8u);
	EXPECT_TRUE(is_sorted_by_weight(subsets));

	// Expected weights sorted: 0,1,2,1+2=3,4,1+4=5,2+4=6,1+2+4=7
	std::vector<int> expected_weights{0, 1, 2, 3, 4, 5, 6, 7};
	std::vector<int> got_weights;
	for (const auto &s : subsets)
		got_weights.push_back(s.totalWeight);
	EXPECT_EQ(got_weights, expected_weights);
}

TEST(GenerateCopaSubsets_Correctness, MultiThreadMatchesSingleThread)
{
	std::vector<std::pair<int, int>> items{{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}};
	auto seq = generate_copa_subsets(items, 1);
	auto par = generate_copa_subsets(items, 4);

	ASSERT_EQ(seq.size(), par.size());
	ASSERT_EQ(seq.size(), 32u);

	for (size_t i = 0; i < seq.size(); ++i)
	{
		EXPECT_EQ(seq[i].totalWeight, par[i].totalWeight) << "Mismatch at index " << i;
		EXPECT_EQ(seq[i].totalValue, par[i].totalValue) << "Mismatch at index " << i;
	}

	EXPECT_TRUE(is_sorted_by_weight(seq));
	EXPECT_TRUE(is_sorted_by_weight(par));
}

TEST(GenerateCopaSubsets_Correctness, EmptyInputReturnsSingleEmptySubset)
{
	std::vector<std::pair<int, int>> items;
	auto subsets = generate_copa_subsets(items, 1);
	ASSERT_EQ(subsets.size(), 1u);
	EXPECT_EQ(subsets[0].totalWeight, 0);
	EXPECT_EQ(subsets[0].totalValue, 0);
}

TEST(GenerateCopaSubsets_Correctness, ReverseOrderFlagWorks)
{
	std::vector<std::pair<int, int>> items{{1, 1}, {2, 2}};
	auto forward = generate_copa_subsets(items, 1, false);
	auto reversed = generate_copa_subsets(items, 1, true);

	ASSERT_EQ(forward.size(), reversed.size());
	// forward is ascending by weight, reversed is descending
	for (size_t i = 0; i < forward.size(); ++i)
	{
		EXPECT_EQ(forward[i].totalWeight, reversed[forward.size() - 1 - i].totalWeight);
		EXPECT_EQ(forward[i].totalValue, reversed[forward.size() - 1 - i].totalValue);
	}
}

// ============================================================================
// Cross-validation: total weight sum across all subsets
// ============================================================================
class GenerateCopaSubsetsTimed : public testing::Test
{
  protected:
	// Pre-built items of various sizes
	std::vector<std::pair<int, int>> make_items(int n)
	{
		std::vector<std::pair<int, int>> items;
		items.reserve(n);
		// Use deterministic values: weight=i+1, value=(i+1)*10
		for (int i = 0; i < n; ++i)
		{
			items.emplace_back(i + 1, (i + 1) * 10);
		}
		return items;
	}
};

TEST_F(GenerateCopaSubsetsTimed, TotalWeightSumMatchesReference)
{
	// For n items with weights w_i, each weight appears in exactly 2^(n-1) subsets
	// Total weight sum = 2^(n-1) * sum(w_i)
	auto items = make_items(6);
	int64_t expected = (1LL << (items.size() - 1)) * static_cast<int64_t>(items.size() * (items.size() + 1) / 2);

	auto subsets = generate_copa_subsets(items, 2);
	int64_t total = 0;
	for (const auto &s : subsets)
		total += s.totalWeight;

	EXPECT_EQ(total, expected);
	EXPECT_TRUE(is_sorted_by_weight(subsets));
}

TEST(DivideInBalancedBlocks, BlocksReferenceOriginalData)
{
	std::vector<CopaSubset> input{CopaSubset{{}, 1, 10}, CopaSubset{{}, 2, 20}, CopaSubset{{}, 3, 30}};

	std::vector<CopaBlock> blocks(3);
	distribute_block_per_processor(input, blocks, 3);

	ASSERT_EQ(blocks.size(), 3u);
	EXPECT_EQ(&blocks[0].block.front(), &input[0]);
	EXPECT_EQ(&blocks[1].block.front(), &input[1]);
	EXPECT_EQ(&blocks[2].block.front(), &input[2]);
}
