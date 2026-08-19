#include "Knapsack/knapsack.hpp"
#include "Knapsack/knapsackdpdag_impl.hpp"
#include <random>
#include <vector>
#include <gtest/gtest.h>

TEST(KnapsackDPDAG, TiledSolverMatchesClassicDP)
{
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;

	const auto classic = knapsackdp(weights, values, capacity, 1);

	for (int item_block : {1, 2, 4, 10})
	{
		for (int cap_block : {1, 3, 7})
		{
			auto g = build_graph(weights, capacity, item_block, cap_block);
			const int best = solve_dag(g, weights, values, capacity, item_block, cap_block);
			EXPECT_EQ(best, classic.totalValue) << "item_block=" << item_block << " cap_block=" << cap_block;
		}
	}
}

TEST(KnapsackDPDAG, PublicInterfaceMatchesClassicOnRandomInstances)
{
	std::mt19937 rng(7);
	for (int trial = 0; trial < 5; ++trial)
	{
		const int n = 1 + static_cast<int>(rng() % 40);
		std::vector<int> weights(n), values(n);
		const int capacity = 20 + static_cast<int>(rng() % 200);
		for (int i = 0; i < n; ++i)
		{
			weights[i] = 1 + static_cast<int>(rng() % 100);
			values[i] = 1 + static_cast<int>(rng() % 10);
		}

		const auto classic = knapsackdp(weights, values, capacity, 1);
		const auto dag = knapsackdpdag(weights, values, capacity, 1);

		EXPECT_EQ(dag.totalValue, classic.totalValue) << "trial=" << trial;
		EXPECT_LE(dag.totalWeight, capacity) << "trial=" << trial;
	}
}
