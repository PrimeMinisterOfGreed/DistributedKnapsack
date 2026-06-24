#include <gtest/gtest.h>
#include <boost/mpi.hpp>
#include <random>
#include "Knapsack/knapsack.hpp"
#include "Knapsack/knapsackcopa.hpp"
#include "Knapsackmpi/knapsackmpi.hpp"
#include "Knapsackmpi/utils.tpp"
TEST(CopaSubsetSerialization, BroadcastBetweenNodes)
{
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	boost::mpi::communicator comm;
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	CopaSubset subset;

	if (rank == 0)
	{
		subset.addItem(2, 10, 100);
		subset.addItem(5, 20, 200);
	}

	boost::mpi::broadcast(comm, subset, 0);

	EXPECT_EQ(subset.totalWeight, 30);
	EXPECT_EQ(subset.totalValue, 300);

	auto indices = subset.getItemIndices();
	ASSERT_EQ(indices.size(), 2);
	EXPECT_EQ(indices[0], 2);
	EXPECT_EQ(indices[1], 5);
}

TEST(CopaSubsetSerialization, SendReceiveBetweenNodes)
{
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	boost::mpi::communicator comm;
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	if (rank == 0)
	{
		CopaSubset subset;
		subset.addItem(3, 15, 150);
		subset.addItem(7, 25, 250);
		comm.send(1, 0, subset);
	}
	else if (rank == 1)
	{
		CopaSubset subset;
		comm.recv(0, 0, subset);

		EXPECT_EQ(subset.totalWeight, 40);
		EXPECT_EQ(subset.totalValue, 400);

		auto indices = subset.getItemIndices();
		ASSERT_EQ(indices.size(), 2);
		EXPECT_EQ(indices[0], 3);
		EXPECT_EQ(indices[1], 7);
	}
}

struct MergeTestParam
{
	std::vector<CopaSubset> a;
	std::vector<CopaSubset> b;
	std::vector<CopaSubset> expected;
};

class MpiParallelMergeTest : public ::testing::TestWithParam<MergeTestParam> {};

constexpr static MergeTestParam make_basic_merge_param()
{
	// A: sorted by weight (non-decreasing)
	// B: sorted by weight (non-decreasing)
	std::vector<CopaSubset> a = {
		CopaSubset{{}, 1, 10},
		CopaSubset{{}, 4, 40},
		CopaSubset{{}, 7, 70},
	};
	std::vector<CopaSubset> b = {
		CopaSubset{{}, 2, 20},
		CopaSubset{{}, 5, 50},
		CopaSubset{{}, 8, 80},
		CopaSubset{{}, 9, 90},
	};
	std::vector<CopaSubset> expected = {
		CopaSubset{{}, 1, 10},
		CopaSubset{{}, 2, 20},
		CopaSubset{{}, 4, 40},
		CopaSubset{{}, 5, 50},
		CopaSubset{{}, 7, 70},
		CopaSubset{{}, 8, 80},
		CopaSubset{{}, 9, 90},
	};
	return {std::move(a), std::move(b), std::move(expected)};
}

constexpr static MergeTestParam make_empty_a_param()
{
	std::vector<CopaSubset> a;
	std::vector<CopaSubset> b = {
		CopaSubset{{}, 1, 10},
		CopaSubset{{}, 3, 30},
	};
	std::vector<CopaSubset> expected = b;
	return {std::move(a), std::move(b), std::move(expected)};
}

constexpr static MergeTestParam make_empty_b_param()
{
	std::vector<CopaSubset> a = {
		CopaSubset{{}, 2, 20},
		CopaSubset{{}, 4, 40},
	};
	std::vector<CopaSubset> b;
	std::vector<CopaSubset> expected = a;
	return {std::move(a), std::move(b), std::move(expected)};
}

constexpr static MergeTestParam make_both_empty_param()
{
	return {{}, {}, {}};
}

