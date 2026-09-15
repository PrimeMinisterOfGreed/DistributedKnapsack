#include "knapsack.hpp"
#include "time.hpp"
#include <Eigen/Eigen>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <omp.h>
#include <optional>
#include <print>
KnapsackSolution knapsackdp(const std::vector<int> &weights, const std::vector<int> &values, int capacity)
{
	int n = weights.size();
	Eigen::MatrixX<int> dp{};
	dp.resize(n + 1, capacity + 1);
	dp.setZero();
	// Build the dp table
	//
	START_BLOCK("KnapsackDP::Compute");
#pragma omp parallel
	for (int i = 1; i <= n; ++i)
	{
		int tid = omp_get_thread_num();
		int nt = omp_get_num_threads();
		int begin = std::ceil(tid * capacity / nt);
		int end = std::min<int>(capacity, std::ceil((tid + 1) * capacity / nt));
		START_BLOCK("KnapsackDP::ComputeItem");
		if (tid == 0)
		{
			START_BLOCK("KnapsackDP::ComputeThreadBlock");
		}
		for (int w = begin; w <= end; ++w)
		{
			if (weights[i - 1] <= w)
			{
				dp(i, w) = std::max(dp(i - 1, w), dp(i - 1, w - weights[i - 1]) + values[i - 1]);
			}
			else
			{
				dp(i, w) = dp(i - 1, w);
			}
		}
		if (tid == 0)
		{
			END_BLOCK("KnapsackDP::ComputeThreadBlock");
		}
		END_BLOCK("KnapsackDP::ComputeItem");
#pragma omp barrier
	}
	END_BLOCK("KnapsackDP::Compute");
	// Backtrack to find the items included in the knapsack
	std::vector<int> includedItems;
	int totalValue = dp(n, capacity);
	int totalWeight = 0;
	int w = capacity;

	for (int i = n; i > 0 && totalValue > 0; --i)
	{
		if (totalValue != dp(i - 1, w))
		{
			includedItems.push_back(i - 1); // Store the index of the included item
			totalValue -= values[i - 1];
			w -= weights[i - 1];
			totalWeight += weights[i - 1];
		}
	}

	return {includedItems, dp(n, capacity), totalWeight};
}
