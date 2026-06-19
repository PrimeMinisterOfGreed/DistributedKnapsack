#include "Knapsack/knapsackcopa.hpp"
#include "Knapsackmpi/utils.tpp"
#include "knapsackmpi.hpp"
#include <concepts>
#include <ranges>

std::optional<KnapsackSolution> knapsackcopampi(boost::mpi::communicator &comm, const std::vector<int> &weights,
												const std::vector<int> &values, int capacity)
{
	using namespace std::ranges::views;
	auto list = zip(weights, values);
	int world = comm.size();
	int n = static_cast<int>(weights.size());
	if (n == 0)
		return KnapsackSolution{};
	if (n % 2 != 0)
		return std::nullopt;

	auto Alist = take(list, n / 2);
	auto Blist = drop(list, n / 2);
	auto A = mpi_generate_copa_subsets(comm, Alist);
	auto B = mpi_generate_copa_subsets(comm, Blist, true);
	int N = static_cast<int>(A.size());
	if (N == 0 || static_cast<int>(B.size()) != N)
		return std::nullopt;
	// Stage 2 : Parallel suffix max for B (MaxBj and Lj)
	std::vector<CopaBlock> blocksA(world);
	std::vector<CopaBlock> blocksB(world);

	// Note: we avoid communication during distribution, each node will only communicate the tasks to perform
	distribute_block_per_processor(A, blocksA, world);
	distribute_block_per_processor(B, blocksB, world);

	std::vector<std::pair<CopaBlock, CopaBlock>> local_remaining_pairs{};

	auto i = comm.rank();
	prune_block_pair(blocksA[i], blocksB, local_remaining_pairs, capacity, i);
	if (local_remaining_pairs.size() > 1)
	{
		spdlog::warn("Process {} has {} remaining pairs after pruning, this may lead to load imbalance", i,
					 local_remaining_pairs.size());
	}
	auto [blockA, blockB] = local_remaining_pairs[0];
	auto solution = block_pair_pointer_search(blockA, blockB, capacity);
	std::vector<BlockPairSearchResult> all_solutions(world);
	gather(comm, solution, all_solutions, 0);
	if (comm.rank() == 0)
	{
		auto best = std::max_element(all_solutions.begin(), all_solutions.end(),[](const BlockPairSearchResult &a, const BlockPairSearchResult &b) {
			return a.bestVal < b.bestVal;
		});

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