#pragma once
#include <cstring>
#include <string>

struct ProgramOptions {
  bool use_gpu;
  int chunk_size;
  int verbosity;
  std::string savefile;
  std::string weights_file;
  bool restore_from_file;
  bool use_mpi;
};

ProgramOptions get_opts();