#include "Knapsack/knapsack.hpp"
#include "Knapsackmpi/knapsackmpi.hpp"
#include <boost/python.hpp>
#include <iostream>

void hello()
{
	std::cout << "Hello, World!" << std::endl;
}

struct KnapsackArguments
{
	std::vector<int> weights;
	std::vector<int> values;
	int capacity;

	KnapsackArguments(const boost::python::list &weights, const boost::python::list &values, int capacity)
	{
		int n = boost::python::len(weights);
		this->weights = std::vector<int>(n);
		this->values = std::vector<int>(n);
		this->capacity = capacity;
		for (int i = 0; i < n; ++i)
		{
			this->weights[i] = boost::python::extract<int>(weights[i]);
			this->values[i] = boost::python::extract<int>(values[i]);
		}
	}
};

KnapsackSolution knapsackdpsolver(KnapsackArguments args, int numThreads)
{
	return knapsackdp(args.weights, args.values, args.capacity, numThreads);
}

KnapsackSolution knapsackcopasolver(KnapsackArguments args, int numThreads)
{
	auto res = knapsackcopa(args.weights, args.values, args.capacity, numThreads);
	if (res.has_value())
		return res.value();
	else
	{
		std::cerr << "Error: No solution found by knapsackcopa" << std::endl;
		return KnapsackSolution();
	}
}

KnapsackSolution knapsackcopasequentialsolver(KnapsackArguments args)
{
	auto res = knapsackcopasequential(args.weights, args.values, args.capacity);
	if (res.has_value())
		return res.value();
	else
	{
		std::cerr << "Error: No solution found by knapsackcopasequential" << std::endl;
		return KnapsackSolution();
	}
}

KnapsackSolution knapsackcopampisolver(KnapsackArguments args, int numThreads)
{
	MPI_Init(NULL, NULL);
	auto comm = boost::mpi::communicator();	
	auto res = knapsackcopampi(comm,args.weights, args.values, args.capacity);
	MPI_Finalize();
	if (res.has_value())
		return res.value();
	else
	{
		std::cerr << "Error: No solution found by knapsackcopampi" << std::endl;
		return KnapsackSolution();
	}
}

BOOST_PYTHON_MODULE(libdistributed_knapsack)
{
	using namespace boost::python;
	def("hello", hello);
	class_<KnapsackSolution>("KnapsackSolution", init<>())
		.def_readwrite("items", &KnapsackSolution::items)
		.def_readwrite("totalValue", &KnapsackSolution::totalValue)
		.def_readwrite("totalWeight", &KnapsackSolution::totalWeight);
	class_<KnapsackArguments>("KnapsackArguments", init<list, list, int>())
		.def_readwrite("weights", &KnapsackArguments::weights)
		.def_readwrite("values", &KnapsackArguments::values)
		.def_readwrite("capacity", &KnapsackArguments::capacity);
	def("knapsackdp", knapsackdpsolver,
		(boost::python::arg("args"), boost::python::arg("numThreads") = 1));
	def("knapsackcopa", knapsackcopasolver,
		(boost::python::arg("args"), boost::python::arg("numThreads") = 1));
	def("knapsackcopasequential", knapsackcopasequentialsolver,
		(boost::python::arg("args")));
	def("knapsackcopampi", knapsackcopampisolver,
		(boost::python::arg("args"), boost::python::arg("numThreads") = 1));
}