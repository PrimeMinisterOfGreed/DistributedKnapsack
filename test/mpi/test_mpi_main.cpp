#include "Knapsackmpi/utils.tpp"
#include "options_bag.hpp"
#include <boost/mpi/collectives/gather.hpp>
#include <boost/mpi/collectives/scatter.hpp>
#include <boost/mpi/collectives/scatterv.hpp>
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
	// print the rank and size to verify that MPI is working
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	std::cout << "MPI Rank: " << rank << ", Size: " << size << std::endl;
}

TEST(MPIInitializationTest, TestScatterGatherPrimitives)
{
	check_mpi();
	using namespace boost::mpi;
	auto comm = boost::mpi::communicator();
	auto rank = comm.rank();
	std::vector<int> data{0, 1, 2, 3, 4, 5, 6, 7, 8};
	auto procedure = compute_scatter_procedure(comm, data);
	std::vector<int> local_data(procedure.sizes[rank]);
	scatterv(comm, data.data(), procedure.sizes, procedure.displacements, local_data.data(), procedure.sizes[rank], 0);
	std::cout << "Rank " << rank << " received data: " << local_data[0] << ", " << local_data[1] << std::endl;
	for (int i = 0; i < procedure.sizes[rank]; i++)
	{
		local_data[i] += 10; // Modify local data to verify gather
	}
	std::vector<int> gathered_data(data.size());
	gatherv(comm, local_data.data(), procedure.sizes[rank], gathered_data.data(), procedure.sizes,
			procedure.displacements, 0);
	if (rank == 0)
	{
		std::cout << "Gathered data: ";
		for (const auto &val : gathered_data)
		{
			std::cout << val << " ";
		}
		std::cout << std::endl;
	}
}

TEST(MPIFeaturesTest, TestMpiMasterWorkerScheme)
{
	check_mpi();
	using namespace boost::mpi;
	auto comm = boost::mpi::communicator();
	if (comm.rank() == 0)
	{
		for (int i = 1; i < comm.size(); i++)
		{
			auto status = comm.recv(any_source, 2);
			spdlog::info("[mainnode] received message from rank {}, tag {}", status.source(), status.tag());
		}
	}
	else
	{
		comm.send(0, 2);
	}
}