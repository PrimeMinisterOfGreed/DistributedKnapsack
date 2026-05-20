
#include "Actors/root.hpp"
#include "Actors/worker.hpp"
#include "options_bag.hpp"
#include <boost/mpi.hpp>
#include <boost/program_options.hpp>
#include <log_engine.hpp>
#include <mpi/mpi.h>
#include <print>
#include <string>

ProgramOptions options{};

ProgramOptions get_opts()
{
	return options;
}


void mpi_routine(int argc, char **argv)
{
	using namespace boost::mpi;
	MPI_Init(&argc, &argv);
	communicator comm{};
	auto rank = comm.rank();
	if (rank == 0)
	{
		std::println("Master PID:{}", getpid());
	}
	else
	{
		std::println("Process {}, pid {}", rank, getpid());
	}

	MPI_Finalize();
}

int main(int argc, char *argv[])
{
	using namespace std;
	using namespace boost::program_options;
	namespace po = boost::program_options;
	po::options_description desc("Allowed options");
	desc.add_options()("gpu", po::bool_switch(&options.use_gpu)->default_value(false),
					   "use_gpu")(
		"chunk-size", po::value<int>(&options.chunk_size)->default_value(1000), "size of chunks to digest for each thread")
		("multithread", po::bool_switch(&options.use_mpi)->default_value(false), "use multithreading")
		("weights-file", po::value<std::string>(&options.weights_file)->default_value("weights.dat"), "file containing weights")(
		"verbosity", po::value<int>(&options.verbosity)->default_value(1), "verbosity of the log file")(
		"restore", po::bool_switch(&options.restore_from_file)->default_value(false), "restore a previous save file")(
		"save_file", po::value<std::string>(&options.savefile)->default_value("store.dat"), "store file for data")(
		"mpi", po::bool_switch(&options.use_mpi)->default_value(false),
		"assume launched with mpi")("help", "produce help message");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	try
	{
		po::notify(vm);
	}
	catch (const po::required_option &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		std::cerr << desc << std::endl;
		return 1;
	}
	if (vm.count("help"))
	{
		std::println("Usage: {} [options]", argv[0]);
    desc.print(std::cout);
		return 0;
	}
	if (options.use_mpi)
		mpi_routine(argc, argv);

	return 0;
}
