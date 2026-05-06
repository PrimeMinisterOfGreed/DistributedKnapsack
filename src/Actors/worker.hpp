#pragma once
#include "mpiconfig.hpp"
void worker_routine(boost::mpi::communicator&comm);
void single_node_routine();