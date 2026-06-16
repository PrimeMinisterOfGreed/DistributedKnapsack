#include <gtest/gtest.h>
#include <boost/mpi.hpp>
#include "Knapsack/knapsack.hpp"
#include "Knapsackmpi/knapsackmpi.hpp"
#include "options_bag.hpp"

extern ProgramOptions options;

TEST(KnapsackDPMPI, SolvesSmallProblem)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	options.chunk_size = 2;

	boost::mpi::communicator comm;

	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;

	auto result = knapsackdpmpi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 26);
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackDPMPI, EmptyItemsReturnsZero)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	options.chunk_size = 2;

	boost::mpi::communicator comm;

	const std::vector<int> weights{};
	const std::vector<int> values{};
	constexpr int capacity = 10;

	auto result = knapsackdpmpi(comm, weights, values, capacity);

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

TEST(KnapsackDPMPI, ZeroCapacityReturnsZero)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	options.chunk_size = 2;

	boost::mpi::communicator comm;

	const std::vector<int> weights{1, 2, 3};
	const std::vector<int> values{10, 20, 30};
	constexpr int capacity = 0;

	auto result = knapsackdpmpi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 0);
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackDPMPI, AllItemsFit)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	options.chunk_size = 2;

	boost::mpi::communicator comm;

	const std::vector<int> weights{1, 2, 3};
	const std::vector<int> values{10, 15, 40};
	constexpr int capacity = 10;

	auto result = knapsackdpmpi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 65);
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}
