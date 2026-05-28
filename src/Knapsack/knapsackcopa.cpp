#include "knapsackcopa.hpp"
#include <algorithm>

struct CopaSubset
{
	std::vector<int> items;
	int totalWeight{};
	int totalValue{};
};

std::vector<CopaSubset> generate_copa_subsets(const std::vector<int> &weights, const std::vector<int> &values,
											  std::size_t begin, std::size_t end)
{
	std::vector<CopaSubset> subsets{CopaSubset{}};
	if (begin > end || end > weights.size() || end > values.size())
	{
		return subsets;
	}

	for (std::size_t index = begin; index < end; ++index)
	{
		const auto currentSize = subsets.size();
		subsets.reserve(currentSize * 2);
		for (std::size_t subsetIndex = 0; subsetIndex < currentSize; ++subsetIndex)
		{
			auto nextSubset = subsets[subsetIndex];
			nextSubset.items.push_back(static_cast<int>(index));
			nextSubset.totalWeight += weights[index];
			nextSubset.totalValue += values[index];
			subsets.push_back(std::move(nextSubset));
		}
	}

	std::sort(subsets.begin(), subsets.end(), [](const CopaSubset &left, const CopaSubset &right) {
		if (left.totalWeight != right.totalWeight)
		{
			return left.totalWeight < right.totalWeight;
		}
		return left.totalValue > right.totalValue;
	});

	return subsets;
}

std::optional<KnapsackSolution> knapsackcopa(const std::vector<int> &weights, const std::vector<int> &values,
											 int capacity, int numThreads)
{
	(void)weights;
	(void)values;
	(void)capacity;
	(void)numThreads;
	return std::nullopt;
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