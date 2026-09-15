#pragma once
#include <vector>
namespace kp::gpu
{
struct KnapsackSolution
{
	std::vector<int> items; // Indices of items included in the knapsack
	int totalValue;			// Total value of the included items
	int totalWeight;		// Total weight of the included items
};

KnapsackSolution knapsackdp(const std::vector<int> &weights, const std::vector<int> &values, int capacity);
} // namespace kp::gpu
