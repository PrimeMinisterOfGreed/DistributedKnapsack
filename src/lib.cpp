#include "Knapsack/knapsack.hpp"
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
}