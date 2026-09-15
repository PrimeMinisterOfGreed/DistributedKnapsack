#pragma once
#include <boost/graph/adjacency_list.hpp>
#include <optional>
#include <vector>
struct KnapsackSolution
{
	std::vector<int> items; // Indices of items included in the knapsack
	int totalValue;			// Total value of the included items
	int totalWeight;		// Total weight of the included items
};

KnapsackSolution knapsackdp(const std::vector<int> &weights, const std::vector<int> &values, int capacity);
std::optional<KnapsackSolution> knapsackcopasequential(const std::vector<int> &weights, const std::vector<int> &values,
													   int capacity);

std::optional<KnapsackSolution> knapsackcopa(const std::vector<int> &weights, const std::vector<int> &values,
											 int capacity);

KnapsackSolution knapsackdpdag(const std::vector<int> &weights, const std::vector<int> &values, int capacity,
							   int item_block = 10, int cap_block = 0);
