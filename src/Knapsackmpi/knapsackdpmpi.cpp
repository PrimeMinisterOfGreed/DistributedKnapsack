#include "knapsackmpi.hpp"
#include "options_bag.hpp"
#include <boost/mpi.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <deque>
#include <fmt/core.h>
#include <vector>

struct NodeTask
{
	int startIndex;
	int endIndex;

	template <class Archive> void serialize(Archive &ar, const unsigned int)
	{
		ar & startIndex;
		ar & endIndex;
	}
};

struct NodeResponse
{
	int startIndex;
	int endIndex;

	template <class Archive> void serialize(Archive &ar, const unsigned int)
	{
		ar & startIndex;
		ar & endIndex;
	}
};

struct NodeResponseData
{
	std::vector<uint32_t> line;

	template <class Archive> void serialize(Archive &ar, const unsigned int)
	{
		ar & line;
	}
};

enum NODETAG
{
	TASK = 1,
	RESPONSE = 2,
	DATA = 3,
	TERMINATE = 4
};

static KnapsackSolution mainnode(boost::mpi::communicator &comm, const std::vector<int> &weights,
								 const std::vector<int> &values, int capacity)
{
	using namespace boost::mpi;
	int n = static_cast<int>(weights.size());
	int num_workers = comm.size() - 1;
	int chunk_size = get_opts().chunk_size;

	fmt::println("[mainnode] starting: n={}, capacity={}, workers={}, chunk_size={}", n, capacity, num_workers,
				 chunk_size);

	std::vector<std::vector<uint32_t>> dp(n + 1, std::vector<uint32_t>(capacity + 1, 0));

	for (int i = 1; i <= n; ++i)
	{
		fmt::println("[mainnode] item {}: broadcasting dp[{}] (size {})", i, i - 1, capacity + 1);
		broadcast(comm, dp[i - 1], 0);
		fmt::println("[mainnode] item {}: broadcast complete", i);

		std::deque<NodeTask> tasks;
		for (int w = 0; w <= capacity; w += chunk_size)
		{
			tasks.push_back({w, std::min(w + chunk_size - 1, capacity)});
		}
		fmt::println("[mainnode] item {}: {} tasks created", i, tasks.size());

		int num_tasks = static_cast<int>(tasks.size());
		int in_flight = 0;

		for (int rank = 1; rank <= num_workers && !tasks.empty(); ++rank)
		{
			auto task = tasks.front();
			tasks.pop_front();
			fmt::println("[mainnode] item {}: sending TASK[{},{}] to rank {}", i, task.startIndex, task.endIndex, rank);
			comm.send(rank, NODETAG::TASK, task);
			in_flight++;
		}
		fmt::println("[mainnode] item {}: {} tasks sent initially, {} remaining", i, in_flight, tasks.size());
		int completed = 0;
		while (completed < num_tasks)
		{
			NodeResponse resp;
			NodeResponseData data;
			fmt::println("[mainnode] item {}: waiting for RESPONSE (any_source)", i);
			auto status = comm.recv(any_source, NODETAG::RESPONSE, resp);
			fmt::println("[mainnode] item {}: got RESPONSE from rank {}, start={}, end={}", i, status.source(),
						 resp.startIndex, resp.endIndex);

			fmt::println("[mainnode] item {}: receiving DATA from rank {}", i, status.source());
			comm.recv(status.source(), NODETAG::DATA, data);
			fmt::println("[mainnode] item {}: received DATA, line.size={}", i, data.line.size());

			for (int w = resp.startIndex; w <= resp.endIndex; ++w)
			{
				dp[i][w] = data.line[w];
			}
			completed++;

			if (!tasks.empty())
			{
				auto task = tasks.front();
				tasks.pop_front();
				fmt::println("[mainnode] item {}: sending next TASK[{},{}] to rank {}", i, task.startIndex,
							 task.endIndex, status.source());
				comm.send(status.source(), NODETAG::TASK, task);
				fmt::println("[mainnode] item {}: posted irecv for rank {}", i, status.source());
			}
		}
		fmt::println("[mainnode] item {}: all tasks complete, sending TERMINATE", i);

		for (int rank = 1; rank <= num_workers; ++rank)
		{
			comm.send(rank, NODETAG::TERMINATE);
			comm.recv(rank, NODETAG::RESPONSE); // wait for worker to acknowledge termination
		}
		fmt::println("[mainnode] item {}: done", i);
	}

	fmt::println("[mainnode] DP complete, optimal value = {}", dp[n][capacity]);

	KnapsackSolution solution;
	solution.totalValue = dp[n][capacity];
	solution.totalWeight = 0;
	for (int i = n, w = capacity; i > 0 && dp[i][w] > 0; --i)
	{
		if (dp[i][w] != dp[i - 1][w])
		{
			solution.items.push_back(i - 1);
			w -= weights[i - 1];
			solution.totalWeight += weights[i - 1];
		}
	}
	return solution;
}

static void workernode(boost::mpi::communicator &comm, const std::vector<int> &weights, const std::vector<int> &values,
					   int capacity)
{
	using namespace boost::mpi;
	int n = static_cast<int>(weights.size());
	std::vector<uint32_t> line(capacity + 1, 0);

	for (int i = 1; i <= n; ++i)
	{
		broadcast(comm, line, 0);

		bool end = false;
		while (!end)
		{
			NodeTask task{};
			auto status = comm.recv(any_source, any_tag, task);

			if (status.tag() == NODETAG::TERMINATE)
			{
				end = true;
				comm.send(status.source(), NODETAG::RESPONSE, NodeResponse{0, 0}); // acknowledge termination
				continue;
			}

			for (int w = task.startIndex; w <= task.endIndex; ++w)
			{
				if (weights[i - 1] <= w)
				{
					line[w] = std::max(line[w], line[w - weights[i - 1]] + values[i - 1]);
				}
			}

			comm.send(status.source(), NODETAG::RESPONSE, NodeResponse{task.startIndex, task.endIndex});
			comm.send(status.source(), NODETAG::DATA, NodeResponseData{line});
		}
	}
}

std::optional<KnapsackSolution> knapsackdpmpi(boost::mpi::communicator &comm, const std::vector<int> &weights,
											  const std::vector<int> &values, int capacity)
{
	if (comm.rank() == 0)
	{
		return {mainnode(comm, weights, values, capacity)};
	}
	else
	{
		workernode(comm, weights, values, capacity);
		return std::nullopt;
	}
}
