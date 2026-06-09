#include "Knapsack/knapsack.hpp"
#include "Knapsack/knapsackcopa.hpp"
#include "TestEnvironment.hpp"
#include <fmt/format.h>
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

	std::vector<std::pair<std::span<const CopaSubset>, std::span<const CopaSubset>>> result;
	prune(A, B, result, 10, 1);

	EXPECT_EQ(result.size(), 1u);
	EXPECT_EQ(result[0].first.size(), 4u);
	EXPECT_EQ(result[0].second.size(), 4u);
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

	std::vector<std::pair<std::span<const CopaSubset>, std::span<const CopaSubset>>> result;
	prune(A, B, result, 6, 2);

	EXPECT_EQ(result.size(), 2u);
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


