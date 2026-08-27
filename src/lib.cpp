#include "Knapsack/knapsack.hpp"
#include "time.hpp"
#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <spdlog/spdlog.h>

namespace py = pybind11;

void hello()
{
	std::cout << "Hello, World!" << std::endl;
}

struct KnapsackArguments
{
	std::vector<int> weights;
	std::vector<int> values;
	int capacity;

	KnapsackArguments(const py::list &weights, const py::list &values, int capacity)
	{
		int n = py::len(weights);
		this->weights = std::vector<int>(n);
		this->values = std::vector<int>(n);
		this->capacity = capacity;
		for (int i = 0; i < n; ++i)
		{
			this->weights[i] = py::cast<int>(weights[i]);
			this->values[i] = py::cast<int>(values[i]);
		}
	}
};

KnapsackSolution knapsackdpsolver(KnapsackArguments args)
{
	return knapsackdp(args.weights, args.values, args.capacity);
}

KnapsackSolution knapsackcopasolver(KnapsackArguments args)
{
	auto res = knapsackcopa(args.weights, args.values, args.capacity);
	if (res.has_value())
		return res.value();
	return KnapsackSolution();
}

KnapsackSolution knapsackcopasequentialsolver(KnapsackArguments args)
{
	auto res = knapsackcopasequential(args.weights, args.values, args.capacity);
	if (res.has_value())
		return res.value();
	return KnapsackSolution();
}

void disable_logging()
{
	spdlog::set_level(spdlog::level::err);
}

PYBIND11_MODULE(libdistributed_knapsack, m)
{
	m.doc() = "Distributed Knapsack solvers";
	m.def("hello", &hello);
	py::class_<KnapsackSolution>(m, "KnapsackSolution")
		.def(py::init<>())
		.def_readwrite("items", &KnapsackSolution::items)
		.def_readwrite("totalValue", &KnapsackSolution::totalValue)
		.def_readwrite("totalWeight", &KnapsackSolution::totalWeight);
	py::class_<KnapsackArguments>(m, "KnapsackArguments")
		.def(py::init<py::list, py::list, int>())
		.def_readwrite("weights", &KnapsackArguments::weights)
		.def_readwrite("values", &KnapsackArguments::values)
		.def_readwrite("capacity", &KnapsackArguments::capacity);
	m.def("knapsackdp", &knapsackdpsolver, py::arg("args"));
	m.def("knapsackcopa", &knapsackcopasolver, py::arg("args"));
	m.def("knapsackcopasequential", &knapsackcopasequentialsolver, py::arg("args"));
	m.def(
		"knapsackdpdag",
		[](KnapsackArguments args, int item_block, int cap_block) {
			return knapsackdpdag(args.weights, args.values, args.capacity, item_block, cap_block);
		},
		py::arg("args"), py::arg("item_block") = 10, py::arg("cap_block") = 0);
	m.def("disable_logging", &disable_logging);
	py::class_<TimeSection>(m, "TimeSection")
		.def_readonly("min", &TimeSection::min)
		.def_readonly("max", &TimeSection::max)
		.def_readonly("mean", &TimeSection::mean)
		.def_readonly("variance", &TimeSection::variance)
		.def_readonly("count", &TimeSection::count);
	m.def(
		"get_section", [](const std::string &name) { return TimeSectionRegister::instance().get_section(name); },
		py::arg("name"));
	m.def("get_all_sections", []() { return TimeSectionRegister::instance().get_all_sections(); });
}
