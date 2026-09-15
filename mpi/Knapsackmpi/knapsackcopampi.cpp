#include "Knapsack/knapsackcopa.hpp"
#include "Knapsackmpi/utils.hxx"
#include "knapsackmpi.hpp"
#include <concepts>
#include <omp.h>
#include <ranges>

std::optional<KnapsackSolution> knapsackcopampi(boost::mpi::communicator &comm, const std::vector<int> &weights,
												const std::vector<int> &values, int capacity)
{
	int world = comm.size();
	int n = static_cast<int>(weights.size());
	if (n == 0)
	{
		spdlog::error("Error: No items provided for knapsackcopampi");
		return std::nullopt;
	}
	if (n % 2 != 0)
	{
		spdlog::error("Error: Odd number of items provided for knapsackcopampi");
		return std::nullopt;
	}

	std::vector<std::pair<int, int>> Alist, Blist;
	Alist.reserve(n / 2);
	Blist.reserve(n / 2);
	for (int i = 0; i < n / 2; ++i)
		Alist.emplace_back(weights[i], values[i]);
	for (int i = n / 2; i < n; ++i)
		Blist.emplace_back(weights[i], values[i]);
	auto A = mpi_generate_copa_subsets(comm, Alist);
	auto B = mpi_generate_copa_subsets(comm, Blist, true);
	int N = static_cast<int>(B.size());
	// Stage 2 : Parallel suffix max for B (MaxBj and Lj)
	int numThreads = std::max(1, omp_get_max_threads());
	std::vector<CopaBlock> blocksA_local;
	std::vector<CopaBlock> blocksB(world);
	std::vector<CopaDistributionIndex> blockBdescriptors(world);
	auto blockAdesc = mpi_distribute_block_per_process(comm, A);
	auto blockBdesc = mpi_distribute_block_per_process(comm, B);
	boost::mpi::all_gather(comm, blockBdesc, blockBdescriptors.data());

	int sliceLen = blockAdesc.end - blockAdesc.start;
	numThreads = std::clamp(numThreads, 1, sliceLen);
	// Subdivide this process's A slice into per-thread contiguous sub-blocks
	{
		int subSize = sliceLen / numThreads;
		int remainder = sliceLen % numThreads;
		int pos = blockAdesc.start;
		blocksA_local.reserve(numThreads);
		for (int t = 0; t < numThreads; ++t)
		{
			int len = subSize + (t < remainder ? 1 : 0);
			CopaBlock cb;
			cb.block = std::span(A).subspan(pos, len);
			int maxVal = 0;
			for (const auto &elem : cb.block)
			{
				if (elem.totalValue > maxVal)
					maxVal = elem.totalValue;
			}
			cb.maxValue = maxVal;
			blocksA_local.push_back(cb);
			pos += len;
		}
	}
	spdlog::debug("Process {} has block A: start {}, end {}, maxValue {}", comm.rank(), blockAdesc.start,
				  blockAdesc.end, blockAdesc.maxValue);
	for (int i = 0; i < world; ++i)
	{

		blocksB[i].block =
			std::span(B).subspan(blockBdescriptors[i].start, blockBdescriptors[i].end - blockBdescriptors[i].start);
		blocksB[i].maxValue = blockBdescriptors[i].maxValue;
	}

	// Note: we avoid communication during distribution, each node will only communicate the tasks to perform
	std::vector<std::pair<CopaBlock, CopaBlock>> local_remaining_pairs{};
	spdlog::debug("Process {} has distributed blocks A:{} B:{}, starting pruning", comm.rank(),
				  blocksA_local.size(), blocksB.size());

	comm.barrier();

	prune(blocksA_local, blocksB, local_remaining_pairs, capacity, numThreads);
	spdlog::debug("Process {} has pruned block pairs, remaining pairs: {}", comm.rank(), local_remaining_pairs.size());

	// Stage 3+4 : Parallel saved-max across all remaining block pairs
	int m = static_cast<int>(local_remaining_pairs.size());
	std::vector<int> localBestVal(m, 0), localBestAIdx(m, 0), localBestBIdx(m, 0);
	parallel_save_max(local_remaining_pairs, localBestVal, localBestAIdx, localBestBIdx, capacity);
	BlockPairSearchResult solution{};
	for (int i = 0; i < m; ++i)
	{
		if (localBestVal[i] > solution.bestVal)
		{
			solution = BlockPairSearchResult{localBestVal[i], localBestAIdx[i], localBestBIdx[i]};
		}
	}
	BlockPairSearchResult best{};
	reduce(comm, solution,best,boost::mpi::maximum<BlockPairSearchResult>(),0);
	if (comm.rank() == 0)
	{
		
		// Reconstruct solution
		KnapsackSolution solution{};
		solution.totalValue = best.bestVal;
		solution.totalWeight = A[best.bestAIdx].totalWeight + B[best.bestBIdx].totalWeight;
		auto items = (A[best.bestAIdx] << B[best.bestBIdx]).getItemIndices();
		solution.items.reserve(items.size());
		solution.items.insert(solution.items.end(), items.begin(), items.end());
		return solution;
	}
	return {};
}