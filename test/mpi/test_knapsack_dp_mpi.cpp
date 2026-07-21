#include <gtest/gtest.h>
#include <boost/mpi.hpp>
#include "Knapsack/knapsack.hpp"
#include "Knapsackmpi/knapsackmpi.hpp"
#include "options_bag.hpp"
#include <spdlog/spdlog.h>
#include <numeric>
#include <chrono>
extern ProgramOptions options;

TEST(KnapsackDPMPI, SolvesSmallProblem)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";


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
	spdlog::set_level(spdlog::level::debug);

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

TEST(KnapsackDPMPI, VerifiesItemSelected)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	// 4 items: weights [1, 2, 3, 4], values [1, 6, 10, 16]
	// Capacity 7: optimal is items 1,2,3 (0-indexed: weights 2+3+4=9 > 7, so items 1,3 with weights 2+4=6, values 6+16=22)
	// Actually: items 2,3 (0-indexed) with weights 3+4=7, values 10+16=26
	const std::vector<int> weights{1, 2, 3, 4};
	const std::vector<int> values{1, 6, 10, 16};
	constexpr int capacity = 7;

	auto result = knapsackdpmpi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 26);
		EXPECT_EQ(result->totalWeight, 7);
		EXPECT_EQ(result->items.size(), 2);
		
		// Verify the selected items are valid
		int computedWeight = 0;
		int computedValue = 0;
		for (int itemIdx : result->items)
		{
			EXPECT_GE(itemIdx, 0);
			EXPECT_LT(itemIdx, static_cast<int>(weights.size()));
			computedWeight += weights[itemIdx];
			computedValue += values[itemIdx];
		}
		EXPECT_EQ(computedWeight, result->totalWeight);
		EXPECT_EQ(computedValue, result->totalValue);
		EXPECT_LE(computedWeight, capacity);
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackDPMPI, MatchesSequentialDP)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	// Medium-sized problem
	const std::vector<int> weights{2, 3, 4, 5, 6, 7, 8};
	const std::vector<int> values{3, 4, 5, 8, 9, 10, 11};
	constexpr int capacity = 15;

	auto mpi_result = knapsackdpmpi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(mpi_result.has_value());
		
		// Compare with sequential DP
		auto seq_result = knapsackdp(weights, values, capacity, 1);
		
		EXPECT_EQ(mpi_result->totalValue, seq_result.totalValue);
		EXPECT_EQ(mpi_result->totalWeight, seq_result.totalWeight);
		
		// Both solutions should have the same value (though item selection may vary)
		EXPECT_LE(mpi_result->totalWeight, capacity);
	}
	else
	{
		EXPECT_FALSE(mpi_result.has_value());
	}
}

TEST(KnapsackDPMPI, LargerProblemWithManyItems)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	// 20 items with varying weights and values
	const std::vector<int> weights{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
	const std::vector<int> values{2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40};
	constexpr int capacity = 50;

	auto result = knapsackdpmpi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_LE(result->totalWeight, capacity);
		
		// Verify solution validity
		int computedWeight = 0;
		int computedValue = 0;
		for (int itemIdx : result->items)
		{
			EXPECT_GE(itemIdx, 0);
			EXPECT_LT(itemIdx, static_cast<int>(weights.size()));
			computedWeight += weights[itemIdx];
			computedValue += values[itemIdx];
		}
		EXPECT_EQ(computedWeight, result->totalWeight);
		EXPECT_EQ(computedValue, result->totalValue);
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackDPMPI, SingleItemProblem)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	// Single item that fits
	const std::vector<int> weights{5};
	const std::vector<int> values{10};
	constexpr int capacity = 10;

	auto result = knapsackdpmpi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 10);
		EXPECT_EQ(result->totalWeight, 5);
		ASSERT_EQ(result->items.size(), 1);
		EXPECT_EQ(result->items[0], 0);
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackDPMPI, SingleItemTooHeavy)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	// Single item that doesn't fit
	const std::vector<int> weights{15};
	const std::vector<int> values{100};
	constexpr int capacity = 10;

	auto result = knapsackdpmpi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 0);
		EXPECT_EQ(result->totalWeight, 0);
		EXPECT_TRUE(result->items.empty());
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackDPMPI, DuplicateValues)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	// Items with duplicate values but different weights
	const std::vector<int> weights{2, 3, 4, 5};
	const std::vector<int> values{10, 10, 10, 10};
	constexpr int capacity = 7;

	auto result = knapsackdpmpi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_LE(result->totalWeight, capacity);
		
		// With capacity 7 and items of value 10 each:
		// Best is to take items with smallest weights: 2+3=5 (value 20) or 2+4=6 (value 20) or 3+4=7 (value 20)
		EXPECT_EQ(result->totalValue, 20);
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackDPMPI, AllWeightsZero)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	// All items have zero weight (should all fit)
	const std::vector<int> weights{0, 0, 0, 0};
	const std::vector<int> values{5, 10, 15, 20};
	constexpr int capacity = 10;

	auto result = knapsackdpmpi(comm, weights, values, capacity);

	if (rank == 0)
	{
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(result->totalValue, 50); // All items
		EXPECT_EQ(result->totalWeight, 0);
	}
	else
	{
		EXPECT_FALSE(result.has_value());
	}
}

