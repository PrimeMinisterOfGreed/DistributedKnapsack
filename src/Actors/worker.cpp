#include "worker.hpp"
#include "log_engine.hpp"
#include "options_bag.hpp"
#include <future>
using namespace boost::mpi;
using namespace std;

// Mpi Node

void operator<<(std::string &val, char *buffer)
{
	for (int i = 0; i < val.size(); i++)
	{
		val[i] = buffer[i];
	}
}

struct WorkerNode
{
  private:
  public:
	virtual void recv_phase() = 0;
	virtual void compute_phase() = 0;
	static void send_result(std::string str, communicator &comm)
	{
		char buffer[RESULT_SIZE]{};
		memcpy(buffer, str.c_str(), str.size());
		comm.send(0, RESULT_TAG, buffer, sizeof(buffer));
	}
	virtual ~WorkerNode() = default;
};
