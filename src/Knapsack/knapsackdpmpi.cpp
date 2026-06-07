#include "knapsack.hpp"
#include "options_bag.hpp"
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <deque>
#include <vector>

struct NodeTask
{
	int startIndex; // Starting index of the items assigned to this process
	int endIndex;	// Ending index of the items assigned to this process

	template <class Archive> void serialize(Archive &ar, const unsigned int /*version*/)
	{
		ar & startIndex;
		ar & endIndex;
	}
};

struct NodeResponse
{
	int startIndex;
	int endIndex;
	std::vector<uint32_t> line;

	template <class Archive> void serialize(Archive &ar, const unsigned int /*version*/)
	{
		ar & startIndex;
		ar & endIndex;
		ar & line;
	}
};

enum NODETAG
{
	TASK = 1,
	RESPONSE = 2,
	TERMINATE = 3
};

static KnapsackSolution mainnode(boost::mpi::communicator &comm, const std::vector<int> &weights,
						  const std::vector<int> &values, int capacity)
{
	int n = weights.size();
	std::vector<std::vector<uint32_t>> dp{};
	for (int i = 1; i <= n; ++i)
	{
		boost::mpi::broadcast(comm, dp[0], 0);
		auto nodes = comm.size() - 1;
		auto chunk_size = get_opts().chunk_size;
		auto tasks = std::deque<NodeTask>(static_cast<size_t>(std::ceil(capacity / chunk_size)));
		auto requests = std::vector<boost::mpi::request>(nodes);
		for (int w = 0, k = 0; w <= capacity; w += chunk_size, ++k)
		{
			tasks.push_back({w, std::min(w + chunk_size - 1, capacity)});
		}
		for (int rank = 1; rank < comm.size(); ++rank)
		{
			if (!tasks.empty())
			{
				auto task = tasks.front();
				tasks.pop_front();
				comm.send(rank, NODETAG::TASK, task);
				requests.push_back(comm.irecv(rank, NODETAG::RESPONSE, dp[i]));
			}
		}
		while (!requests.empty())
		{
			auto req = boost::mpi::wait_any(requests.begin(), requests.end());

			requests.erase(req.second);
		}
	}
	for (int i = 1; i < comm.size(); ++i)
	{
		comm.send(i, NODETAG::TERMINATE);
	}
	auto solution = KnapsackSolution{};
	solution.totalValue = dp[n][capacity];
	solution.totalWeight = 0;
	for (int i = n, w = capacity; i > 0 && w > 0; --i)
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
	int n = weights.size();
	std::vector<uint32_t> line{};
	for (int i = 1; i <= n; ++i)
	{
		boost::mpi::broadcast(comm, line, 0);
		bool end = false;
		while (!end)
		{
			NodeTask task{};
			boost::mpi::status status;
			comm.recv(boost::mpi::any_source, TASK, task);
			if (status.tag() == NODETAG::TERMINATE)
			{
				end = true;
				continue;
			}
			for (int w = task.startIndex; w <= task.endIndex; ++w)
			{
				if (weights[i - 1] <= w)
				{
					line[w] = std::max(line[w], line[w - weights[i - 1]] + values[i - 1]);
				}
			}
			NodeResponse response{task.startIndex, task.endIndex, line};
			comm.send(status.source(), NODETAG::RESPONSE, response);
		}
	}
}

std::optional<KnapsackSolution> knapsackdpmpi(boost::mpi::communicator &comm, const std::vector<int> &weights,
											  const std::vector<int> &values, int capacity)
{
	int rank = comm.rank();
	int size = comm.size();

	if (rank == 0)
	{
		return {mainnode(comm, weights, values, capacity)};
	}
	else
	{
		workernode(comm, weights, values, capacity);
		return std::nullopt;
	}
}
