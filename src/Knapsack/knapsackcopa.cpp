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
	KnapsackSolution solution{};
	solution.totalValue = bestValue;
	solution.totalWeight = A[X1.first].totalWeight + B[X1.second].totalWeight;
	solution.items.reserve(A[X1.first].items.size() + B[X1.second].items.size());
	solution.items.insert(solution.items.end(), A[X1.first].items.begin(), A[X1.first].items.end());
	solution.items.insert(solution.items.end(), B[X1.second].items.begin(), B[X1.second].items.end());
	return solution;
}

std::optional<KnapsackSolution> knapsackcopa(const std::vector<int> &weights, const std::vector<int> &values,
											 int capacity, int numThreads)
{
	using namespace std::views;
	auto list = zip(weights, values);
	auto Alist = take(list, list.size() / 2);
	auto Blist = drop(list, list.size() / 2);
	auto A = generate_copa_subsets(Alist);
	//TODO This is evaluated eagerly because we need to reverse it, explore if we can avoid that
	auto B = generate_copa_subsets(Blist);
	std::reverse(B.begin(), B.end());
	std::vector<CopaBlock> blocksA{};
	std::vector<CopaBlock> blocksB{};
	prune(A, B, blocksA, blocksB, capacity, numThreads);
}

