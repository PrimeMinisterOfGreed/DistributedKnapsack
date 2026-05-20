#pragma once
#include <vector>
#include <boost/mpi.hpp>

struct KnapsackSolution
{
	std::vector<int> items; // Indices of items included in the knapsack
	int totalValue;			// Total value of the included items
	int totalWeight;		// Total weight of the included items
};

KnapsackSolution knapsackdp(const std::vector<int>& weights, const std::vector<int>& values, int capacity, int numThreads = 1);




std::optional<KnapsackSolution> knapsackdpmpi(boost::mpi::communicator& comm, const std::vector<int>& weights, const std::vector<int>& values, int capacity);
