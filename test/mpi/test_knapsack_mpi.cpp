#include <gtest/gtest.h>
#include <boost/mpi.hpp>
#include "Knapsack/knapsackcopa.hpp"

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