constexpr static MergeTestParam make_interleaved_param()
{
	std::vector<CopaSubset> a = {
		CopaSubset{{}, 1, 1},
		CopaSubset{{}, 3, 3},
		CopaSubset{{}, 5, 5},
		CopaSubset{{}, 7, 7},
		CopaSubset{{}, 9, 9},
	};
	std::vector<CopaSubset> b = {
		CopaSubset{{}, 2, 2},
		CopaSubset{{}, 4, 4},
		CopaSubset{{}, 6, 6},
		CopaSubset{{}, 8, 8},
		CopaSubset{{}, 10, 10},
	};
	std::vector<CopaSubset> expected = {
		CopaSubset{{}, 1, 1},
		CopaSubset{{}, 2, 2},
		CopaSubset{{}, 3, 3},
		CopaSubset{{}, 4, 4},
		CopaSubset{{}, 5, 5},
		CopaSubset{{}, 6, 6},
		CopaSubset{{}, 7, 7},
		CopaSubset{{}, 8, 8},
		CopaSubset{{}, 9, 9},
		CopaSubset{{}, 10, 10},
	};
	return {std::move(a), std::move(b), std::move(expected)};
}

INSTANTIATE_TEST_SUITE_P(
	MpiParallelMerge,
	MpiParallelMergeTest,
	::testing::Values(
		make_basic_merge_param(),
		make_empty_a_param(),
		make_empty_b_param(),
		make_both_empty_param(),
		make_interleaved_param()
	)
);

TEST_P(MpiParallelMergeTest, MergesCorrectlyAcrossRanks)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;
	auto param = GetParam();

	std::vector<CopaSubset> output(param.expected.size());
	mpi_parallel_merge(comm, param.a, param.b, output);

	ASSERT_EQ(output.size(), param.expected.size());

	for (size_t i = 0; i < output.size(); ++i)
	{
		EXPECT_EQ(output[i].totalWeight, param.expected[i].totalWeight)
			<< "Mismatch at index " << i << " on rank " << rank;
		EXPECT_EQ(output[i].totalValue, param.expected[i].totalValue)
			<< "Mismatch at index " << i << " on rank " << rank;
	}

	// Verify output is sorted by weight
	for (size_t i = 1; i < output.size(); ++i)
	{
		EXPECT_LE(output[i - 1].totalWeight, output[i].totalWeight)
			<< "Output not sorted at index " << i << " on rank " << rank;
	}
}

static std::vector<CopaSubset> reference_subsets(const std::vector<std::pair<int, int>> &items)
{
	return generate_copa_subsets(items, 2);
}

TEST(MpiGenerateCopaSubsets, SmallSetMatchesSequential)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	std::vector<std::pair<int, int>> items{{1, 5}, {2, 3}, {3, 8}, {4, 2}};

	auto mpi_result = mpi_generate_copa_subsets(comm, items);
	auto seq_result = reference_subsets(items);

	ASSERT_EQ(mpi_result.size(), seq_result.size());

	for (size_t i = 0; i < mpi_result.size(); ++i)
	{
		EXPECT_EQ(mpi_result[i].totalWeight, seq_result[i].totalWeight)
			<< "Weight mismatch at index " << i << " on rank " << rank;
		EXPECT_EQ(mpi_result[i].totalValue, seq_result[i].totalValue)
			<< "Value mismatch at index " << i << " on rank " << rank;
	}

	for (size_t i = 1; i < mpi_result.size(); ++i)
	{
		EXPECT_LE(mpi_result[i - 1].totalWeight, mpi_result[i].totalWeight)
			<< "MPI result not sorted at index " << i << " on rank " << rank;
	}
}

TEST(MpiGenerateCopaSubsets, EmptyInputReturnsEmptySubset)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;
	std::vector<std::pair<int, int>> items;

	auto result = mpi_generate_copa_subsets(comm, items);

	ASSERT_EQ(result.size(), 1u);
	EXPECT_EQ(result[0].totalWeight, 0);
	EXPECT_EQ(result[0].totalValue, 0);
}

