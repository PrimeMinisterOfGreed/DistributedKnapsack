#pragma once
#include <boost/mpi.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <concepts>
#include <ranges>
using communicator = boost::mpi::communicator;

namespace collectives
{

namespace details
{
struct NodeTask
{
	int start;
	int end;

	void serialize(auto &ar, unsigned int version)
	{
		ar & start;
		ar & end;
	}
};

template<typename T>
struct NodeResponse
{
    int start;
    int end;
    std::vector<T> value;
    void serialize(auto &ar, unsigned int version)
    {
        ar & start;
        ar & end;
        ar & value;
    }
};

enum class tags : int
{
    task = 1,
    response = 2,
    terminate = 3
};


} // namespace details

void apply(communicator &comm, const std::ranges::input_range auto &array,
		   const std::invocable<decltype(array[0])> auto &func,
		   std::ranges::output_range<decltype(func(array[0]))> auto &out)
{
	int world = comm.size();
	int rank = comm.rank();
	int n = static_cast<int>(array.size());
	int blockSize = n / world;
	int start = rank * blockSize;
	int end = (rank == world - 1) ? n : start + blockSize;

	for (int i = start; i < end; ++i)
	{
		out[i] = func(array[i]);
	}

	// Gather results from all processes
	boost::mpi::all_gather(comm, out.data() + start, end - start, out.data());
}

void apply(communicator &comm, const std::ranges::input_range auto &array,
		   const std::invocable<decltype(array[0])> auto &func,
		   std::ranges::output_range<decltype(func(array[0]))> auto &out, int root, int chunk_size)
{
    using value_type = decltype(func(array[0]));
	using namespace boost::mpi;
    using namespace details;
	int n = static_cast<int>(array.size());
	int world = comm.size();
	int rank = comm.rank();
	if (rank == root)
	{
		// Handle the single-process case: execute everything locally.
		if (world == 1)
		{
			for (int i = 0; i < n; ++i)
				out[i] = func(array[i]);
			return;
		}

		int next_start = 0;	      // next unassigned element index
		int active_count = 0;     // number of workers that received a task

		// ---- initial distribution: one chunk per worker ----------------
		for (int i = 1; i < world; ++i)
		{
			if (next_start >= n)
				break; // no more work → remaining workers stay idle
			NodeTask task;
			task.start = next_start;
			task.end   = std::min(next_start + chunk_size, n);
			comm.send<NodeTask>(i, static_cast<int>(tags::task), task);
			next_start = task.end;
			++active_count;
		}

		// ---- dynamic load-balancing loop -------------------------------
		while (next_start < n)
		{
			NodeResponse<value_type> response;
			comm.recv(MPI_ANY_SOURCE, static_cast<int>(tags::response), response);
			std::copy(response.value.begin(), response.value.end(),
			          out.begin() + response.start);

			NodeTask task;
			task.start = next_start;
			task.end   = std::min(next_start + chunk_size, n);
			comm.send<NodeTask>(response.start, static_cast<int>(tags::task), task);
			next_start = task.end;
		}

		// ---- collect final responses from all active workers -----------
		for (int i = 0; i < active_count; ++i)
		{
			NodeResponse<value_type> response;
			comm.recv(MPI_ANY_SOURCE, static_cast<int>(tags::response), response);
			std::copy(response.value.begin(), response.value.end(),
			          out.begin() + response.start);
			printf("Master received final response from worker %d for range [%d, %d)\n",
			          response.start, response.end);
		}

		// ---- terminate all workers (including idle ones) ---------------
		for (int i = 1; i < world; ++i)
			comm.send(i, static_cast<int>(tags::terminate));
	}
	else
	{
        while (true)
        {
            auto resp = comm.probe(any_source, any_tag);
            if (resp.tag() == static_cast<int>(tags::terminate))
            {
                comm.recv(resp.source(), resp.tag());
                return;
            }
            NodeTask task;
            comm.recv(resp.source(), static_cast<int>(tags::task), task);
            NodeResponse<value_type> response;
            response.start = task.start;
            response.end = task.end;
            response.value.reserve(task.end - task.start);
            for (int i = task.start; i < task.end; ++i)
            {
                response.value.push_back(func(array[i]));
            }
            comm.send<NodeResponse<value_type>>(root, static_cast<int>(tags::response), response);
        }
	}
}
} // namespace collectives