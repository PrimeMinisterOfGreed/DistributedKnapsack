#include "knapsack.hpp"
#include "time.hpp"
#include <Eigen/Eigen>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <omp.h>
#include <optional>
#include "phases.hpp"
#include <print>

namespace
{
/**
 * DP table stored ROW-major.
 *
 * Eigen's default storage order is column-major: with the inner loop running
 * over the capacity w, `dp(i, w)` and `dp(i, w + 1)` were (n+1)*4 bytes apart
 * (204 B at n = 50, 20 KB at n = 5000), so every cell touched a new cache line
 * and, for large n, a new page. Row-major makes every row a contiguous stream.
 */
using DpTable = Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

/// Half-open, disjoint slice [begin, end) of the capacity axis for thread tid:
/// 64-bit math (tid * capacity overflows int for large tables) and no shared
/// boundary cell between neighbours.
std::pair<std::int64_t, std::int64_t> capacity_slice(std::int64_t cols, int tid, int nt)
{
	const std::int64_t base = cols / nt;
	const std::int64_t rem = cols % nt;
	const std::int64_t begin = static_cast<std::int64_t>(tid) * base + std::min<std::int64_t>(tid, rem);
	const std::int64_t len = base + (tid < rem ? 1 : 0);
	return {begin, begin + len};
}
} // namespace

KnapsackSolution knapsackdp(const std::vector<int> &weights, const std::vector<int> &values, int capacity)
{
	PhaseTimes::instance().clear();
	int n = weights.size();
	const std::int64_t cols = static_cast<std::int64_t>(capacity) + 1;

	DpTable dp{};
	{
		// No serial setZero(): the table is first-touched in parallel below.
		ScopedPhase phase("alloc");
		dp.resize(n + 1, cols);
	}
	{
		// Parallel first touch with the compute partition: every page is placed
		// on the NUMA node of the thread that will compute it, and the page-fault
		// cost is measured here instead of inflating the kernel time.
		ScopedPhase phase("first_touch");
#pragma omp parallel
		{
			const auto [begin, end] = capacity_slice(cols, omp_get_thread_num(), omp_get_num_threads());
			for (int i = 0; i <= n; ++i)
			{
				int *row = dp.row(i).data();
				for (std::int64_t w = begin; w < end; ++w)
					row[w] = 0;
			}
		}
	}

	// Build the dp table
	//
	START_BLOCK("KnapsackDP::Compute");
	{
		ScopedPhase phase("compute");
#pragma omp parallel
		{
			const int tid = omp_get_thread_num();
			const int nt = omp_get_num_threads();
			// The same slice for every row, so a thread always touches the same
			// columns (first-touch placement stays valid for the whole run).
			const auto [begin, end] = capacity_slice(cols, tid, nt);

			for (int i = 1; i <= n; ++i)
			{
				START_BLOCK("KnapsackDP::ComputeItem");
				if (tid == 0)
				{
					START_BLOCK("KnapsackDP::ComputeThreadBlock");
				}
				const int wi = weights[i - 1];
				const int vi = values[i - 1];
				const int *prev = dp.row(i - 1).data();
				int *cur = dp.row(i).data();
				for (std::int64_t w = begin; w < end; ++w)
				{
					if (wi <= w)
					{
						cur[w] = std::max(prev[w], prev[w - wi] + vi);
					}
					else
					{
						cur[w] = prev[w];
					}
				}
				if (tid == 0)
				{
					END_BLOCK("KnapsackDP::ComputeThreadBlock");
				}
				END_BLOCK("KnapsackDP::ComputeItem");
				// Row i+1 reads row i at arbitrary columns w - w_{i+1}: every slice of
				// row i must be complete before anyone starts the next row.
#pragma omp barrier
			}
		}
	}
	END_BLOCK("KnapsackDP::Compute");

	ScopedPhase phase("backtrack");
	// Backtrack to find the items included in the knapsack
	std::vector<int> includedItems;
	int totalValue = dp(n, capacity);
	int totalWeight = 0;
	int w = capacity;

	for (int i = n; i > 0 && totalValue > 0; --i)
	{
		if (totalValue != dp(i - 1, w))
		{
			includedItems.push_back(i - 1); // Store the index of the included item
			totalValue -= values[i - 1];
			w -= weights[i - 1];
			totalWeight += weights[i - 1];
		}
	}

	return {includedItems, dp(n, capacity), totalWeight};
}
