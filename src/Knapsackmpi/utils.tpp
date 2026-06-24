#pragma once
#include "Knapsack/utils.tpp"
#include <boost/mpi.hpp>
#include <boost/mpi/collectives.hpp>
#include <fmt/core.h>
#include <ranges>

using communicator = boost::mpi::communicator;

struct ScatterProcedure
{
	std::vector<int> displacements;
	std::vector<int> sizes;
};

struct CoRankProcedure
{
	std::vector<int> output_sizes;
	std::vector<int> output_displacements;
	std::vector<int> a_sizes;
	std::vector<int> a_displacements;
	std::vector<int> b_sizes;
	std::vector<int> b_displacements;
};

ScatterProcedure compute_scatter_procedure(communicator &comm, const std::ranges::input_range auto &data)
{
	ScatterProcedure proc;
	int total_size = static_cast<int>(data.size());
	int num_procs = comm.size();
	auto local_size = total_size / num_procs;
	auto remainder = total_size % num_procs;

	proc.sizes.resize(num_procs);
	proc.displacements.resize(num_procs);

	for (int i = 0; i < num_procs; ++i)
	{
		proc.sizes[i] = local_size + (i < remainder ? 1 : 0);
		proc.displacements[i] = local_size * i + std::min(i, remainder);
	}
	return proc;
}

CoRankProcedure compute_corank_procedure(communicator &comm, const std::ranges::input_range auto &A,
										 const std::ranges::input_range auto &B)
{
	int total_size = static_cast<int>(A.size() + B.size());
	int num_procs = comm.size();

	CoRankProcedure proc;
	proc.output_sizes.resize(num_procs);
	proc.output_displacements.resize(num_procs);
	proc.a_sizes.resize(num_procs);
	proc.a_displacements.resize(num_procs);
	proc.b_sizes.resize(num_procs);
	proc.b_displacements.resize(num_procs);

	for (int p = 0; p < num_procs; ++p)
	{
		int start = p * total_size / num_procs;
		int end = (p + 1) * total_size / num_procs;

		auto [a_start, b_start] = co_rank(A, B, start);
		auto [a_end, b_end] = co_rank(A, B, end);

		proc.a_sizes[p] = a_end - a_start;
		proc.a_displacements[p] = a_start;
		proc.b_sizes[p] = b_end - b_start;
		proc.b_displacements[p] = b_start;

		proc.output_sizes[p] = (a_end - a_start) + (b_end - b_start);
		proc.output_displacements[p] = start;
	}

	return proc;
}

/**
 * @brief Merge two sorted ranges in parallel using MPI. Each process computes its assigned portion of the merge and
 * then gathers the results.
 *
 * @param comm
 * @param A
 * @param B
 * @param output
 * @warning It's not optimal since it gathers the entire merged output on all processes, but it's a simple way to
 * implement parallel merging with MPI.
 */
constexpr void mpi_parallel_merge(communicator &comm, const std::vector<CopaSubset> &A,
								  const std::vector<CopaSubset> &B, std::vector<CopaSubset> &output)
{
	using namespace boost::mpi;

	auto proc = compute_corank_procedure(comm, A, B);

	int rank = comm.rank();
	int a_start = proc.a_displacements[rank];
	int a_end = a_start + proc.a_sizes[rank];
	int b_start = proc.b_displacements[rank];
	int b_end = b_start + proc.b_sizes[rank];

	comm.barrier();

	std::vector<CopaSubset> local_result(proc.output_sizes[rank]);
	std::merge(A.begin() + a_start, A.begin() + a_end, B.begin() + b_start, B.begin() + b_end, local_result.begin());

	comm.barrier();

	gatherv(comm, local_result.data(), proc.output_sizes[rank], output.data(), proc.output_sizes,
			proc.output_displacements, 0);

	broadcast(comm, output, 0);
}

