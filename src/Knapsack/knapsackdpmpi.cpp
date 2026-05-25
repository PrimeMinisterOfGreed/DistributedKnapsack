#include "knapsack.hpp"
#include "options_bag.hpp"
#include <deque>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <vector>

struct NodeTask
{
	int startIndex;			   // Starting index of the items assigned to this process
	int endIndex;			   // Ending index of the items assigned to this process

	template <class Archive>
	void serialize(Archive &ar, const unsigned int /*version*/)
	{
		ar & startIndex;
		ar & endIndex;
	}
};

struct NodeResponse
{
	int startIndex;
	int endIndex;
	std::vector<int> line;

	template <class Archive>
	void serialize(Archive &ar, const unsigned int /*version*/)
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

KnapsackSolution mainnode(boost::mpi::communicator &comm, const std::vector<int> &weights,
						  const std::vector<int> &values, int capacity)
{
	int n = weights.size();
    std::vector<std::vector<uint32_t>> dp{};
 	for (int i = 1; i <= n; ++i)
	{
		boost::mpi::broadcast(comm, dp[0],0);
        auto nodes = comm.size() - 1;
        auto chunk_size = get_opts().chunk_size;
        auto tasks = std::deque<NodeTask>(
            static_cast<size_t>(std::ceil(capacity/chunk_size))
        );
        auto requests = std::vector<boost::mpi::request>(nodes);
        for(int w = 0, k =0 ; w <= capacity; w += chunk_size, ++k) {
            tasks.push_back({w, std::min(w + chunk_size - 1, capacity)});
        }
        for(int rank = 1; rank < comm.size(); ++rank) {
            if(!tasks.empty()) {
                auto task = tasks.front();
                tasks.pop_front();
                comm.send(rank, NODETAG::TASK, task);
                requests.push_back(comm.irecv(rank, NODETAG::RESPONSE, dp[i]));
            }
        }
        while(!requests.empty())
        {
            auto req = boost::mpi::wait_any(requests.begin(), requests.end());
            
            requests.erase(req.second);
        }
	}
    return KnapsackSolution{};
}

void workernode(boost::mpi::communicator &comm, const std::vector<int> &weights, const std::vector<int> &values,
				int capacity)
{
	int n = weights.size();
    std::vector<uint32_t> line{};
	for (int i = 1; i <= n; ++i)
	{
		boost::mpi::broadcast(comm, line,0);
        bool end = false;   
        
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
