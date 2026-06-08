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


