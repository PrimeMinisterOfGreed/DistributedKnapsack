#include "Knapsackmpi/utils.hxx"
#include "options_bag.hpp"
#include <boost/mpi/communicator.hpp>
#include <execinfo.h>
#include <gtest/gtest.h>
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

ProgramOptions options{};

void fillOptionsMap()
{
}

void mpi_errhandler(MPI_Comm *comm, int *error_code, ...)
{
	int rank;
	MPI_Comm_rank(*comm, &rank);
	char error_string[MPI_MAX_ERROR_STRING];
	int result_len;
	MPI_Error_string(*error_code, error_string, &result_len);

	spdlog::error("MPI error on rank {}: {} (error code {})", rank, error_string, *error_code);
	spdlog::error("Backtrace:");

	void *buffer[100];
	int frames = backtrace(buffer, 100);
	backtrace_symbols_fd(buffer, frames, STDERR_FILENO);

	MPI_Abort(*comm, *error_code);
}

void check_mpi()
{
	int flag;
	MPI_Initialized(&flag);
	if (!flag)
	{
		std::cerr << "MPI is not initialized. Please run the tests with mpirun or mpiexec." << std::endl;
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	MPI_Init(&argc, &argv);
	spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%@] %v");
	MPI_Errhandler errhandler;
	MPI_Comm_create_errhandler(reinterpret_cast<MPI_Comm_errhandler_function *>(mpi_errhandler), &errhandler);
	MPI_Comm_set_errhandler(MPI_COMM_WORLD, errhandler);
	
	spdlog::set_level(spdlog::level::trace);
	spdlog::debug("MPI initialized. Running tests...");
	testing::InitGoogleTest(&argc, argv);
	auto res = RUN_ALL_TESTS();
	MPI_Finalize();
	return res;
}

ProgramOptions get_opts()
{
	return options;
}

TEST(MPIInitializationTest, CheckMPIInitialized)
{
	check_mpi();
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	ASSERT_GE(size, 1);
	ASSERT_LE(rank, size);
}