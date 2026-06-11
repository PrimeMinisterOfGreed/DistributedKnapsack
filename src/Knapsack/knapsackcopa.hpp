#pragma once
#include "knapsack.hpp"

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

struct CopaBlock
{
	std::span<const CopaSubset> block;
	int maxValue;
};