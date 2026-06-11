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
	int bestValue = prune(A, B, remainingPairs, capacity, numThreads);

	int k = static_cast<int>(remainingPairs.size());
	if (k == 0)
	{
		KnapsackSolution solution{};
		solution.totalValue = bestValue;
		if (bestValue > 0)
		{
			int bestA = 0, bestB = 0;
			for (int i = 0; i < N; ++i)
			{
				if (A[i].totalValue > A[bestA].totalValue)
					bestA = i;
				if (B[i].totalValue > B[bestB].totalValue)
					bestB = i;
			}

			solution.totalWeight = A[bestA].totalWeight + B[bestB].totalWeight;
			auto Aitems = A[bestA].getItemIndices();
			auto Bitems = B[bestB].getItemIndices();
			solution.items.reserve(Aitems.size() + Bitems.size());
			solution.items.insert(solution.items.end(), Aitems.begin(), Aitems.end());
			solution.items.insert(solution.items.end(), Bitems.begin(), Bitems.end());
		}
		return solution;
	}

	// Stages 4+5: For each remaining block pair, compute suffix max and search
	std::vector<int> localBestVal(k, 0);
	std::vector<int> localBestAIdx(k, 0);
	std::vector<int> localBestBIdx(k, 0);

#pragma omp parallel for num_threads(numThreads)
	for (int i = 0; i < k; ++i)
	{
		const auto &blockA = remainingPairs[i].first;
		const auto &blockB = remainingPairs[i].second;

		int eA = static_cast<int>(blockA.block.size());
		int eB = static_cast<int>(blockB.block.size());

		if (eA == 0 || eB == 0)
			continue;

		int aGlobalStart = static_cast<int>(blockA.block.data() - A.data());
		int bGlobalStart = static_cast<int>(blockB.block.data() - B.data());

		// Stage 4: Suffix max for this B block
		std::vector<int> suffixMaxVal(eB);
		std::vector<int> suffixMaxIdx(eB);
		block_suffix_max_values(blockB.block, bGlobalStart, suffixMaxVal, suffixMaxIdx);

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
				bestA = aGlobalStart + x;
				bestB = suffixMaxIdx[y];
			}
			x++;
		}
		localBestVal[i] = bestVal;
		localBestAIdx[i] = bestA;
		localBestBIdx[i] = bestB;
	}

	// Reduce across all remaining pairs
	int bestAIdx = 0, bestBIdx = 0;
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
