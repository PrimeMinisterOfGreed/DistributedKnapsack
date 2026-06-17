#include "knapsackcopa.hpp"
#include "utils.tpp"
#include <algorithm>
#include <ranges>

std::optional<KnapsackSolution> knapsackcopasequential(const std::vector<int> &weights, const std::vector<int> &values,
													   int capacity)
{
	using namespace std::views;
	auto list = zip(weights, values);
	auto Alist = take(list, list.size() / 2);
	auto Blist = drop(list, list.size() / 2);
	auto A = generate_copa_subsets(Alist);
	auto B = std::views::reverse(generate_copa_subsets(Blist));

	auto N = A.size();
	if (N == 0 || B.size() != N)
		return std::nullopt;

	// Step 1: Compute suffix max for B (MaxBj and Lj)
	std::vector<int> MaxB(N);
	std::vector<int> L(N);
	MaxB[N - 1] = B[N - 1].totalValue;
	L[N - 1] = static_cast<int>(N - 1);

	for (int i = static_cast<int>(N - 2); i >= 0; i--)
	{
		if (B[i].totalValue > MaxB[i + 1])
		{
			MaxB[i] = B[i].totalValue;
			L[i] = i;
		}
		else
		{
			MaxB[i] = MaxB[i + 1];
			L[i] = L[i + 1];
		}
	}

	// Step 2: Search for optimal solution
	int bestValue = 0;
	std::pair<int, int> X1{0, 0};

	int i = 0, j = 0;
	while (i < static_cast<int>(N) && j < static_cast<int>(N))
	{
		if (A[i].totalWeight + B[j].totalWeight > capacity)
		{
			j++;
			continue;
		}
		if (A[i].totalValue + MaxB[j] > bestValue)
		{
			bestValue = A[i].totalValue + MaxB[j];
			X1 = {i, L[j]};
		}
		i++;
	}

	// Step 3: Build solution
	auto best = A[X1.first] << B[X1.second];
	KnapsackSolution solution{};
	solution.totalValue = bestValue;
	solution.totalWeight = A[X1.first].totalWeight + B[X1.second].totalWeight;
	auto items = best.getItemIndices();
	solution.items.reserve(items.size());
	solution.items.insert(solution.items.end(), items.begin(), items.end());
	return solution;
}

std::optional<KnapsackSolution> knapsackcopa(const std::vector<int> &weights, const std::vector<int> &values,
											 int capacity, int numThreads)
{
	using namespace std::views;
	auto list = zip(weights, values);

	int n = static_cast<int>(weights.size());
	if (n == 0)
		return KnapsackSolution{};
	if (n % 2 != 0)
		return std::nullopt;

	auto Alist = take(list, n / 2);
	auto Blist = drop(list, n / 2);
	auto A = generate_copa_subsets(Alist, numThreads);
	auto B = generate_copa_subsets(Blist, numThreads);
	std::reverse(B.begin(), B.end());

	int N = static_cast<int>(A.size());
	if (N == 0 || static_cast<int>(B.size()) != N)
		return std::nullopt;

	// Stage 3: Parallel pruning
	std::vector<std::pair<CopaBlock, CopaBlock>> remainingPairs;
	prune(A, B, remainingPairs, capacity, numThreads);

	int k = static_cast<int>(remainingPairs.size());

	// Stages 4+5: For each remaining block pair, compute suffix max and search
	std::vector<int> localBestVal(k, 0);
	std::vector<int> localBestAIdx(k, 0);
	std::vector<int> localBestBIdx(k, 0);

	parallel_save_max(A, B, remainingPairs, localBestVal, localBestAIdx, localBestBIdx, capacity, k);

	// Reduce across all remaining pairs
	int bestAIdx = 0, bestBIdx = 0, bestValue = 0;
#pragma omp parallel for num_threads(k) reduction(max : bestValue)
	for (int i = 0; i < k; ++i)
	{
		if (localBestVal[i] > bestValue)
		{
			bestValue = localBestVal[i];
			bestAIdx = localBestAIdx[i];
			bestBIdx = localBestBIdx[i];
		}
	}

	// Reconstruct solution
	KnapsackSolution solution{};
	solution.totalValue = bestValue;
	solution.totalWeight = A[bestAIdx].totalWeight + B[bestBIdx].totalWeight;
	auto items = (A[bestAIdx] << B[bestBIdx]).getItemIndices();
	solution.items.reserve(items.size());
	solution.items.insert(solution.items.end(), items.begin(), items.end());
	return solution;
}