TEST(MpiGenerateCopaSubsets, SingleItemProducesTwoSubsets)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;
	std::vector<std::pair<int, int>> items{{7, 11}};

	auto result = mpi_generate_copa_subsets(comm, items);

	ASSERT_EQ(result.size(), 2u);
	EXPECT_EQ(result[0].totalWeight, 0);
	EXPECT_EQ(result[0].totalValue, 0);
	EXPECT_EQ(result[1].totalWeight, 7);
	EXPECT_EQ(result[1].totalValue, 11);
	EXPECT_TRUE(result[0].getItemIndices().empty());
	EXPECT_EQ(result[1].getItemIndices(), std::vector<int>{0});
}

struct BlockExpectation
{
	int size;
	int max_value;
};

static void check_block(const CopaBlock &block, BlockExpectation expected, int rank)
{
	ASSERT_EQ(block.block.size(), static_cast<size_t>(expected.size))
		<< "Block size mismatch on rank " << rank;
	EXPECT_EQ(block.maxValue, expected.max_value)
		<< "Max value mismatch on rank " << rank;
}

static BlockExpectation expected_block(int n, int k, int rank, const std::vector<int> &values)
{
	int block_size = n / k;
	int rem = n % k;
	int sz = block_size + (rank < rem ? 1 : 0);
	int start = block_size * rank + std::min(rank, rem);

	int mx = 0;
	for (int i = start; i < start + sz && i < n; ++i)
		if (values[i] > mx)
			mx = values[i];
	return {sz, mx};
}

TEST(MpiDistributeBlock, BasicDistribution)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2);

	boost::mpi::communicator comm;

	std::vector<CopaSubset> input;
	if (rank == 0)
	{
		for (int i = 1; i <= 10; ++i)
			input.push_back(CopaSubset{{}, i * 10, i * 10});
	}
	broadcast(comm, input, 0);

	// New API: mpi_distribute_block_per_process returns CopaDistributionIndex
	auto desc = mpi_distribute_block_per_process(comm, input);

	// Construct CopaBlock from the descriptor
	CopaBlock local_block;
	local_block.block = std::span(input).subspan(desc.start, desc.end - desc.start);
	local_block.maxValue = desc.maxValue;

	std::vector<int> values;
	for (auto &s : input)
		values.push_back(s.totalValue);

	auto exp = expected_block(static_cast<int>(input.size()), comm.size(), rank, values);
	check_block(local_block, exp, rank);
}


TEST(MpiDistributeBlock, UnevenDistribution)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2);

	boost::mpi::communicator comm;

	int n = 7;
	std::vector<CopaSubset> input;
	if (rank == 0)
	{
		for (int i = 1; i <= n; ++i)
			input.push_back(CopaSubset{{}, i, i * 10});
	}
	broadcast(comm, input, 0);

	// New API: mpi_distribute_block_per_process returns CopaDistributionIndex
	auto desc = mpi_distribute_block_per_process(comm, input);

	// Construct CopaBlock from the descriptor
	CopaBlock local_block;
	local_block.block = std::span(input).subspan(desc.start, desc.end - desc.start);
	local_block.maxValue = desc.maxValue;

	std::vector<int> values;
	for (auto &s : input)
		values.push_back(s.totalValue);

	auto exp = expected_block(static_cast<int>(input.size()), comm.size(), rank, values);
	check_block(local_block, exp, rank);
}




