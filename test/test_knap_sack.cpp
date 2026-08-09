#include "Knapsack/knapsack.hpp"
#include "Knapsack/knapsackcopa.hpp"
#include "Knapsack/utils.hxx"
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <random>
#include <vector>
#include <gtest/gtest.h>
TEST(KnapsackDP, MultipleThreadsFindOptimalValue)
{
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;
	auto result = knapsackdp(weights, values, capacity, 4);
	fmt::println("Included items: {}", fmt::join(result.items, ", "));
	EXPECT_EQ(result.totalValue, 26);
}

TEST(Prune, SingleThreadKeepsValidBlockPairs)
{
	std::vector<CopaSubset> A{CopaSubset{{}, 1, 10}, CopaSubset{{}, 3, 30}, CopaSubset{{}, 5, 50},
							  CopaSubset{{}, 7, 70}};

	std::vector<CopaSubset> B{CopaSubset{{}, 8, 80}, CopaSubset{{}, 6, 60}, CopaSubset{{}, 4, 40},
							  CopaSubset{{}, 2, 20}};

	std::vector<std::pair<CopaBlock, CopaBlock>> blocks;
	std::vector<CopaBlock> blocksA(1);
	std::vector<CopaBlock> blocksB(1);
	distribute_block_per_processor(A, blocksA, 1);
	distribute_block_per_processor(B, blocksB, 1);
	prune(blocksA, blocksB, blocks, 10, 1);
	ASSERT_EQ(blocks.size(), 1u);

	// Check A block (entire A array with 1 thread)
	EXPECT_EQ(blocks[0].first.block.size(), 4u);
	EXPECT_EQ(blocks[0].first.block[0].totalWeight, 1);
	EXPECT_EQ(blocks[0].first.block[0].totalValue, 10);
	EXPECT_EQ(blocks[0].first.block[3].totalWeight, 7);
	EXPECT_EQ(blocks[0].first.block[3].totalValue, 70);
	EXPECT_EQ(blocks[0].first.maxValue, 70);

	// Check B block (entire B array with 1 thread)
	EXPECT_EQ(blocks[0].second.block.size(), 4u);
	EXPECT_EQ(blocks[0].second.block[0].totalWeight, 8);
	EXPECT_EQ(blocks[0].second.block[0].totalValue, 80);
	EXPECT_EQ(blocks[0].second.block[3].totalWeight, 2);
	EXPECT_EQ(blocks[0].second.block[3].totalValue, 20);
	EXPECT_EQ(blocks[0].second.maxValue, 80);

	// Verify Z = A[0].w + B[3].w = 1 + 2 = 3 <= 10  and Y = A[3].w + B[0].w = 7 + 8 = 15 > 10
	// => Z <= capacity && Y > capacity => keep
	EXPECT_LE(blocks[0].first.block.front().totalWeight + blocks[0].second.block.back().totalWeight, 10);
	EXPECT_GT(blocks[0].first.block.back().totalWeight + blocks[0].second.block.front().totalWeight, 10);
}

TEST(Prune, MultiThreadPrunesAndKeepsCorrectly)
{
	std::vector<CopaSubset> A{CopaSubset{{}, 1, 10}, CopaSubset{{}, 2, 20}, CopaSubset{{}, 3, 30},
							  CopaSubset{{}, 4, 40}};

	std::vector<CopaSubset> B{CopaSubset{{}, 5, 50}, CopaSubset{{}, 4, 40}, CopaSubset{{}, 3, 30},
							  CopaSubset{{}, 2, 20}};

	std::vector<std::pair<CopaBlock, CopaBlock>> blocks;
	std::vector<CopaBlock> blocksA(2);
	std::vector<CopaBlock> blocksB(2);
	distribute_block_per_processor(A, blocksA, 2);
	distribute_block_per_processor(B, blocksB, 2);
	prune(blocksA, blocksB, blocks, 6, 2);

	// With 2 threads, blocks are: A0=[(1,10),(2,20)], A1=[(3,30),(4,40)]
	//                        B0=[(5,50),(4,40)], B1=[(3,30),(2,20)]
	// P0 keeps (A0,B0): Z=1+4=5<=6, Y=2+5=7>6
	// P1 keeps (A1,B1): Z=3+2=5<=6, Y=4+3=7>6
	// P0 prunes (A0,B1): Y=2+3=5<=6 => all valid
	// P1 prunes (A1,B0): Z=3+4=7>6 => none valid

	// Check first block pair (A0, B0)
	EXPECT_EQ(blocks[0].first.block.size(), 2u);
	EXPECT_EQ(blocks[0].first.block[0].totalWeight, 1);
	EXPECT_EQ(blocks[0].first.block[0].totalValue, 10);
	EXPECT_EQ(blocks[0].first.block[1].totalWeight, 2);
	EXPECT_EQ(blocks[0].first.block[1].totalValue, 20);
	EXPECT_EQ(blocks[0].first.maxValue, 20);

	EXPECT_EQ(blocks[0].second.block.size(), 2u);
	EXPECT_EQ(blocks[0].second.block[0].totalWeight, 5);
	EXPECT_EQ(blocks[0].second.block[0].totalValue, 50);
	EXPECT_EQ(blocks[0].second.block[1].totalWeight, 4);
	EXPECT_EQ(blocks[0].second.block[1].totalValue, 40);
	EXPECT_EQ(blocks[0].second.maxValue, 50);

	// Check second block pair (A1, B1)
	EXPECT_EQ(blocks[1].first.block.size(), 2u);
	EXPECT_EQ(blocks[1].first.block[0].totalWeight, 3);
	EXPECT_EQ(blocks[1].first.block[0].totalValue, 30);
	EXPECT_EQ(blocks[1].first.block[1].totalWeight, 4);
	EXPECT_EQ(blocks[1].first.block[1].totalValue, 40);
	EXPECT_EQ(blocks[1].first.maxValue, 40);

	EXPECT_EQ(blocks[1].second.block.size(), 2u);
	EXPECT_EQ(blocks[1].second.block[0].totalWeight, 3);
	EXPECT_EQ(blocks[1].second.block[0].totalValue, 30);
	EXPECT_EQ(blocks[1].second.block[1].totalWeight, 2);
	EXPECT_EQ(blocks[1].second.block[1].totalValue, 20);
	EXPECT_EQ(blocks[1].second.maxValue, 30);
}

