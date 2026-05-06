#include "root.hpp"
#include "log_engine.hpp"
#include "options_bag.hpp"
#include <mpi.h>
using namespace boost::mpi;
using namespace std;

struct GeneratorNode
{
	virtual void send(int dest) = 0;
	virtual ~GeneratorNode() = default;
};

struct MpiAwaiter
{
	communicator &_comm;
	vector<request> _pending{};
	vector<int> _toprocess{};
	bool _result_available = false;
	char _result[RESULT_SIZE]{};

	MpiAwaiter(communicator &comm) : _comm(comm)
	{
		_pending.resize(comm.size());
		for (int i = 0; i < comm.size(); i++)
		{
			if (i != comm.rank())
				_toprocess.push_back(i);
		}
		_pending.push_back(comm.irecv(any_source, RESULT_TAG, _result, RESULT_SIZE));
	}

	bool result_received()
	{
		return _result_available;
	}

	std::string result() const
	{
		return _result;
	}
	void scan()
	{
		auto itr = _pending.begin();
		while (itr != _pending.end())
		{
			if (itr->test().has_value())
			{
				if (itr->test().value().tag() == RESULT_TAG)
				{
					_result_available = true;
				}
				else
				{
					_toprocess.push_back(itr->test()->source());
				}
				_pending.erase(itr);
			}
			itr++;
		}
	}

	int get_next()
	{
		if (_toprocess.size() > 0)
		{
			auto res = _toprocess.front();
			_toprocess.erase(_toprocess.begin());
			_pending.push_back(_comm.irecv(res, AVAILABLE_RESP_TAG));
			return res;
		}
		return -1;
	}

	void wait_any_request()
	{
		auto res = boost::mpi::wait_any(_pending.begin(), _pending.end());
		_toprocess.push_back(res.first.source());
		if (res.first.tag() == RESULT_TAG)
		{
			_result_available = true;
		}
		_pending.erase(res.second);
	}

	~MpiAwaiter()
	{
		for (int i = 0; i < _comm.size(); i++)
			if (i != _comm.rank())
			{
				_comm.send(i, STOP_TAG);
				_comm.send(i, UNLOCK_TAG);
			}
	}
};
