#pragma once
#include "Knapsack/utils.tpp"
#include <boost/mpi.hpp>


using communicator = boost::mpi::communicator;
void mpi_parallel_merge(communicator &comm, const std::ranges::input_range auto &A,
										  const std::ranges::input_range auto &B,
										  std::ranges::output_range<decltype(*std::ranges::begin(A))> auto &output)
{
	if (comm.rank() == 0)
	{
	}
	else
	{
	}
}

std::vector<CopaSubset> mpi_generate_copa_subsets(communicator &comm, const std::ranges::input_range auto &items)
{
	std::vector<CopaSubset> subsets{CopaSubset{}};
	for (const auto &item : items)
	{
		auto shifted = subsets;
		auto [w, v] = item;
		boost::mpi::broadcast(comm, shifted, 0);
	}
	return subsets;
}