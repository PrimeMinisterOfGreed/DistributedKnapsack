#include "knapsackmpi.hpp"
#include <boost/mpi.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
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

	std::vector<std::vector<uint32_t>> dp(n + 1, std::vector<uint32_t>(capacity + 1, 0));

	for (int i = 1; i <= n; ++i)
	{
		SPDLOG_DEBUG("[mainnode] item {}: broadcasting dp[{}] (size {})", i, i - 1, capacity + 1);
		broadcast(comm, dp[i - 1].data(), capacity + 1, 0);
		SPDLOG_DEBUG("[mainnode] item {}: broadcast complete", i);

		int chunk_size = std::max(1, (capacity + num_workers) / num_workers);
		int in_flight = 0;

		for (int rank = 1; rank <= num_workers; ++rank)
		{
			int start = (rank - 1) * chunk_size;
			int end = std::min(rank * chunk_size - 1, capacity);
			if (start > end)
				break;
			SPDLOG_DEBUG("[mainnode] item {}: sending TASK[{},{}] to rank {}", i, start, end, rank);
			comm.send(rank, NODETAG::TASK, NodeTask{start, end});
			++in_flight;
		}

		while (in_flight > 0)
		{
			NodeResponse resp{};
			NodeResponseData data;
			auto status = comm.recv(any_source, NODETAG::RESPONSE, resp);
			comm.recv(status.source(), NODETAG::DATA, data);
			--in_flight;

			SPDLOG_DEBUG("[mainnode] item {}: received chunk [{},{}] from rank {}", i,
					resp.startIndex, resp.endIndex, status.source());

			for (int w = resp.startIndex; w <= resp.endIndex; ++w)
				dp[i][w] = data.line[w - resp.startIndex];
		}

		for (int rank = 1; rank <= num_workers; ++rank)
			comm.send(rank, NODETAG::TERMINATE, NodeTask{0, 0});

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
	std::vector<uint32_t> line(capacity + 1, 0);

	for (int i = 1; i <= n; ++i)
	{
		SPDLOG_DEBUG("[workernode] item {}: waiting for broadcast", i);
		broadcast(comm, line.data(), static_cast<int>(line.size()), 0);
		SPDLOG_DEBUG("[workernode] item {}: received broadcast", i);

		while (true)
		{
			NodeTask task{};
			auto status = comm.recv(any_source, any_tag, task);

			if (status.tag() == NODETAG::TERMINATE)
			{
				SPDLOG_DEBUG("[workernode] item {}: received TERMINATE", i);
				break;
			}

			int len = task.endIndex - task.startIndex + 1;
			std::vector<uint32_t> result(len);
			for (int w = task.startIndex; w <= task.endIndex; ++w)
			{
				if (weights[i - 1] <= w)
					result[w - task.startIndex] = std::max(line[w], line[w - weights[i - 1]] + values[i - 1]);
				else
					result[w - task.startIndex] = line[w];
			}

			comm.send(status.source(), NODETAG::RESPONSE, NodeResponse{task.startIndex, task.endIndex});
			comm.send(status.source(), NODETAG::DATA, NodeResponseData{std::move(result)});
		}
		comm.barrier();
	}
}

std::optional<KnapsackSolution> knapsackdpmpi(boost::mpi::communicator &comm, const std::vector<int> &weights,
											  const std::vector<int> &values, int capacity)
{
	if (comm.rank() == 0)
		return {maintask(comm, weights, values, capacity)};
	else
	{
		workertask(comm, weights, values, capacity);
		return std::nullopt;
	}
}
