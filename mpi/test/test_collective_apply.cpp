#include <gtest/gtest.h>
#include <boost/mpi.hpp>
#include "collective.hpp"
#include <vector>
#include <string>
#include <span>

// =============================================================================
// Tests for collectives::apply (first overload — simple all_gather-based apply)
//
// This overload uses a static block distribution:
//   blockSize = n / world_size
//   start    = rank * blockSize
//   end      = (rank == last) ? n : start + blockSize
//
// Results are gathered via boost::mpi::all_gather, which requires every
// process to contribute the *same* number of elements.  Therefore all test
// inputs use a size that is a multiple of the world size.
//
// =============================================================================

class CollectiveApplyTest : public ::testing::Test
{
protected:
    boost::mpi::communicator comm;
    int rank;
    int world_size;

    CollectiveApplyTest()
        : comm()
        , rank(comm.rank())
        , world_size(comm.size())
    {
    }
};

// -----------------------------------------------------------------------------
// Identity mapping: f(x) = x
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyTest, SimpleApplyIdentity)
{
    int n = world_size * 3;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = i;

    collectives::apply(comm, input, [](int idx, int x) { return x; }, output);

    for (int i = 0; i < n; ++i)
        EXPECT_EQ(output[i], i) << "Mismatch at index " << i << " on rank " << rank;
}

// -----------------------------------------------------------------------------
// Square mapping: f(x) = x * x
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyTest, SimpleApplySquare)
{
    int n = world_size * 3;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = i;

    collectives::apply(comm, input, [](int idx, int x) { return x * x; }, output);

    for (int i = 0; i < n; ++i)
        EXPECT_EQ(output[i], i * i) << "Mismatch at index " << i << " on rank " << rank;
}

// -----------------------------------------------------------------------------
// Double mapping: f(x) = x * 2
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyTest, SimpleApplyDouble)
{
    int n = world_size * 4;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = i + 1;

    collectives::apply(comm, input, [](int idx, int x) { return x * 2; }, output);

    for (int i = 0; i < n; ++i)
        EXPECT_EQ(output[i], (i + 1) * 2) << "Mismatch at index " << i << " on rank " << rank;
}

// -----------------------------------------------------------------------------
// String conversion: from int to std::string
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyTest, SimpleApplyStringConversion)
{
    int n = world_size * 2;
    std::vector<int> input(n);
    std::vector<std::string> output(n);
    for (int i = 0; i < n; ++i)
        input[i] = i * 10;

    collectives::apply(comm, input, [](int idx, int x) { return std::to_string(x); }, output);

    for (int i = 0; i < n; ++i)
        EXPECT_EQ(output[i], std::to_string(i * 10))
            << "Mismatch at index " << i << " on rank " << rank;
}

// -----------------------------------------------------------------------------
// Modulo operation: f(x) = x % 3
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyTest, SimpleApplyModulo)
{
    int n = world_size * 5;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = i;

    collectives::apply(comm, input, [](int idx, int x) { return x % 3; }, output);

    for (int i = 0; i < n; ++i)
        EXPECT_EQ(output[i], i % 3) << "Mismatch at index " << i << " on rank " << rank;
}

// -----------------------------------------------------------------------------
// Empty input — should not crash or hang
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyTest, SimpleApplyEmptyInput)
{
    std::vector<int> input;
    std::vector<int> output;

    collectives::apply(comm, input, [](int idx, int x) { return x; }, output);

    EXPECT_TRUE(output.empty());
}

// -----------------------------------------------------------------------------
// Single element per rank
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyTest, SimpleApplySingleElementPerRank)
{
    int n = world_size;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = 100 + i;

    collectives::apply(comm, input, [](int idx, int x) { return x + 1; }, output);

    for (int i = 0; i < n; ++i)
        EXPECT_EQ(output[i], 101 + i) << "Mismatch at index " << i << " on rank " << rank;
}

// -----------------------------------------------------------------------------
// Const reference input (std::span) satisfies std::ranges::input_range
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyTest, SimpleApplySpanInput)
{
    std::vector<int> input(world_size * 2);
    std::vector<int> output(world_size * 2, -1);
    for (int i = 0; i < static_cast<int>(input.size()); ++i)
        input[i] = i;

    std::span<const int> input_span(input);
    collectives::apply(comm, input_span, [](int idx, int x) { return x * 3; }, output);

    for (int i = 0; i < static_cast<int>(output.size()); ++i)
        EXPECT_EQ(output[i], i * 3) << "Mismatch at index " << i << " on rank " << rank;
}

// -----------------------------------------------------------------------------
// Lambda with state (capture by value)
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyTest, SimpleApplyLambdaWithState)
{
    int n = world_size * 2;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = i;

    int offset = 42;
    collectives::apply(comm, input, [offset](int idx, int x) { return x + offset; }, output);

    for (int i = 0; i < n; ++i)
        EXPECT_EQ(output[i], i + offset) << "Mismatch at index " << i << " on rank " << rank;
}

// -----------------------------------------------------------------------------
// Negative values in input
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyTest, SimpleApplyNegativeValues)
{
    int n = world_size * 3;
    std::vector<int> input(n);
    std::vector<int> output(n, 0);
    for (int i = 0; i < n; ++i)
        input[i] = -(i + 1);

    collectives::apply(comm, input, [](int idx, int x) { return x * x; }, output);

    for (int i = 0; i < n; ++i)
    {
        int expected = (i + 1) * (i + 1);
        EXPECT_EQ(output[i], expected) << "Mismatch at index " << i << " on rank " << rank;
    }
}

