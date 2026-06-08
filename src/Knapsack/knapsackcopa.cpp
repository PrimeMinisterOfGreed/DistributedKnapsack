#include "knapsackcopa.hpp"
#include <algorithm>
#include <ranges>
#include "utils.tpp"





std::optional<KnapsackSolution> knapsackcopa(const std::vector<int> &weights, const std::vector<int> &values,
											 int capacity, int numThreads)
{
	
}

// Region MPI version

KnapsackSolution mainnode(boost::mpi::communicator &comm, const std::vector<int> &weights,
						  const std::vector<int> &values, int capacity)
{
	(void)comm;
	(void)weights;
	(void)values;
	(void)capacity;
	return {};
}

KnapsackSolution workernode(boost::mpi::communicator &comm, const std::vector<int> &weights,
							const std::vector<int> &values, int capacity)
{
	(void)comm;
	(void)weights;
	(void)values;
	(void)capacity;
	return {};
}

std::optional<KnapsackSolution> knapsackcopampi(boost::mpi::communicator &comm, const std::vector<int> &weights,
												const std::vector<int> &values, int capacity)
{
	(void)comm;
	(void)weights;
	(void)values;
	(void)capacity;
	return std::nullopt;
}