TEST(KnapsackCopaSequential, EmptyItemsReturnsZero)
{
	const std::vector<int> weights{};
	const std::vector<int> values{};
	constexpr int capacity = 10;
	auto result = knapsackcopasequential(weights, values, capacity);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->totalValue, 0);
	EXPECT_EQ(result->items.empty(), true);
}

TEST(KnapsackCopaSequential, OddNumberOfItemsReturnsNullopt)
{
	const std::vector<int> weights{1, 2, 3};
	const std::vector<int> values{1, 6, 10};
	constexpr int capacity = 7;
	auto result = knapsackcopasequential(weights, values, capacity);
	EXPECT_FALSE(result.has_value());
}

TEST(CrossValidator, DpCopaSequentialCopaParallelAgree)
{
	struct TestCase
	{
		std::vector<int> weights;
		std::vector<int> values;
		int capacity;
	};

	std::vector<TestCase> cases = {
		{{1, 2, 3, 4}, {1, 6, 10, 16}, 7},
		{{2, 3, 4, 5}, {3, 4, 5, 6}, 5},
		{{1, 2, 3, 4}, {1, 6, 10, 16}, 15},
		{{1, 2, 3, 4}, {1, 6, 10, 16}, 1},
		{{5, 10, 15, 22, 25, 30, 35, 40}, {30, 40, 45, 77, 90, 100, 110, 120}, 100},
		{{3, 7, 12, 18, 24, 30}, {5, 10, 15, 20, 25, 30}, 50},
		{{1, 4, 6, 8, 10, 12}, {2, 5, 7, 9, 11, 13}, 20},
		{{2, 5, 7, 9, 11, 13, 15, 18}, {3, 8, 12, 14, 16, 20, 22, 25}, 40},
	};

	for (const auto &[w, v, cap] : cases)
	{
		auto dp_result = knapsackdp(w, v, cap, 1);
		auto copa_seq = knapsackcopasequential(w, v, cap);
		auto copa_par = knapsackcopa(w, v, cap);

		ASSERT_TRUE(copa_seq.has_value()) << "COPA sequential failed for capacity=" << cap;
		ASSERT_TRUE(copa_par.has_value()) << "COPA parallel failed for capacity=" << cap;

		EXPECT_EQ(dp_result.totalValue, copa_seq->totalValue) << "DP/COPA-seq value mismatch for capacity=" << cap;
		EXPECT_EQ(dp_result.totalValue, copa_par->totalValue) << "DP/COPA-par value mismatch for capacity=" << cap;
		EXPECT_EQ(copa_seq->totalValue, copa_par->totalValue) << "COPA-seq/par value mismatch for capacity=" << cap;
		EXPECT_LE(copa_seq->totalWeight, cap) << "COPA-seq solution exceeds capacity=" << cap;
		EXPECT_LE(copa_par->totalWeight, cap) << "COPA-par solution exceeds capacity=" << cap;
	}
}

TEST(TestKnapSackCopa, TestHugeCase)
{


		std::mt19937 rng(42);
		std::vector<int> weights(46);
		std::vector<int> values(46);

		constexpr int capacity = 100;
		for (int i = 0; i < weights.size(); ++i)
		{
			weights[i] = rng() % 100 + 1; // Weights between 1 and 100
			values[i] = rng() % 100 + 1;  // Values between 1 and 100
		}

		auto dp_result = knapsackdp(weights, values, capacity, 32);
		auto copa_par = knapsackcopa(weights, values, capacity);

		ASSERT_TRUE(copa_par.has_value()) << "COPA parallel failed for huge case";

		EXPECT_EQ(dp_result.totalValue, copa_par->totalValue) << "DP/COPA-par value mismatch for huge case";
		EXPECT_LE(copa_par->totalWeight, capacity) << "COPA-par solution exceeds capacity for huge case";
	
}