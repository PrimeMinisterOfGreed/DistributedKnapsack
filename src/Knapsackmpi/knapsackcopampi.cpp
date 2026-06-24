#include "Knapsack/knapsackcopa.hpp"
#include "Knapsackmpi/utils.tpp"
#include "knapsackmpi.hpp"
#include <concepts>
#include <ranges>

std::optional<KnapsackSolution> knapsackcopampi(boost::mpi::communicator &comm, const std::vector<int> &weights,
												const std::vector<int> &values, int capacity)
{
	int world = comm.size();
	int n = static_cast<int>(weights.size());
	if (n == 0)
		return KnapsackSolution{};
	if (n % 2 != 0)
		return std::nullopt;

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
	auto i = comm.rank();
	// Stage 2 : Parallel suffix max for B (MaxBj and Lj)
	CopaBlock blockA;
	std::vector<CopaBlock> blocksB(world);
	std::vector<CopaDistributionIndex> blockBdescriptors(world);
	auto blockAdesc = mpi_distribute_block_per_process(comm, A);
	auto blockBdesc = mpi_distribute_block_per_process(comm, B);
	boost::mpi::all_gather(comm, blockBdesc, blockBdescriptors.data());

	blockA.block = std::span(A).subspan(blockAdesc.start, blockAdesc.end - blockAdesc.start);
	blockA.maxValue = blockAdesc.maxValue;
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
	spdlog::debug("Process {} has distributed blocks A:{} B:{}, starting pruning", comm.rank(), blockA.block.size(),
				  blocksB.size());

	comm.barrier();

	prune_block_pair(blockA, blocksB, local_remaining_pairs, capacity, i);
	spdlog::debug("Process {} has pruned block pairs, remaining pairs: {}", i, local_remaining_pairs.size());

	if (local_remaining_pairs.size() > 1)
	{
		spdlog::warn("Process {} has {} remaining pairs after pruning, this may lead to load imbalance", i,
					 local_remaining_pairs.size());
	}
	BlockPairSearchResult solution{};
	if (!local_remaining_pairs.empty())
	{
		auto [remBlockA, remBlockB] = local_remaining_pairs[0];
		solution = block_pair_pointer_search(remBlockA, remBlockB, capacity);
	}
	std::vector<BlockPairSearchResult> all_solutions(world);
	gather(comm, solution, all_solutions, 0);
	if (comm.rank() == 0)
	{
		auto best = std::max_element(
			all_solutions.begin(), all_solutions.end(),
			[](const BlockPairSearchResult &a, const BlockPairSearchResult &b) { return a.bestVal < b.bestVal; });

		// Reconstruct solution
		KnapsackSolution solution{};
		solution.totalValue = best->bestVal;
		solution.totalWeight = A[best->bestAIdx].totalWeight + B[best->bestBIdx].totalWeight;
		auto items = (A[best->bestAIdx] << B[best->bestBIdx]).getItemIndices();
		solution.items.reserve(items.size());
		solution.items.insert(solution.items.end(), items.begin(), items.end());
		return solution;
	}
	return {};
}