#include "knapsackdp.hpp"
#include <algorithm>
#include <gtest/gtest.h>
#include <numeric>
#include <random>
#include <vector>

using kp::gpu::knapsackdp;
using kp::gpu::KnapsackSolution;

namespace
{
KnapsackSolution referenceKnapsack(const std::vector<int> &weights, const std::vector<int> &values, int capacity)
{
	const int n = static_cast<int>(weights.size());
	const int cols = capacity + 1;
	std::vector<int> dp(static_cast<std::size_t>(n + 1) * cols, 0);
	auto at = [&](int i, int w) -> int & { return dp[static_cast<std::size_t>(i) * cols + w]; };

	for (int i = 1; i <= n; ++i)
	{
		for (int w = 0; w <= capacity; ++w)
		{
			if (weights[i - 1] <= w)
				at(i, w) = std::max(at(i - 1, w), at(i - 1, w - weights[i - 1]) + values[i - 1]);
			else
				at(i, w) = at(i - 1, w);
		}
	}

	std::vector<int> items;
	int totalValue = at(n, capacity);
	const int maxValue = totalValue;
	int totalWeight = 0;
	int w = capacity;

	for (int i = n; i > 0 && totalValue > 0; --i)
	{
		if (totalValue != at(i - 1, w))
		{
			items.push_back(i - 1);
			totalValue -= values[i - 1];
			w -= weights[i - 1];
			totalWeight += weights[i - 1];
		}
	}

	return {items, maxValue, totalWeight};
}

void expectConsistent(const KnapsackSolution &solution, const std::vector<int> &weights,
					  const std::vector<int> &values, int capacity)
{
	int summedWeight = 0;
	int summedValue = 0;
	for (int idx : solution.items)
	{
		ASSERT_GE(idx, 0);
		ASSERT_LT(idx, static_cast<int>(weights.size()));
		summedWeight += weights[idx];
		summedValue += values[idx];
	}
	EXPECT_EQ(summedWeight, solution.totalWeight);
	EXPECT_EQ(summedValue, solution.totalValue);
	EXPECT_LE(solution.totalWeight, capacity);
}

std::vector<int> sortedItems(const std::vector<int> &items)
{
	std::vector<int> copy = items;
	std::sort(copy.begin(), copy.end());
	return copy;
}
} // namespace

TEST(KnapsackDPGpu, BasicExample)
{
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;

	const auto result = knapsackdp(weights, values, capacity);
	EXPECT_EQ(result.totalValue, 26);
	expectConsistent(result, weights, values, capacity);
}

TEST(KnapsackDPGpu, EmptyItems)
{
	const std::vector<int> weights{};
	const std::vector<int> values{};

	const auto result = knapsackdp(weights, values, 10);
	EXPECT_EQ(result.totalValue, 0);
	EXPECT_EQ(result.totalWeight, 0);
	EXPECT_TRUE(result.items.empty());
}

TEST(KnapsackDPGpu, NoItemFits)
{
	const std::vector<int> weights{5, 6};
	const std::vector<int> values{10, 20};

	const auto result = knapsackdp(weights, values, 4);
	EXPECT_EQ(result.totalValue, 0);
	EXPECT_EQ(result.totalWeight, 0);
	EXPECT_TRUE(result.items.empty());
}

TEST(KnapsackDPGpu, SingleItemFitsExactly)
{
	const std::vector<int> weights{4};
	const std::vector<int> values{9};

	const auto result = knapsackdp(weights, values, 4);
	EXPECT_EQ(result.totalValue, 9);
	EXPECT_EQ(result.totalWeight, 4);
	ASSERT_EQ(result.items.size(), 1u);
	EXPECT_EQ(result.items[0], 0);
	expectConsistent(result, weights, values, 4);
}

TEST(KnapsackDPGpu, MatchesReference)
{
	std::mt19937 rng(1234);
	for (int trial = 0; trial < 200; ++trial)
	{
		const int n = 1 + static_cast<int>(rng() % 40);
		const int capacity = 1 + static_cast<int>(rng() % 300);
		std::vector<int> weights(n);
		std::vector<int> values(n);
		for (int i = 0; i < n; ++i)
		{
			weights[i] = 1 + static_cast<int>(rng() % 50);
			values[i] = 1 + static_cast<int>(rng() % 100);
		}

		const auto gpu = knapsackdp(weights, values, capacity);
		const auto ref = referenceKnapsack(weights, values, capacity);

		EXPECT_EQ(gpu.totalValue, ref.totalValue) << "trial " << trial;
		EXPECT_EQ(gpu.totalWeight, ref.totalWeight) << "trial " << trial;
		EXPECT_EQ(sortedItems(gpu.items), sortedItems(ref.items)) << "trial " << trial;
		expectConsistent(gpu, weights, values, capacity);
	}
}
