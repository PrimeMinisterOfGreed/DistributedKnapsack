#include "knapsackcopa.hpp"
#include "utils.tpp"
#include <algorithm>
#include <ranges>

std::optional<KnapsackSolution> knapsackcopasequential(const std::vector<int> &weights, const std::vector<int> &values,
													   int capacity)
{
	using namespace std::views;
	auto list = zip(weights, values);
	auto Alist = take(list, list.size() / 2);
	auto Blist = drop(list, list.size() / 2);
	auto A = generate_copa_subsets(Alist);
	auto B = generate_copa_subsets(Blist);
	auto N = A.size();
	auto L = N;
	auto maxB = B[N].totalValue;
	auto bestValue = 0;
	std::pair<int,int> X1{0,0};

	for(int i = N - 1; i > 0; i--)
	{
		if(B[i].totalValue > maxB)
		{
			maxB = B[i].totalValue;
			L=i;
		}
	}
	for(int i = 1, j = 1; i < L && j < N;)
	{
		
	}
}

std::optional<KnapsackSolution> knapsackcopa(const std::vector<int> &weights, const std::vector<int> &values,
											 int capacity, int numThreads)
{
	auto list = std::views::zip(weights, values);
	auto A = std::views::take(list, list.size() / 2);
	auto B = std::views::drop(list, list.size() / 2);
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