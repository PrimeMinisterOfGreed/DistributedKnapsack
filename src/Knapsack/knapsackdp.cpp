#include "knapsack.hpp"
#include "time.hpp"
#include <omp.h>

KnapsackSolution knapsackdp(const std::vector<int> &weights, const std::vector<int> &values, int capacity,
							int cap_block)
{
	int n = weights.size();
	std::vector<std::vector<int>> dp(n + 1, std::vector<int>(capacity + 1, 0));

	// Build the dp table
#pragma omp parallel
	for (int i = 1; i <= n; ++i)
	{
		START_BLOCK("KnapsackDP::Compute");
		{
			int tid = omp_get_thread_num();
			int nt = omp_get_num_threads();
			int begin = tid * capacity / nt;
			int end = (tid + 1) * capacity / nt;
			if (tid == 0)
			{
				START_BLOCK("KnapsackDP::ComputeThreadBlock");
			}
			for (int w = begin; w <= end; ++w)
			{
				if (weights[i - 1] <= w)
				{
					dp[i][w] = std::max(dp[i - 1][w], dp[i - 1][w - weights[i - 1]] + values[i - 1]);
				}
				else
				{
					dp[i][w] = dp[i - 1][w];
				}
			}
			if (tid == 0)
			{
				END_BLOCK("KnapsackDP::ComputeThreadBlock");
			}
		}
		END_BLOCK("KnapsackDP::Compute");
#pragma omp barrier
	}

	// Backtrack to find the items included in the knapsack
	std::vector<int> includedItems;
	int totalValue = dp[n][capacity];
	int totalWeight = 0;
	int w = capacity;

	for (int i = n; i > 0 && totalValue > 0; --i)
	{
		if (totalValue != dp[i - 1][w])
		{
			includedItems.push_back(i - 1); // Store the index of the included item
			totalValue -= values[i - 1];
			w -= weights[i - 1];
			totalWeight += weights[i - 1];
		}
	}

	return {includedItems, dp[n][capacity], totalWeight};
}
