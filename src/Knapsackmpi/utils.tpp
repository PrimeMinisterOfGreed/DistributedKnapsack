#pragma once
#include "Knapsack/utils.tpp"
#include <boost/mpi.hpp>
#include <fmt/core.h>

using communicator = boost::mpi::communicator;

struct ScatterProcedure
{
	std::vector<int> displacements;
	std::vector<int> sizes;
};

struct CoRankProcedure
{
	std::vector<int> output_sizes;
	std::vector<int> output_displacements;
	std::vector<int> a_sizes;
	std::vector<int> a_displacements;
	std::vector<int> b_sizes;
	std::vector<int> b_displacements;
};

template <typename T> ScatterProcedure compute_scatter_procedure(communicator &comm, const std::vector<T> &data)
{
	ScatterProcedure proc;
	int total_size = static_cast<int>(data.size());
	int num_procs = comm.size();
	auto local_size = total_size / num_procs;
	auto remainder = total_size % num_procs;

	proc.sizes.resize(num_procs);
	proc.displacements.resize(num_procs);

	for (int i = 0; i < num_procs; ++i)
	{
		proc.sizes[i] = local_size + (i < remainder ? 1 : 0);
		proc.displacements[i] = local_size * i + std::min(i, remainder);
	}
	return proc;
}

template <typename T>
CoRankProcedure compute_corank_procedure(communicator &comm, const std::vector<T> &A, const std::vector<T> &B)
{
	int total_size = static_cast<int>(A.size() + B.size());
	int num_procs = comm.size();

	CoRankProcedure proc;
	proc.output_sizes.resize(num_procs);
	proc.output_displacements.resize(num_procs);
	proc.a_sizes.resize(num_procs);
	proc.a_displacements.resize(num_procs);
	proc.b_sizes.resize(num_procs);
	proc.b_displacements.resize(num_procs);

	for (int p = 0; p < num_procs; ++p)
	{
		int start = p * total_size / num_procs;
		int end = (p + 1) * total_size / num_procs;

		auto [a_start, b_start] = co_rank(A, B, start);
		auto [a_end, b_end] = co_rank(A, B, end);

		proc.a_sizes[p] = a_end - a_start;
		proc.a_displacements[p] = a_start;
		proc.b_sizes[p] = b_end - b_start;
		proc.b_displacements[p] = b_start;

		proc.output_sizes[p] = (a_end - a_start) + (b_end - b_start);
		proc.output_displacements[p] = start;
	}

	return proc;
}

/**
 * @brief Merge two sorted ranges in parallel using MPI. Each process computes its assigned portion of the merge and
 * then gathers the results.
 *
 * @param comm
 * @param A
 * @param B
 * @param output
 * @warning It's not optimal since it gathers the entire merged output on all processes, but it's a simple way to
 * implement parallel merging with MPI.
 */
constexpr void mpi_parallel_merge(communicator &comm, const std::vector<CopaSubset> &A,
								  const std::vector<CopaSubset> &B, std::vector<CopaSubset> &output)
{
	using namespace boost::mpi;

	auto proc = compute_corank_procedure(comm, A, B);

	int rank = comm.rank();
	int a_start = proc.a_displacements[rank];
	int a_end = a_start + proc.a_sizes[rank];
	int b_start = proc.b_displacements[rank];
	int b_end = b_start + proc.b_sizes[rank];

	comm.barrier();

	std::vector<CopaSubset> local_result(proc.output_sizes[rank]);
	std::merge(A.begin() + a_start, A.begin() + a_end, B.begin() + b_start, B.begin() + b_end, local_result.begin());

	comm.barrier();

	gatherv(comm, local_result.data(), proc.output_sizes[rank], output.data(), proc.output_sizes,
			proc.output_displacements, 0);

	broadcast(comm, output, 0);
}

std::vector<CopaSubset> mpi_generate_copa_subsets(communicator &comm, const std::ranges::input_range auto &items)
{
	using namespace boost::mpi;
	std::vector<CopaSubset> subsets{CopaSubset{}};
	for (int item_idx = 0; const auto &item : items)
	{
		auto shifted = subsets;
		auto [w, v] = item;
		// For small subset sizes, do the shifting locally to avoid communication overhead
		if (subsets.size() < 100 * comm.size())
		{
			for (int i = 0; i < static_cast<int>(shifted.size()); i++)
			{
				shifted[i].addItem(item_idx, w, v);
			}
		}

		else
		{
			// For larger subset sizes, distribute the shifting across MPI processes
			int local_size = static_cast<int>(shifted.size()) / comm.size();
			int remainder = static_cast<int>(shifted.size()) % comm.size();
			auto procedure = compute_scatter_procedure(comm, shifted);
			std::vector<CopaSubset> local_shifted{static_cast<size_t>(local_size), CopaSubset{}};
			scatterv(comm, shifted.data(), procedure.sizes, procedure.displacements, local_shifted.data(),
					 procedure.sizes[comm.rank()], 0);

			for (int i = 0; i < static_cast<int>(local_shifted.size()); i++)
			{
				local_shifted[i].addItem(item_idx, w, v);
			}

			// Gather the shifted subsets from all processes
			std::vector<CopaSubset> global_shifted{shifted.size()};
			gatherv(comm, local_shifted.data(), procedure.sizes[comm.rank()], global_shifted.data(), procedure.sizes,
					procedure.displacements, 0);
			shifted = std::move(global_shifted);
		}
		item_idx++;

		std::vector<CopaSubset> newSubsets;
		newSubsets.reserve(subsets.size() + shifted.size());
		if (subsets.size() < 100 * comm.size())
		{
			parallel_merge(subsets, shifted, newSubsets, 1);
		}
		else
		{
			if (comm.rank() != 0)
			{
				subsets.clear();
				shifted.clear();
			}
			broadcast(comm, subsets, 0);
			broadcast(comm, shifted, 0);
			mpi_parallel_merge(comm, subsets, shifted, newSubsets);
		}
		subsets = std::move(newSubsets);
	}
	return subsets;
}

#pragma region Optimizers



#pragma endregion