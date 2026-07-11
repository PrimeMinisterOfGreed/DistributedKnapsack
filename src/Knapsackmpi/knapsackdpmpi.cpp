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

static KnapsackSolution maintask(boost::mpi::communicator &comm, const std::vector<int> &weights,
								 const std::vector<int> &values, int capacity)
{
	using namespace boost::mpi;
	int n = static_cast<int>(weights.size());
	int num_workers = comm.size() - 1;
	int chunk_size = (capacity + 1) / num_workers;
	SPDLOG_DEBUG("[mainnode] starting: n={}, capacity={}, workers={}, chunk_size={}", n, capacity, num_workers,
				 chunk_size);

	std::vector<std::vector<uint32_t>> dp(n + 1, std::vector<uint32_t>(capacity + 1, 0));

	for (int i = 1; i <= n; ++i)
	{
		SPDLOG_DEBUG("[mainnode] item {}: broadcasting dp[{}] (size {})", i, i - 1, capacity + 1);
		broadcast(comm, dp[i - 1].data(), capacity+1, 0);
		SPDLOG_DEBUG("[mainnode] item {}: broadcast complete", i);
		int actual_index = 0;
		int active = 0;
		auto generator = [capacity, chunk_size,&actual_index,&active]() -> std::optional<NodeTask> {
			if (actual_index > capacity)
			{
				return std::nullopt;
			}
			int start = actual_index;
			int end = std::min(actual_index + chunk_size - 1, capacity);
			actual_index = end + 1;
			active++;
			return NodeTask{start, end};
		};


		for (int rank = 1; rank <= num_workers; ++rank)
		{
			auto task = generator();
			if (!task.has_value())
			{
				break;
			}
			SPDLOG_DEBUG("[mainnode] item {}: sending TASK[{},{}] to rank {}", i, task->startIndex, task->endIndex, rank);
			comm.send(rank, NODETAG::TASK, task.value());
		}
		SPDLOG_DEBUG("[mainnode] item {}: {} tasks sent initially, {} remaining", i, in_flight, tasks.size());
		while (active > 0)
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
			active--;
			for (int w = resp.startIndex; w <= resp.endIndex; ++w)
			{
				dp[i][w] = data.line[w];
			}
			auto task = generator();
			if (task.has_value())
			{
				SPDLOG_DEBUG("[mainnode] item {}: sending next TASK[{},{}] to rank {}", i, task->startIndex,
							 task->endIndex, status.source());
				comm.send(status.source(), NODETAG::TASK, task.value());
				SPDLOG_DEBUG("[mainnode] item {}: posted irecv for rank {}", i, status.source());
			}
		}
		SPDLOG_DEBUG("[mainnode] item {}: all tasks complete, sending TERMINATE", i);

		for (int rank = 1; rank <= num_workers; ++rank)
		{
			NodeResponse resp{};
			comm.send(rank, NODETAG::TERMINATE, NodeTask{0, 0});
		}
		SPDLOG_DEBUG("[mainnode] item {}: done", i);
		comm.barrier();
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

static void workertask(boost::mpi::communicator &comm, const std::vector<int> &weights, const std::vector<int> &values,
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
		SPDLOG_DEBUG("[workernode] item {}: received row_size = {}", i, row_size);
		broadcast(comm, line.data(),line.size(), 0);

		for(;;)
		{
			NodeTask task{};
			auto status = comm.recv(any_source, any_tag, task);

			if (status.tag() == NODETAG::TERMINATE)
			{
				SPDLOG_DEBUG("[workernode] item {}: received TERMINATE from rank {}", i, status.source());
				break;
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
		comm.barrier();
	}
}

std::optional<KnapsackSolution> knapsackdpmpi(boost::mpi::communicator &comm, const std::vector<int> &weights,
											  const std::vector<int> &values, int capacity)
{
	if (comm.rank() == 0)
	{

		return {maintask(comm, weights, values, capacity)};
	}
	else
	{
		workertask(comm, weights, values, capacity);
		return std::nullopt;
	}
}
