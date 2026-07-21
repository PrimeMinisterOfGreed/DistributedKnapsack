#include <gtest/gtest.h>
#include <mpi.h>

TEST(MPIPingPong, PingPongLatency)
{
    MPI_Init(NULL, NULL);
    int rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    ASSERT_GE(world_size, 2) << "Ping-pong test requires at least 2 MPI processes";

    int tag = 0;
    int64_t value = 42;
    int64_t rounds = 1000;
    double total_time = 0.0;

    for (int64_t i = 0; i < rounds; ++i)
    {
        double start = MPI_Wtime();

        if (rank == 0)
        {
            MPI_Send(&value, 1, MPI_INT64_T, 1, tag, MPI_COMM_WORLD);
            MPI_Recv(&value, 1, MPI_INT64_T, 1, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        else if (rank == 1)
        {
            MPI_Recv(&value, 1, MPI_INT64_T, 0, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Send(&value, 1, MPI_INT64_T, 0, tag, MPI_COMM_WORLD);
        }

        double end = MPI_Wtime();
        if (rank == 0)
        {
            total_time += (end - start);
        }
    }

    if (rank == 0)
    {
        double avg_latency = (total_time / (2.0 * rounds)) * 1e6;
        std::cout << "Ping-pong completed " << rounds << " rounds." << std::endl;
        std::cout << "Average one-way latency: " << avg_latency << " us" << std::endl;
    }
    MPI_Finalize();
}

TEST(MPIPingPong, PingPongCorrectness)
{
    int rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    ASSERT_GE(world_size, 2);

    int tag = 1;
    int64_t sent = rank + 1;

    if (rank == 0)
    {
        int64_t msg = sent;
        MPI_Send(&msg, 1, MPI_INT64_T, 1, tag, MPI_COMM_WORLD);
        MPI_Recv(&msg, 1, MPI_INT64_T, 1, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        ASSERT_EQ(msg, 2) << "Rank 0 expected value 2 from rank 1, got " << msg;
    }
    else if (rank == 1)
    {
        int64_t msg;
        MPI_Recv(&msg, 1, MPI_INT64_T, 0, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        ASSERT_EQ(msg, 1) << "Rank 1 expected value 1 from rank 0, got " << msg;
        msg = sent;
        MPI_Send(&msg, 1, MPI_INT64_T, 0, tag, MPI_COMM_WORLD);
    }
}