// -----------------------------------------------------------------------------
// All elements are the same value
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyTest, SimpleApplyConstantInput)
{
    int n = world_size * 3;
    std::vector<int> input(n, 7);
    std::vector<int> output(n, -1);

    collectives::apply(comm, input, [](int idx, int x) { return x * 10; }, output);

    for (int i = 0; i < n; ++i)
        EXPECT_EQ(output[i], 70) << "Mismatch at index " << i << " on rank " << rank;
}

// =============================================================================
// Tests for collectives::apply (second overload — master-worker dynamic
// load balancing with root process and chunk_size).
//
// The root distributes chunks of work to workers and uses a dynamic
// work-stealing loop.  Workers process chunks and send results back
// until they receive a terminate message.
// =============================================================================

class CollectiveApplyMasterWorkerTest : public ::testing::Test
{
protected:
    boost::mpi::communicator comm;
    int rank;
    int world_size;

    CollectiveApplyMasterWorkerTest()
        : comm()
        , rank(comm.rank())
        , world_size(comm.size())
    {
    }
};

// -----------------------------------------------------------------------------
// Identity mapping with large chunk (single round per worker)
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyMasterWorkerTest, IdentityLargeChunk)
{
    int n = world_size * 3;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = i;

    collectives::apply(comm, input, [](int idx, int x) { return x; }, output, 0, n);

    if (rank == 0)
    {
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(output[i], i) << "Mismatch at index " << i << " on master";
    }
}

// -----------------------------------------------------------------------------
// Square mapping with medium chunk size
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyMasterWorkerTest, SquareMediumChunk)
{
    int n = world_size * 16;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = i;

    collectives::apply(comm, input, [](int idx, int x) { return x * x; }, output, 0, world_size);

    if (rank == 0)
    {
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(output[i], i * i) << "Mismatch at index " << i << " on master";
    }
}

// -----------------------------------------------------------------------------
// Very small chunks (chunk_size = 1) — each element is a separate task
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyMasterWorkerTest, SingleElementChunks)
{
    int n = world_size * 4;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = i;

    collectives::apply(comm, input, [](int idx, int x) { return x + 10; }, output, 0, 1);

    if (rank == 0)
    {
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(output[i], i + 10) << "Mismatch at index " << i << " on master";
    }
}

// -----------------------------------------------------------------------------
// Chunk size = world_size — causes multiple rounds per worker
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyMasterWorkerTest, MultipleRounds)
{
    int n = world_size * 5;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = i;

    collectives::apply(comm, input, [](int idx, int x) { return x * 3; }, output, 0, world_size);

    if (rank == 0)
    {
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(output[i], i * 3) << "Mismatch at index " << i << " on master";
    }
}

// -----------------------------------------------------------------------------
// String conversion
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyMasterWorkerTest, StringConversion)
{
    int n = world_size * 2;
    std::vector<int> input(n);
    std::vector<std::string> output(n);
    for (int i = 0; i < n; ++i)
        input[i] = i;

    collectives::apply(comm, input, [](int idx, int x) { return std::to_string(x * 2); }, output, 0, 2);

    if (rank == 0)
    {
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(output[i], std::to_string(i * 2)) << "Mismatch at index " << i << " on master";
    }
}

// -----------------------------------------------------------------------------
// Empty input — should not crash or hang
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyMasterWorkerTest, EmptyInput)
{
    std::vector<int> input;
    std::vector<int> output;

    collectives::apply(comm, input, [](int idx, int x) { return x; }, output, 0, 1);

    EXPECT_TRUE(output.empty());
}

// -----------------------------------------------------------------------------
// Modulo operation
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyMasterWorkerTest, ModuloOperation)
{
    int n = world_size * 4;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = i;

    collectives::apply(comm, input, [](int idx, int x) { return x % 7; }, output, 0, world_size);

    if (rank == 0)
    {
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(output[i], i % 7) << "Mismatch at index " << i << " on master";
    }
}

// -----------------------------------------------------------------------------
// Constant input — all elements the same
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyMasterWorkerTest, ConstantInput)
{
    int n = world_size * 3;
    std::vector<int> input(n, 42);
    std::vector<int> output(n, -1);

    collectives::apply(comm, input, [](int idx, int x) { return x / 6; }, output, 0, world_size);

    if (rank == 0)
    {
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(output[i], 7) << "Mismatch at index " << i << " on master";
    }
}

// -----------------------------------------------------------------------------
// Lambda with captured state
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyMasterWorkerTest, LambdaWithState)
{
    int n = world_size * 3;
    std::vector<int> input(n);
    std::vector<int> output(n, -1);
    for (int i = 0; i < n; ++i)
        input[i] = i;

    int factor = 5;
    collectives::apply(comm, input, [factor](int idx, int x) { return x * factor; }, output, 0, world_size);

    if (rank == 0)
    {
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(output[i], i * factor) << "Mismatch at index " << i << " on master";
    }
}

// -----------------------------------------------------------------------------
// Negative values
// -----------------------------------------------------------------------------
TEST_F(CollectiveApplyMasterWorkerTest, NegativeValues)
{
    int n = world_size * 3;
    std::vector<int> input(n);
    std::vector<int> output(n, 0);
    for (int i = 0; i < n; ++i)
        input[i] = -(i + 1);

    collectives::apply(comm, input, [](int idx, int x) { return x * x; }, output, 0, world_size);

    if (rank == 0)
    {
        for (int i = 0; i < n; ++i)
        {
            int expected = (i + 1) * (i + 1);
            EXPECT_EQ(output[i], expected) << "Mismatch at index " << i << " on master";
        }
    }
}
