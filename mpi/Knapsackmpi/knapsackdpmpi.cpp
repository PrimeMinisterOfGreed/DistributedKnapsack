#include "Knapsackmpi/knapsackmpi.hpp"
#include "time.hpp"
#include <algorithm>
#include <boost/mpi/collectives.hpp>
#include <boost/serialization/vector.hpp>
#include <cmath>
#include <cstddef>
#include <omp.h>
#include <vector>

namespace
{
constexpr int prefix_tag = 100;

struct Partition
{
	int start;
	int end;
	int cols;
};

struct GatheredTable
{
	std::vector<int> data;
	std::vector<int> displs;
};

constexpr int partition_start(int part, int total, int parts)
{
	return static_cast<int>(static_cast<long long>(part) * total / parts);
}

Partition partition_of(int rank, int total, int parts)
{
	const int start = partition_start(rank, total, parts);
	const int end = partition_start(rank + 1, total, parts);
	return {start, end, end - start};
}

// Step 1: fill this rank's column slice of every DP row, exchanging the
// left-neighbor prefix each iteration so the recurrence has its halo.
std::vector<int> compute_local_table(boost::mpi::communicator &comm, const std::vector<int> &weights,
									 const std::vector<int> &values, int n, const Partition &part)
{
	const int rank = comm.rank();
	const int world = comm.size();

	// Each rank only stores its own column slice of every DP row.
	std::vector<int> local_dp(static_cast<std::size_t>(n + 1) * part.cols, 0);
	// Left halo: columns [0, end) of the previous row. Row 0 is all zeros.
	std::vector<int> prefix(part.end, 0);
	std::vector<int> row_prefix;
	std::vector<int> left_buffer;
	row_prefix.reserve(part.end);
	left_buffer.reserve(part.end);

	START_BLOCK("KnapsackDPMPI::Compute");
#pragma omp parallel
	for (int i = 1; i <= n; ++i)
	{
		const int tid = omp_get_thread_num();
		const int nt = omp_get_num_threads();
		const int begin = static_cast<int>(std::ceil(static_cast<double>(tid) * part.cols / nt));
		const int end =
			std::min<int>(part.cols, static_cast<int>(std::ceil(static_cast<double>(tid + 1) * part.cols / nt)));
		const int weight = weights[i - 1];
		const int value = values[i - 1];
		const int *prev = prefix.data();
		int *cur = local_dp.data() + static_cast<std::size_t>(i) * part.cols;

		START_BLOCK("KnapsackDPMPI::ComputeItem");
		if (tid == 0)
		{
			START_BLOCK("KnapsackDPMPI::ComputeThreadBlock");
		}
		for (int c = begin; c < end; ++c)
		{
			const int w = part.start + c;
			int best = prev[w];
			if (weight <= w)
			{
				const int include = prev[w - weight] + value;
				if (include > best)
					best = include;
			}
			cur[c] = best;
		}
		if (tid == 0)
		{
			END_BLOCK("KnapsackDPMPI::ComputeThreadBlock");
		}
		END_BLOCK("KnapsackDPMPI::ComputeItem");
#pragma omp barrier

		// Neighbor halo exchange: this row's prefix [0, end) is the left
		// neighbor's prefix followed by the slice we just computed.
#pragma omp single
		{
			row_prefix.clear();
			if (rank > 0)
			{
				comm.recv(rank - 1, prefix_tag, left_buffer);
				row_prefix.insert(row_prefix.end(), left_buffer.begin(), left_buffer.end());
			}
			row_prefix.insert(row_prefix.end(), cur, cur + part.cols);
			if (rank + 1 < world)
			{
				comm.send(rank + 1, prefix_tag, row_prefix);
			}
			prefix.swap(row_prefix);
		}
	}
	END_BLOCK("KnapsackDPMPI::Compute");

	return local_dp;
}

// Step 2: collect every rank's slice on rank 0, recording where each block
// starts in the gathered buffer.
GatheredTable gather_table(boost::mpi::communicator &comm, const std::vector<int> &local_dp, int n, int cols_total,
						   int world)
{
	GatheredTable table;
	table.displs.resize(world);
	std::vector<int> counts(world);

	int offset = 0;
	for (int r = 0; r < world; ++r)
	{
		const int r_cols = partition_of(r, cols_total, world).cols;
		counts[r] = (n + 1) * r_cols;
		table.displs[r] = offset;
		offset += counts[r];
	}

	if (comm.rank() == 0)
		table.data.resize(static_cast<std::size_t>(n + 1) * cols_total, 0);

	boost::mpi::gatherv(comm, local_dp.data(), static_cast<int>(local_dp.size()), table.data.data(), counts,
						table.displs, 0);
	return table;
}

// Step 3: rebuild the full (n + 1) x (capacity + 1) table from the per-rank slices.
std::vector<int> reassemble_table(const GatheredTable &gathered, int n, int cols_total, int world)
{
	std::vector<int> dp(static_cast<std::size_t>(n + 1) * cols_total, 0);
	for (int r = 0; r < world; ++r)
	{
		const int r_start = partition_start(r, cols_total, world);
		const int r_cols = partition_start(r + 1, cols_total, world) - r_start;
		const int *block = gathered.data.data() + gathered.displs[r];
		for (int i = 0; i <= n; ++i)
		{
			std::copy_n(block + static_cast<std::size_t>(i) * r_cols, r_cols,
						dp.begin() + static_cast<std::size_t>(i) * cols_total + r_start);
		}
	}
	return dp;
}

// Step 4: walk the table backwards to recover the chosen items.
KnapsackSolution backtrack(const std::vector<int> &dp, const std::vector<int> &weights,
						   const std::vector<int> &values, int capacity, int n)
{
	const int cols_total = capacity + 1;
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
	const Partition part = partition_of(rank, cols_total, world);

	const std::vector<int> local_dp = compute_local_table(comm, weights, values, n, part);

	const GatheredTable gathered = gather_table(comm, local_dp, n, cols_total, world);
	if (rank != 0)
		return std::nullopt;

	const std::vector<int> dp = reassemble_table(gathered, n, cols_total, world);
	return backtrack(dp, weights, values, capacity, n);
}
} // namespace kp::mpi
