#pragma once
#include "knapsack.hpp"
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <span>
struct CopaSubset
{
	int totalWeight{};
	int totalValue{};
	int index{};

	friend class boost::serialization::access;
	template <class Archive> void serialize(Archive &ar, const unsigned int /*version*/)
	{
		ar & totalWeight;
		ar & totalValue;
		ar & index;
	}

	void addItem(int itemIndex, int weight, int value)
	{
		totalWeight += weight;
		totalValue += value;
	}
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

	std::vector<int> getItemIndices() const
	{

		return {};
	}

	CopaSubset operator<<(const CopaSubset &other) const
	{

		return {};
	}
};

struct CopaBlock
{
	std::span<const CopaSubset> block;
	int maxValue;
};
