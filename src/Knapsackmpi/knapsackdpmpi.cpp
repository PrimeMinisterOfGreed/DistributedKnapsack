#include "knapsackmpi.hpp"
#include "options_bag.hpp"
#include <boost/mpi.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <deque>
#include <spdlog/spdlog.h>
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
	int chunk_size = (capacity+1) / num_workers;
	SPDLOG_DEBUG("[mainnode] starting: n={}, capacity={}, workers={}, chunk_size={}", n, capacity, num_workers,
				 chunk_size);

	std::vector<std::vector<uint32_t>> dp(n + 1, std::vector<uint32_t>(capacity + 1, 0));

	for (int i = 1; i <= n; ++i)
	{
		SPDLOG_DEBUG("[mainnode] item {}: broadcasting dp[{}] (size {})", i, i - 1, capacity + 1);
		int row_size = static_cast<int>(dp[i - 1].size());
		broadcast(comm, row_size, 0);
		broadcast(comm, dp[i - 1].data(), row_size, 0);
		SPDLOG_DEBUG("[mainnode] item {}: broadcast complete", i);

		std::deque<NodeTask> tasks;
		for (int w = 0; w <= capacity; w += chunk_size)
		{
			tasks.push_back({w, std::min(w + chunk_size - 1, capacity)});
		}
		SPDLOG_DEBUG("[mainnode] item {}: {} tasks created", i, tasks.size());

		int num_tasks = static_cast<int>(tasks.size());
		int in_flight = 0;

		for (int rank = 1; rank <= num_workers && !tasks.empty(); ++rank)
		{
			auto task = tasks.front();
			tasks.pop_front();
			SPDLOG_DEBUG("[mainnode] item {}: sending TASK[{},{}] to rank {}", i, task.startIndex, task.endIndex, rank);
			comm.send(rank, NODETAG::TASK, task);
			in_flight++;
		}
		SPDLOG_DEBUG("[mainnode] item {}: {} tasks sent initially, {} remaining", i, in_flight, tasks.size());
		int completed = 0;
		while (completed < num_tasks)
		{
			NodeResponse resp{};
			NodeResponseData data;
			SPDLOG_DEBUG("[mainnode] item {}: waiting for RESPONSE (any_source)", i);
			auto status = comm.recv(any_source, NODETAG::RESPONSE, resp);
			SPDLOG_DEBUG("[mainnode] item {}: got RESPONSE from rank {}, start={}, end={}", i, status.source(),
						 resp.startIndex, resp.endIndex);

			SPDLOG_DEBUG("[mainnode] item {}: receiving DATA from rank {}", i, status.source());
			comm.recv(status.source(), NODETAG::DATA, data);
			SPDLOG_DEBUG("[mainnode] item {}: received DATA, line.size={}", i, data.line.size());

			for (int w = resp.startIndex; w <= resp.endIndex; ++w)
			{
				dp[i][w] = data.line[w];
			}
			completed++;

			if (!tasks.empty())
			{
				auto task = tasks.front();
				tasks.pop_front();
				SPDLOG_DEBUG("[mainnode] item {}: sending next TASK[{},{}] to rank {}", i, task.startIndex,
							 task.endIndex, status.source());
				comm.send(status.source(), NODETAG::TASK, task);
				SPDLOG_DEBUG("[mainnode] item {}: posted irecv for rank {}", i, status.source());
			}
		}
		SPDLOG_DEBUG("[mainnode] item {}: all tasks complete, sending TERMINATE", i);

		for (int rank = 1; rank <= num_workers; ++rank)
		{
			NodeResponse resp{};
			comm.send(rank, NODETAG::TERMINATE, NodeTask{0, 0});
			comm.recv(rank, NODETAG::RESPONSE,resp); // wait for worker to acknowledge termination
		}
		SPDLOG_DEBUG("[mainnode] item {}: done", i);
	}
	SPDLOG_DEBUG("[mainnode] DP complete, optimal value = {}", dp[n][capacity]);
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
	std::vector<uint32_t> line;

	for (int i = 1; i <= n; ++i)
	{
		line.clear();
		line.resize(capacity + 1, 0);
		SPDLOG_DEBUG("[workernode] item {}: waiting for broadcast of dp[{}]", i, i - 1);
		int row_size;
		broadcast(comm, row_size, 0);
		SPDLOG_DEBUG("[workernode] item {}: received row_size = {}", i, row_size);
		if (static_cast<int>(line.size()) != row_size)
			line.resize(row_size);
		broadcast(comm, line.data(), row_size, 0);

		bool end = false;
		while (!end)
		{
			NodeTask task{};
			auto status = comm.recv(any_source, any_tag, task);

			if (status.tag() == NODETAG::TERMINATE)
			{
				end = true;
				SPDLOG_DEBUG("[workernode] item {}: received TERMINATE from rank {}", i, status.source());
				comm.send(status.source(), NODETAG::RESPONSE, NodeResponse{0, 0}); // acknowledge termination
				continue;
			}

			auto prev = line;
			for (int w = task.startIndex; w <= task.endIndex; ++w)
			{
				if (weights[i - 1] <= w)
				{
					line[w] = std::max(prev[w], prev[w - weights[i - 1]] + values[i - 1]);
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
