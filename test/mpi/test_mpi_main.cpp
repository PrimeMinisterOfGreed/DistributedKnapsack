#include <gtest/gtest.h>
#include <mpi.h>
#include "options_bag.hpp"


ProgramOptions options{};

void fillOptionsMap()
{

}

void check_mpi()
{
    int flag;
    MPI_Initialized(&flag);
    if (!flag) {
        std::cerr << "MPI is not initialized. Please run the tests with mpirun or mpiexec." << std::endl;
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    MPI_Finalize();
    return res;
}


ProgramOptions get_opts()
{
	return options;
}


TEST(MPIInitializationTest, CheckMPIInitialized) {
    check_mpi();
    // print the rank and size to verify that MPI is working
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    std::cout << "MPI Rank: " << rank << ", Size: " << size << std::endl;
}

TEST(MPIInitializationTest, TestScatterGatherPrimitives) {
    check_mpi();
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Each process will send its rank to the root process
    int send_data = rank;
    std::vector<int> recv_data(size);
    
    MPI_Gather(&send_data, 1, MPI_INT, recv_data.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::cout << "Received data from all processes: ";
        for (int i = 0; i < size; ++i) {
            std::cout << recv_data[i] << " ";
        }
        std::cout << std::endl;
    }
}