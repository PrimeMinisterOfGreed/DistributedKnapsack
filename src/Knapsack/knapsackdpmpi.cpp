#include "knapsack.hpp"
#include "options_bag.hpp"
#include <deque>
#include <Eigen/Dense>
struct NodeTask
{
	int startIndex;			   // Starting index of the items assigned to this process
	int endIndex;			   // Ending index of the items assigned to this process
};

struct NodeResponse
{
    int startIndex;
    int endIndex;
    int line[];
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
    Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic,Eigen::RowMajor> dp{};
    dp.resize(n + 1, capacity + 1);
    dp.fill(0);
	for (int i = 1; i <= n; ++i)
	{
		boost::mpi::broadcast(comm, dp.row(i-1).data(),capacity  ,0);
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
}

void workernode(boost::mpi::communicator &comm, const std::vector<int> &weights, const std::vector<int> &values,
				int capacity)
{
	int n = weights.size();
    Eigen::Matrix<int, 2, Eigen::Dynamic,Eigen::RowMajor> row{};
    row.resize(2, capacity + 1);
	for (int i = 1; i <= n; ++i)
	{
		boost::mpi::broadcast(comm, row.row(0).data(), capacity + 1, 0);
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
