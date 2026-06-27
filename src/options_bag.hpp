#pragma once
#include <cstring>
#include <string>

struct ProgramOptions {
  int verbosity;
  std::string weights_file;
  bool use_mpi;
  int threads;
  size_t random_seed;
  size_t samples;
};

ProgramOptions get_opts();