TEST(MpiDistributeBlock, MaxValueCorrectlyComputed)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2);

	boost::mpi::communicator comm;

	// Create input where max value is not at the end of the local block
	std::vector<CopaSubset> input;
	if (rank == 0)
	{
		// Values: 10, 50, 30, 70, 20, 80
		// With 2 processes: rank 0 gets [10, 50, 30], rank 1 gets [70, 20, 80]
		for (int v : {10, 50, 30, 70, 20, 80})
			input.push_back(CopaSubset{{}, v, v});
	}
	broadcast(comm, input, 0);

	// New API: mpi_distribute_block_per_process returns CopaDistributionIndex
	auto desc = mpi_distribute_block_per_process(comm, input);

	// Construct CopaBlock from the descriptor
	CopaBlock local_block;
	local_block.block = std::span(input).subspan(desc.start, desc.end - desc.start);
	local_block.maxValue = desc.maxValue;

	int expected_max;
	if (rank == 0)
	{
		expected_max = 50; // max of [10, 50, 30]
	}
	else
	{
		expected_max = 80; // max of [70, 20, 80]
	}

	EXPECT_EQ(local_block.maxValue, expected_max)
		<< "Max value mismatch on rank " << rank;
}

TEST(MpiPrune, PrunesBlockPairsCorrectly)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	std::vector<CopaSubset> a_subsets{CopaSubset{{}, 1, 5}, CopaSubset{{}, 2, 10}};
	CopaBlock blockA{std::span<const CopaSubset>(a_subsets), 10};

	std::vector<CopaSubset> b1_subsets{CopaSubset{{}, 1, 5}, CopaSubset{{}, 2, 10}};
	std::vector<CopaSubset> b2_subsets{CopaSubset{{}, 3, 15}};
	std::vector<CopaBlock> blocksB{
		CopaBlock{std::span<const CopaSubset>(b1_subsets), 10},
		CopaBlock{std::span<const CopaSubset>(b2_subsets), 15},
	};

	std::vector<std::pair<CopaBlock, CopaBlock>> local_results;
	int capacity = 3;
	mpi_prune(comm, blockA, blocksB, local_results, capacity);

	// Each rank processes its own block pairs; verify only non-empty results
	for (const auto &[a, b] : local_results)
	{
		EXPECT_FALSE(a.block.empty());
		EXPECT_FALSE(b.block.empty());
		if (!a.block.empty() && !b.block.empty())
		{
			int Z = a.block.front().totalWeight + b.block.back().totalWeight;
			int Y = a.block.back().totalWeight + b.block.front().totalWeight;
			EXPECT_LE(Z, capacity) << "Z should be <= capacity for kept block pair on rank " << rank;
			EXPECT_GT(Y, capacity) << "Y should be > capacity for kept block pair on rank " << rank;
		}
	}
}

TEST(MpiPrune, PrunesWhenAllPairsValid)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2);

	boost::mpi::communicator comm;

	std::vector<CopaSubset> a_subsets{CopaSubset{{}, 1, 5}, CopaSubset{{}, 2, 10}, CopaSubset{{}, 3, 15}};
	CopaBlock blockA{std::span<const CopaSubset>(a_subsets), 15};

	std::vector<CopaSubset> b1_subsets{CopaSubset{{}, 1, 5}};
	std::vector<CopaSubset> b2_subsets{CopaSubset{{}, 2, 10}, CopaSubset{{}, 3, 15}};
	std::vector<CopaSubset> b3_subsets{CopaSubset{{}, 1, 3}};
	std::vector<CopaBlock> blocksB{
		CopaBlock{std::span<const CopaSubset>(b1_subsets), 5},
		CopaBlock{std::span<const CopaSubset>(b2_subsets), 15},
		CopaBlock{std::span<const CopaSubset>(b3_subsets), 3},
	};

	std::vector<std::pair<CopaBlock, CopaBlock>> local_results;
	int capacity = 100;
	mpi_prune(comm, blockA, blocksB, local_results, capacity);

	EXPECT_TRUE(local_results.empty()) << "All pairs should be pruned when Y <= capacity for all block pairs";
}

