#include "knapsackdp.hpp"
#include <algorithm>
#include <cstddef>
#include <thrust/device_vector.h>
#include <thrust/execution_policy.h>
#include <thrust/for_each.h>
#include <thrust/host_vector.h>
#include <thrust/iterator/counting_iterator.h>

namespace
{
struct KnapsackStep
{
	const int *prev;
	int *cur;
	int weight;
	int value;

	__host__ __device__ void operator()(int w) const
	{
		int exclude = prev[w];
		int include = (weight <= w) ? prev[w - weight] + value : exclude;
		cur[w] = include > exclude ? include : exclude;
	}
};
} // namespace

namespace kp::gpu
{
KnapsackSolution knapsackdp(const std::vector<int> &weights, const std::vector<int> &values, int capacity)
{
	const int n = static_cast<int>(weights.size());
	const int cols = capacity + 1;

	thrust::device_vector<int> d_dp(static_cast<std::size_t>(n + 1) * cols, 0);
	int *dp = thrust::raw_pointer_cast(d_dp.data());

	for (int i = 1; i <= n; ++i)
	{
		KnapsackStep step{dp + static_cast<std::size_t>(i - 1) * cols, dp + static_cast<std::size_t>(i) * cols,
						  weights[i - 1], values[i - 1]};
		thrust::for_each(thrust::device, thrust::counting_iterator<int>(0), thrust::counting_iterator<int>(cols), step);
	}

	thrust::host_vector<int> dp_table = d_dp;

	std::vector<int> includedItems;
	int totalValue = dp_table[static_cast<std::size_t>(n) * cols + capacity];
	const int maxValue = totalValue;
	int totalWeight = 0;
	int w = capacity;

	for (int i = n; i > 0 && totalValue > 0; --i)
	{
		if (totalValue != dp_table[static_cast<std::size_t>(i - 1) * cols + w])
		{
			includedItems.push_back(i - 1);
			totalValue -= values[i - 1];
			w -= weights[i - 1];
			totalWeight += weights[i - 1];
		}
	}

	return {includedItems, maxValue, totalWeight};
}
} // namespace kp::gpu
