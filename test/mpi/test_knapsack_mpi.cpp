#include <gtest/gtest.h>
#include <boost/mpi.hpp>
#include "Knapsack/knapsackcopa.hpp"
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
