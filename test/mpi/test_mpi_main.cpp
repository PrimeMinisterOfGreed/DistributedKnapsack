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