#include "Knapsack/knapsack.hpp"
#include "Knapsack/knapsackcopa.hpp"
#include "TestEnvironment.hpp"
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <vector>
#include "Knapsack/utils.tpp"


TEST(KnapsackDP, OneThreadFindsOptimalValue)
{
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;
	auto result = knapsackdp(weights, values, capacity, 1);
	fmt::println("Included items: {}", fmt::join(result.items, ", "));
	EXPECT_EQ(result.totalValue, 26);
}

TEST(KnapsackDP, MultipleThreadsFindOptimalValue)
{
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;
	auto result = knapsackdp(weights, values, capacity, 4);
	fmt::println("Included items: {}", fmt::join(result.items, ", "));
	EXPECT_EQ(result.totalValue, 26);
}

TEST(GenerateCopaSubsets, ProducesAllSubsetsSortedByWeight)
{
	std::vector<std::pair<int, int>> items{{1, 1}, {2, 2}};
	auto subsets = generate_copa_subsets(items,3);

	EXPECT_EQ(subsets.size(), 4u);

	std::vector<int> weights;
	std::vector<int> values;
	for (auto &s : subsets)
	{
		weights.push_back(s.totalWeight);
		values.push_back(s.totalValue);
	}

	std::vector<int> expected_weights{0, 1, 2, 3};
	std::vector<int> expected_values{0, 1, 2, 3};

	EXPECT_EQ(weights, expected_weights);
	EXPECT_EQ(values, expected_values);

	std::vector<std::vector<int>> expected_items{{}, {1}, {2}, {1, 2}};
	EXPECT_EQ(subsets[0].items, expected_items[0]);
	EXPECT_EQ(subsets[1].items, expected_items[1]);
	EXPECT_EQ(subsets[2].items, expected_items[2]);
	EXPECT_EQ(subsets[3].items, expected_items[3]);
}

TEST(Prune, SingleThreadKeepsValidBlockPairs)
{
	std::vector<CopaSubset> A{
		CopaSubset{{}, 1, 10},
		CopaSubset{{}, 3, 30},
		CopaSubset{{}, 5, 50},
		CopaSubset{{}, 7, 70}
	};

	std::vector<CopaSubset> B{
		CopaSubset{{}, 8, 80},
		CopaSubset{{}, 6, 60},
		CopaSubset{{}, 4, 40},
		CopaSubset{{}, 2, 20}
	};

	std::vector<CopaBlock> Aout;
	std::vector<CopaBlock> Bout;
	prune(A, B, Aout, Bout, 10, 1);

	ASSERT_EQ(Aout.size(), Bout.size());
	EXPECT_EQ(Aout.size(), 1u);
	EXPECT_EQ(Aout[0].block.size(), 4u);
	EXPECT_EQ(Bout[0].block.size(), 4u);
}

TEST(Prune, MultiThreadPrunesAndKeepsCorrectly)
{
	std::vector<CopaSubset> A{
		CopaSubset{{}, 1, 10},
		CopaSubset{{}, 2, 20},
		CopaSubset{{}, 3, 30},
		CopaSubset{{}, 4, 40}
	};

	std::vector<CopaSubset> B{
		CopaSubset{{}, 5, 50},
		CopaSubset{{}, 4, 40},
		CopaSubset{{}, 3, 30},
		CopaSubset{{}, 2, 20}
	};

	std::vector<CopaBlock> Aout;
	std::vector<CopaBlock> Bout;
	prune(A, B, Aout, Bout, 6, 2);

	ASSERT_EQ(Aout.size(), Bout.size());
	EXPECT_EQ(Aout.size(), 2u);
}

TEST(ParallelSaveMaxValue, SingleThreadComputesSuffixMax)
{
	std::vector<CopaSubset> input{
		CopaSubset{{1}, 1, 10},
		CopaSubset{{2}, 2, 5},
		CopaSubset{{3}, 3, 20},
		CopaSubset{{4}, 4, 15}
	};
	std::vector<CopaSubset> output;
	parallel_save_max_value(input, output, 1);

	ASSERT_EQ(output.size(), 4u);
	EXPECT_EQ(output[0].totalValue, 20);
	EXPECT_EQ(output[1].totalValue, 20);
	EXPECT_EQ(output[2].totalValue, 20);
	EXPECT_EQ(output[3].totalValue, 15);
}

TEST(ParallelSaveMaxValue, MultiThreadComputesSuffixMax)
{
	std::vector<CopaSubset> input{
		CopaSubset{{1}, 1, 10},
		CopaSubset{{2}, 2, 5},
		CopaSubset{{3}, 3, 20},
		CopaSubset{{4}, 4, 15},
		CopaSubset{{5}, 5, 25},
		CopaSubset{{6}, 6, 8}
	};
	std::vector<CopaSubset> output;
	parallel_save_max_value(input, output, 3);

	ASSERT_EQ(output.size(), 6u);
	EXPECT_EQ(output[0].totalValue, 25);
	EXPECT_EQ(output[1].totalValue, 25);
	EXPECT_EQ(output[2].totalValue, 25);
	EXPECT_EQ(output[3].totalValue, 25);
	EXPECT_EQ(output[4].totalValue, 25);
	EXPECT_EQ(output[5].totalValue, 8);
}

TEST(KnapsackCopaSequential, OneThreadFindsOptimalValue)
{
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;
	auto result = knapsackcopasequential(weights, values, capacity);
	ASSERT_TRUE(result.has_value());
	fmt::println("Included items: {}", fmt::join(result->items, ", "));
	EXPECT_EQ(result->totalValue, 26);
}

TEST(KnapsackCopaSequential, SmallCapacityFindsOptimalValue)
{
	const std::vector<int> weights{2, 3, 4, 5};
	const std::vector<int> values{3, 4, 5, 6};
	constexpr int capacity = 5;
	auto result = knapsackcopasequential(weights, values, capacity);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->totalValue, 7);
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

TEST(KnapsackCopaSequential, AllItemsFit)
{
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 15;
	auto result = knapsackcopasequential(weights, values, capacity);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->totalValue, 33);
	EXPECT_EQ(result->totalWeight, 10);
}

TEST(KnapsackCopaSequential, SingleItemFits)
{
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 1;
	auto result = knapsackcopasequential(weights, values, capacity);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->totalValue, 1);
	EXPECT_EQ(result->totalWeight, 1);
}