TEST(MpiPrune, PrunesWhenNoPairsValid)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2);

	boost::mpi::communicator comm;

	std::vector<CopaSubset> a_subsets{CopaSubset{{}, 10, 50}};
	CopaBlock blockA{std::span<const CopaSubset>(a_subsets), 50};

	std::vector<CopaSubset> b1_subsets{CopaSubset{{}, 10, 50}};
	std::vector<CopaSubset> b2_subsets{CopaSubset{{}, 15, 60}};
	std::vector<CopaBlock> blocksB{
		CopaBlock{std::span<const CopaSubset>(b1_subsets), 50},
		CopaBlock{std::span<const CopaSubset>(b2_subsets), 60},
	};

	std::vector<std::pair<CopaBlock, CopaBlock>> local_results;
	int capacity = 5;
	mpi_prune(comm, blockA, blocksB, local_results, capacity);

	EXPECT_TRUE(local_results.empty()) << "All pairs should be pruned when Z > capacity for all block pairs";
}

// Tests for knapsackcopampi
TEST(KnapsackCopaMPI, SolvesSmallProblem)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	// 4 items: weights [1, 2, 3, 4], values [1, 6, 10, 16]
	// Capacity 7: optimal is items 2,3 (weights 3+4=7, values 10+16=26)
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;

	auto result = knapsackcopampi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 26);
		EXPECT_LE(result->totalWeight, capacity);
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackCopaMPI, EmptyItemsReturnsZero)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2);

	boost::mpi::communicator comm;

	const std::vector<int> weights{};
	const std::vector<int> values{};
	constexpr int capacity = 10;

	auto result = knapsackcopampi(comm, weights, values, capacity);

	// knapsackcopampi returns KnapsackSolution{} (empty solution) for empty input on all ranks
	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 0);
		EXPECT_TRUE(result->items.empty());
	}
}

TEST(KnapsackCopaMPI, SingleItemEachHalf)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2);

	boost::mpi::communicator comm;

	// 2 items: weights [5, 3], values [10, 8]
	// Capacity 7: can take item 0 (weight 5, value 10) or item 1 (weight 3, value 8)
	// Best: item 0 with value 10
	const std::vector<int> weights{5, 3};
	const std::vector<int> values{10, 8};
	constexpr int capacity = 7;

	auto result = knapsackcopampi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 10);
		EXPECT_EQ(result->totalWeight, 5);
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackCopaMPI, FitsAllItems)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2);

	boost::mpi::communicator comm;

	// 4 items with small weights
	const std::vector<int> weights{1, 1, 1, 1};
	const std::vector<int> values{5, 10, 15, 20};
	constexpr int capacity = 10;

	auto result = knapsackcopampi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 50); // All items
		EXPECT_EQ(result->totalWeight, 4);
		EXPECT_EQ(result->items.size(), 4);
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackCopaMPI, NoItemsFit)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2);

	boost::mpi::communicator comm;

	// 4 items all too heavy
	const std::vector<int> weights{10, 10, 10, 10};
	const std::vector<int> values{5, 10, 15, 20};
	constexpr int capacity = 5;

	auto result = knapsackcopampi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 0);
		EXPECT_TRUE(result->items.empty());
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackCopaMPI, MatchesSequentialSolution)
{
	int rank, world_size;
	spdlog::debug("Starting KnapsackCopaMPI MatchesSequentialSolution test on rank ", rank);
	boost::mpi::communicator comm{};
	rank = comm.rank();
	world_size = comm.size();
	std::mt19937 rng(42);
	ASSERT_GE(world_size, 2);


	// Random-ish test case with 6 items
	std::vector<int> weights(10);
	std::vector<int> values(10);
	constexpr int capacity = 12;
	for(int i = 0; i < 100; i++)
	{
		weights.push_back(rng()%100+1);
		values.push_back(rng()%100+1);
	}
	spdlog::debug("Rank {}: Starting test", rank);
	auto mpi_result = knapsackcopampi(comm, weights, values, capacity);
	
	if (rank == 0)
	{
		auto seq_result = knapsackcopasequential(weights, values, capacity);
		ASSERT_TRUE(mpi_result.has_value());
		ASSERT_TRUE(seq_result.has_value());
		EXPECT_EQ(mpi_result->totalValue, seq_result->totalValue);
		EXPECT_EQ(mpi_result->totalWeight, seq_result->totalWeight);
	}
	else
	{
	}
}
