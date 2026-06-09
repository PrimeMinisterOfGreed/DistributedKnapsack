#pragma once
#include "knapsack.hpp"

std::optional<KnapsackSolution> knapsackcopasequential(const std::vector<int> &weights, const std::vector<int> &values,
											 int capacity);

std::optional<KnapsackSolution> knapsackcopa(const std::vector<int> &weights, const std::vector<int> &values,
											 int capacity, int numThreads = 1);
std::optional<KnapsackSolution> knapsackcopampi(boost::mpi::communicator &comm, const std::vector<int> &weights,
												const std::vector<int> &values, int capacity);

struct CopaSubset
{
	std::vector<int> items;
	int totalWeight{};
	int totalValue{};

	bool operator>(const CopaSubset &other) const
	{
		return totalWeight > other.totalWeight;
	}

	bool operator>=(const CopaSubset &other) const
	{
		return totalWeight >= other.totalWeight;
	}

	bool operator<(const CopaSubset &other) const
	{
		return totalWeight < other.totalWeight;
	}
};