constexpr std::vector<CopaSubset> mpi_generate_copa_subsets(communicator &comm,
															const std::ranges::input_range auto &items,
															bool reverse = false)
{
	using namespace boost::mpi;
	std::vector<CopaSubset> subsets{CopaSubset{}};
	for (int item_idx = 0; const auto &item : items)
	{
		auto shifted = subsets;
		auto [w, v] = item;
		// For small subset sizes, do the shifting locally to avoid communication overhead
		if (subsets.size() < 100 * comm.size())
		{
			for (int i = 0; i < static_cast<int>(shifted.size()); i++)
			{
				shifted[i].addItem(item_idx, w, v);
			}
		}

		else
		{
			// For larger subset sizes, distribute the shifting across MPI processes
			int local_size = static_cast<int>(shifted.size()) / comm.size();
			int remainder = static_cast<int>(shifted.size()) % comm.size();
			auto procedure = compute_scatter_procedure(comm, shifted);
			std::vector<CopaSubset> local_shifted{static_cast<size_t>(procedure.sizes[comm.rank()]), CopaSubset{}};
			scatterv(comm, shifted.data(), procedure.sizes, procedure.displacements, local_shifted.data(),
					 procedure.sizes[comm.rank()], 0);

			for (int i = 0; i < static_cast<int>(local_shifted.size()); i++)
			{
				local_shifted[i].addItem(item_idx, w, v);
			}

			// Gather the shifted subsets from all processes
			std::vector<CopaSubset> global_shifted{shifted.size()};
			gatherv(comm, local_shifted.data(), procedure.sizes[comm.rank()], global_shifted.data(), procedure.sizes,
					procedure.displacements, 0);
			shifted = std::move(global_shifted);
		}
		item_idx++;

		std::vector<CopaSubset> newSubsets;
		if (subsets.size() < 100 * comm.size())
		{
			newSubsets.resize(subsets.size() + shifted.size());
			parallel_merge(subsets, shifted, newSubsets, 1);
		}
		else
		{
			if (comm.rank() != 0)
			{
				subsets.clear();
				shifted.clear();
			}
			broadcast(comm, subsets, 0);
			broadcast(comm, shifted, 0);
			newSubsets.resize(subsets.size() + shifted.size());
			mpi_parallel_merge(comm, subsets, shifted, newSubsets);
		}
		spdlog::debug("Rank {}: After merging item {}, new subset size: {}", comm.rank(), item_idx, newSubsets.size());
		subsets = prune_dominated_subsets(newSubsets);
	}
	if (reverse)
		std::ranges::reverse(subsets);
	for (int i = 0; i < subsets.size(); i++)
		subsets[i].index = i;
	return subsets;
}

#pragma region Optimizers

struct CopaDistributionIndex
{
	int start;
	int end;
	int maxValue;

	template<typename Archive>
	void serialize(Archive &ar, const unsigned int version)
	{
		ar & start;
		ar & end;
		ar & maxValue;
	}
};

constexpr CopaDistributionIndex mpi_distribute_block_per_process(communicator &comm, const CopaRange auto &input)
{
	using value_type = std::ranges::range_value_t<decltype(input)>;
	// calculate the size of the blocks for the thread
	int n = static_cast<int>(std::ranges::size(input));
	int k = comm.size();
	int i = comm.rank();

	int block_size = n / k;
	int remainder = n % k;

	int start = i * block_size + std::min(i, remainder);
	int end = start + block_size + (i < remainder ? 1 : 0);
	auto span = std::ranges::subrange(std::ranges::begin(input) + start, std::ranges::begin(input) + end);
	int max_val = 0;
	for (const auto &elem : span)
	{
		if (elem.totalValue > max_val)
			max_val = elem.totalValue;
	}
	return {start, end, max_val};
}

constexpr void mpi_prune(communicator &comm, const CopaBlock &blockA, const CopaBlockInputRange auto &blocksB,
						 CopaBlockPairingOutRange auto &blocks, int capacity)
{
	using namespace boost::mpi;
	using namespace std::ranges;
	int k = comm.size();
	int i = comm.rank();
	int best_value = 0;
	for (int j = i; j < k + i; ++j)
	{
		int b_idx = j % k;
		const auto &blockB = blocksB[b_idx];
		if (blockB.block.empty())
			continue;
		int Z = blockA.block.front().totalWeight + blockB.block.back().totalWeight;
		int Y = blockA.block.back().totalWeight + blockB.block.front().totalWeight;

		// Note: prune is done by not adding the pair to local_results
		if (Y <= capacity)
		{
			// All pairs in this block pair are valid; save max profit and prune
			if (blockA.maxValue + blockB.maxValue > best_value)
			{
				best_value = blockA.maxValue + blockB.maxValue;
			}
			blocks.emplace_back(blockA, blockB);
		}
		else if (Z <= capacity && Y > capacity)
		{
			blocks.emplace_back(blockA, blockB);
		}
		else if (Z > capacity)
		{
			// No pairs in this block pair are valid; prune
		}
	}
}

constexpr void mpi_parallel_save_max(communicator &comm, const CopaBlock &blockA, const CopaBlock &blockB,
									 int &processBestVal, int &processBestAIdx, int &processBestBIdx, int capacity)
{
	using namespace boost::mpi;
	int k = comm.size();
	int rank = comm.rank();

	int eA = static_cast<int>(blockA.block.size());
	int eB = static_cast<int>(blockB.block.size());

	if (eA == 0 || eB == 0)
		return;

	// Stage 4: Suffix max for this B block
	std::vector<int> suffixMaxVal(eB);
	std::vector<int> suffixMaxIdx(eB);
	block_suffix_max_values(blockB.block, suffixMaxVal, suffixMaxIdx);

	// Stage 5: Two-pointer search within this block pair
	int x = 0, y = 0;
	int bestVal = 0, bestA = 0, bestB = 0;
	while (x < eA && y < eB)
	{
		if (blockA.block[x].totalWeight + blockB.block[y].totalWeight > capacity)
		{
			y++;
			continue;
		}
		int candidate = blockA.block[x].totalValue + suffixMaxVal[y];
		if (candidate > bestVal)
		{
			bestVal = candidate;
			bestA = blockA.block[x].index;
			bestB = suffixMaxIdx[y];
		}
		x++;
	}
	processBestVal = bestVal;
	processBestAIdx = bestA;
	processBestBIdx = bestB;
}

#pragma endregion