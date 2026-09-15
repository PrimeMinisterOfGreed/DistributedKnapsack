#include "Knapsackmpi/knapsackmpi.hpp"
#include <algorithm>
#include <boost/mpi/collectives.hpp>
#include <boost/serialization/vector.hpp>
#include <cstddef>
#include <omp.h>
#include <vector>

namespace
{
constexpr int prefix_tag = 100;

int partition_start(int part, int total, int parts)
{
	return static_cast<int>(static_cast<long long>(part) * total / parts);
}
} // namespace

namespace kp::mpi
{
std::optional<KnapsackSolution> knapsackdpmpi(boost::mpi::communicator &comm, const std::vector<int> &weights,
											  const std::vector<int> &values, int capacity)
{
	const int n = static_cast<int>(weights.size());
	const int world = comm.size();
	const int rank = comm.rank();
	const int cols_total = capacity + 1;

	// Contiguous capacity-axis decomposition: rank r owns columns [start, end).
	const int start = partition_start(rank, cols_total, world);
	const int end = partition_start(rank + 1, cols_total, world);
	const int cols = end - start;

	// Each rank only stores its own column slice of every DP row.
	std::vector<int> local_dp(static_cast<std::size_t>(n + 1) * cols, 0);
	// Left halo: columns [0, end) of the previous row. Row 0 is all zeros.
	std::vector<int> prefix(end, 0);
	std::vector<int> row_prefix;
	std::vector<int> left_buffer;
	row_prefix.reserve(end);
	left_buffer.reserve(end);

	for (int i = 1; i <= n; ++i)
	{
		const int weight = weights[i - 1];
		const int value = values[i - 1];
		const int *prev = prefix.data();
		int *cur = local_dp.data() + static_cast<std::size_t>(i) * cols;

#pragma omp parallel for schedule(static)
		for (int c = 0; c < cols; ++c)
		{
			const int w = start + c;
			int best = prev[w];
			if (weight <= w)
			{
				const int include = prev[w - weight] + value;
				if (include > best)
					best = include;
			}
			cur[c] = best;
		}

		// Neighbor halo exchange: this row's prefix [0, end) is the left
		// neighbor's prefix followed by the slice we just computed.
		row_prefix.clear();
		if (rank > 0)
		{
			comm.recv(rank - 1, prefix_tag, left_buffer);
			row_prefix.insert(row_prefix.end(), left_buffer.begin(), left_buffer.end());
		}
		row_prefix.insert(row_prefix.end(), cur, cur + cols);
		if (rank + 1 < world)
		{
			comm.send(rank + 1, prefix_tag, row_prefix);
		}
		prefix.swap(row_prefix);
	}

	// Gather the distributed table on rank 0 for backtracking.
	std::vector<int> counts(world);
	std::vector<int> displs(world);
	{
		int offset = 0;
		for (int r = 0; r < world; ++r)
		{
			const int r_cols = partition_start(r + 1, cols_total, world) - partition_start(r, cols_total, world);
			counts[r] = (n + 1) * r_cols;
			displs[r] = offset;
			offset += counts[r];
		}
	}

	std::vector<int> gathered;
	if (rank == 0)
		gathered.resize(static_cast<std::size_t>(n + 1) * cols_total, 0);

	boost::mpi::gatherv(comm, local_dp.data(), static_cast<int>(local_dp.size()), gathered.data(), counts, displs, 0);

	if (rank != 0)
		return std::nullopt;

	// Reassemble the full (n + 1) x (capacity + 1) table from the per-rank slices.
	std::vector<int> dp(static_cast<std::size_t>(n + 1) * cols_total, 0);
	for (int r = 0; r < world; ++r)
	{
		const int r_start = partition_start(r, cols_total, world);
		const int r_cols = partition_start(r + 1, cols_total, world) - r_start;
		const int *block = gathered.data() + displs[r];
		for (int i = 0; i <= n; ++i)
		{
			std::copy_n(block + static_cast<std::size_t>(i) * r_cols, r_cols,
						dp.begin() + static_cast<std::size_t>(i) * cols_total + r_start);
		}
	}

	// Backtrack exactly like the sequential implementation.
	auto at = [&](int i, int w) { return dp[static_cast<std::size_t>(i) * cols_total + w]; };
	std::vector<int> includedItems;
	int totalValue = at(n, capacity);
	const int maxValue = totalValue;
	int totalWeight = 0;
	int w = capacity;

	for (int i = n; i > 0 && totalValue > 0; --i)
	{
		if (totalValue != at(i - 1, w))
		{
			includedItems.push_back(i - 1);
			totalValue -= values[i - 1];
			w -= weights[i - 1];
			totalWeight += weights[i - 1];
		}
	}

	return KnapsackSolution{includedItems, maxValue, totalWeight};
}
} // namespace kp::mpi