TEST(KnapsackDPMPI, Benchmark100Elements)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	// Benchmark with 100 items
	constexpr int numItems = 100;
	constexpr int capacity = 5000;
	
	std::vector<int> weights(numItems);
	std::vector<int> values(numItems);
	
	// Generate test data: weights 1-50, values proportional with some variation
	for (int i = 0; i < numItems; ++i)
	{
		weights[i] = (i % 50) + 1;  // Weights from 1 to 50
		values[i] = ((i % 30) + 1) * 2;  // Values from 2 to 60
	}

	if (rank == 0)
	{
		spdlog::info("Starting MPI benchmark with {} items, capacity {}", numItems, capacity);
	}
	
	auto mpi_start = std::chrono::high_resolution_clock::now();
	auto mpi_result = knapsackdpmpi(comm, weights, values, capacity);
	auto mpi_end = std::chrono::high_resolution_clock::now();
	
	auto mpi_duration = std::chrono::duration_cast<std::chrono::milliseconds>(mpi_end - mpi_start);

	if (rank == 0)
	{
		ASSERT_TRUE(mpi_result.has_value());
		EXPECT_LE(mpi_result->totalWeight, capacity);
		
		// Verify solution validity
		int computedWeight = 0;
		int computedValue = 0;
		for (int itemIdx : mpi_result->items)
		{
			EXPECT_GE(itemIdx, 0);
			EXPECT_LT(itemIdx, numItems);
			computedWeight += weights[itemIdx];
			computedValue += values[itemIdx];
		}
		EXPECT_EQ(computedWeight, mpi_result->totalWeight);
		EXPECT_EQ(computedValue, mpi_result->totalValue);
		
		// Compare with sequential DP for correctness
		auto seq_start = std::chrono::high_resolution_clock::now();
		auto seq_result = knapsackdp(weights, values, capacity, 1);
		auto seq_end = std::chrono::high_resolution_clock::now();
		auto seq_duration = std::chrono::duration_cast<std::chrono::milliseconds>(seq_end - seq_start);
		
		EXPECT_EQ(mpi_result->totalValue, seq_result.totalValue) 
			<< "MPI and sequential results differ!";
		EXPECT_EQ(mpi_result->totalWeight, seq_result.totalWeight);
		
		spdlog::info("Benchmark Results (100 items, capacity {}):", capacity);
		spdlog::info("  MPI time: {} ms ({} processes)", mpi_duration.count(), world_size);
		spdlog::info("  Sequential time: {} ms", seq_duration.count());
		spdlog::info("  Optimal value: {}, Weight: {}, Items selected: {}", 
			mpi_result->totalValue, mpi_result->totalWeight, mpi_result->items.size());
	}
	else
	{
		EXPECT_FALSE(mpi_result.has_value());
	}
}

TEST(KnapsackDPMPI, Benchmark100ElementsWithValidation)
{
	int rank, world_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	ASSERT_GE(world_size, 2) << "This test requires at least 2 MPI processes";

	boost::mpi::communicator comm;

	// Benchmark with 100 items - more challenging case
	constexpr int numItems = 100;
	constexpr int capacity = 750;
	
	std::vector<int> weights(numItems);
	std::vector<int> values(numItems);
	
	// Generate varied test data
	for (int i = 0; i < numItems; ++i)
	{
		weights[i] = (i * 7) % 100 + 1;  // Weights from 1 to 100
		values[i] = (i * 13) % 150 + 5;  // Values from 5 to 155
	}

	auto mpi_start = std::chrono::high_resolution_clock::now();
	auto mpi_result = knapsackdpmpi(comm, weights, values, capacity);
	auto mpi_end = std::chrono::high_resolution_clock::now();
	
	auto mpi_duration = std::chrono::duration_cast<std::chrono::milliseconds>(mpi_end - mpi_start);

	if (rank == 0)
	{
		ASSERT_TRUE(mpi_result.has_value());
		EXPECT_LE(mpi_result->totalWeight, capacity);
		
		// Comprehensive validation
		int computedWeight = 0;
		int computedValue = 0;
		std::vector<bool> itemSelected(numItems, false);
		
		for (int itemIdx : mpi_result->items)
		{
			EXPECT_GE(itemIdx, 0);
			EXPECT_LT(itemIdx, numItems);
			EXPECT_FALSE(itemSelected[itemIdx]) << "Item " << itemIdx << " selected twice!";
			itemSelected[itemIdx] = true;
			computedWeight += weights[itemIdx];
			computedValue += values[itemIdx];
		}
		
		EXPECT_EQ(computedWeight, mpi_result->totalWeight) << "Weight mismatch";
		EXPECT_EQ(computedValue, mpi_result->totalValue) << "Value mismatch";
		EXPECT_LE(computedWeight, capacity) << "Over capacity!";
		
		// Compare with sequential solution
		auto seq_result = knapsackdp(weights, values, capacity, 1);
		
		EXPECT_EQ(mpi_result->totalValue, seq_result.totalValue) 
			<< "MPI value " << mpi_result->totalValue 
			<< " != Sequential value " << seq_result.totalValue;
		EXPECT_EQ(mpi_result->totalWeight, seq_result.totalWeight);
		
		spdlog::info("Benchmark 100 elements (challenging):");
		spdlog::info("  Processes: {}, Capacity: {}", world_size, capacity);
		spdlog::info("  MPI duration: {} ms", mpi_duration.count());
		spdlog::info("  Solution: value={}, weight={}, items={}", 
			mpi_result->totalValue, mpi_result->totalWeight, mpi_result->items.size());
	}
	else
	{
		EXPECT_FALSE(mpi_result.has_value());
	}
}
