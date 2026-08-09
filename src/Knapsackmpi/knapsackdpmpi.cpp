#include "knapsackmpi.hpp"
#include <boost/mpi.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <omp.h>
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

namespace
{
	int computeChunkStart(int chunk_size, int unit)
	{
		return unit * chunk_size;
	}

	int computeChunkEnd(int chunk_size, int unit, int capacity)
	{
		return std::min((unit + 1) * chunk_size - 1, capacity);
	}
} // namespace

static KnapsackSolution maintask(boost::mpi::communicator &comm, const std::vector<int> &weights,
								 const std::vector<int> &values, int capacity)
{
	using namespace boost::mpi;
	int n = static_cast<int>(weights.size());
	int num_workers = comm.size() - 1;
	int num_units = num_workers + 1;

	// Static partition of the capacity dimension across all compute units
	// (unit 0 is the coordinator, units 1..num_workers are the workers).
	int chunk_size = std::max(1, (capacity + 1 + num_units - 1) / num_units);
	int mainStart = computeChunkStart(chunk_size, 0);
	int mainEnd = computeChunkEnd(chunk_size, 0, capacity);

	std::vector<std::vector<uint32_t>> dp(n + 1, std::vector<uint32_t>(capacity + 1, 0));

	// Distribute the (fixed) partition once; no per-item TASK/TERMINATE if the
	// workers compute and return their slice each item.
	for (int rank = 1; rank <= num_workers; ++rank)
	{
		int start = computeChunkStart(chunk_size, rank);
		int end = computeChunkEnd(chunk_size, rank, capacity);
		comm.send(rank, NODETAG::TASK, NodeTask{start, end});
	}

	for (int i = 1; i <= n; ++i)
	{
		SPDLOG_DEBUG("[mainnode] item {}: broadcasting dp[{}] (size {})", i, i - 1, capacity + 1);
		broadcast(comm, dp[i - 1].data(), capacity + 1, 0);
		SPDLOG_DEBUG("[mainnode] item {}: broadcast complete", i);

		// Coordinator computes its own slice with its own threads.
		#pragma omp parallel for schedule(static)
		for (int w = mainStart; w <= mainEnd; ++w)
		{
			if (weights[i - 1] <= w)
				dp[i][w] = std::max(dp[i - 1][w], dp[i - 1][w - weights[i - 1]] + values[i - 1]);
			else
				dp[i][w] = dp[i - 1][w];
		}

		// Gather the worker slices.
		int in_flight = num_workers;
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
	}

	for (int rank = 1; rank <= num_workers; ++rank)
		comm.send(rank, NODETAG::TERMINATE, NodeTask{0, 0});

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

	// Receive the fixed partition for this worker.
	NodeTask task{};
	comm.recv(0, NODETAG::TASK, task);
	int startIndex = task.startIndex;
	int endIndex = task.endIndex;
	int len = endIndex - startIndex + 1;

	for (int i = 1; i <= n; ++i)
	{
		SPDLOG_DEBUG("[workernode] item {}: waiting for broadcast", i);
		broadcast(comm, line.data(), static_cast<int>(line.size()), 0);
		SPDLOG_DEBUG("[workernode] item {}: received broadcast", i);

		if (len > 0)
		{
			std::vector<uint32_t> result(len);
		#pragma omp parallel for schedule(static)
			for (int w = startIndex; w <= endIndex; ++w)
			{
				if (weights[i - 1] <= w)
					result[w - startIndex] = std::max(line[w], line[w - weights[i - 1]] + values[i - 1]);
				else
					result[w - startIndex] = line[w];
			}

			comm.send(0, NODETAG::RESPONSE, NodeResponse{startIndex, endIndex});
			comm.send(0, NODETAG::DATA, NodeResponseData{std::move(result)});
		}
		else
		{
			comm.send(0, NODETAG::RESPONSE, NodeResponse{startIndex, endIndex});
			comm.send(0, NODETAG::DATA, NodeResponseData{std::vector<uint32_t>()});
		}
	}

	comm.recv(0, NODETAG::TERMINATE, task);
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
