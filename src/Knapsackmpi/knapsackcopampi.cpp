#include "Knapsack/knapsackcopa.hpp"
#include "knapsackmpi.hpp"



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