#pragma once
#include "knapsack.hpp"
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>

// TODO optimize in order to use a vector of bools instead of a vector of ints for the items in the subset, to save memory and improve cache performance
struct CopaSubset
{
	std::vector<bool> items;
	int totalWeight{};
	int totalValue{};

	friend class boost::serialization::access;
	template <class Archive> void serialize(Archive &ar, const unsigned int /*version*/)
	{
		ar & items;
		ar & totalWeight;
		ar & totalValue;
	}

	void addItem(int itemIndex, int weight, int value)
	{
		if(itemIndex >= static_cast<int>(items.size()))
			items.resize(itemIndex + 1, false);
		items[itemIndex] = true;
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
		std::vector<int> indices;
		for (size_t i = 0; i < items.size(); ++i)
		{
			if (items[i])
				indices.push_back(static_cast<int>(i));
		}
		return indices;
	}

	CopaSubset operator<<(const CopaSubset &other) const
	{
		// return the union of the vectors of bools, and sum the weights and values
		CopaSubset result;
		result.items.resize(items.size()+other.items.size(), false);
		result.totalWeight = totalWeight + other.totalWeight;
		result.totalValue = totalValue + other.totalValue;
		for (size_t i = 0; i < items.size(); ++i)
		{
			result.items[i] = items[i];
		}
		for (size_t i = 0; i < other.items.size(); ++i)
		{
			result.items[i] = result.items[i] || other.items[i];
		}
		return result;
	}
};

struct CopaBlock
{
	std::span<const CopaSubset> block;
	int maxValue;
};