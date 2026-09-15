#pragma once
#include "Knapsack/knapsack.hpp"
#include <boost/mpi.hpp>
#include <optional>

namespace kp::mpi
{
std::optional<KnapsackSolution> knapsackdpmpi(boost::mpi::communicator &comm, const std::vector<int> &weights,
											  const std::vector<int> &values, int capacity);

std::optional<KnapsackSolution> knapsackcopampi(boost::mpi::communicator &comm, const std::vector<int> &weights,
												const std::vector<int> &values, int capacity);
} // namespace kp::mpi