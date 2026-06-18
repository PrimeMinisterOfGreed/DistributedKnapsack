# DistributedKnapsack - Agent Instructions

## Project Overview

C++23 distributed knapsack solver using MPI, OpenMP, and Boost. Uses Conan for dependency management and CMake/Ninja for builds.

## Build Commands

**Configure and build (clang - default):**
```bash
cd build && cmake --preset default-clang && cmake --build .
```

**Configure and build (gcc):**
```bash
cd build && cmake --preset default-gcc && cmake --build .
```

**Run tests:**
```bash
./build/test/distributed_knapsack_tests
```

**Run MPI tests:**
```bash
mpirun -np 4 ./build/test/mpi/distributed_knapsack_mpi_tests
```

## Project Structure

- `src/` - Main application sources
  - `main.cpp` - Entry point with Boost.ProgramOptions
  - `Knapsack/` - Sequential algorithms (DP, COPA)
  - `Knapsackmpi/` - MPI-distributed algorithms
- `libs/tasker/` - Custom task scheduling library (Boost.Container + MPI)
- `test/` - Unit tests with GTest
  - `mpi/` - MPI-specific tests
- `doc/` - Research paper (TeX) and algorithm documentation

## Dependencies (via Conan)

- `fmt`, `spdlog` - Logging/formatting
- `Boost::mpi`, `Boost::program_options`, `Boost::container`
- `OpenMP`, `MPI`, `Eigen3`, `GTest`

## Key Implementation Details

**Algorithms:**
- `knapsackdp` - Dynamic programming solution
- `knapsackcopa` - COPA (meet-in-the-middle) with subset generation
- MPI variants distribute work across processes

**C++23 Modules:**
- Files with `.cxx` extension are CXX modules
- Declared in `CMakeLists.txt` via `FILE_SET CXX_MODULES`

**Command-line options:**
- `--mpi` - Launch with MPI
- `--chunk-size` - Thread chunk size (default: 1000)
- `--weights-file` - Input weights file
- `--save_file` / `--restore` - Persist/restore state

## Testing Conventions

- Tests use GTest framework
- Test files: `test*.cpp` in `test/` and `test/mpi/`
- Test executable links against main target sources (excluding `main.cpp`)
- MPI tests require `mpirun`

## Editor/IDE Setup

- **clangd**: Uses `build/` as compilation database
- **Compiler**: Clang 20.x (`/usr/bin/clang`, `/usr/bin/clang++`)
- **CMake preset**: `default-clang` or `default-gcc`
- **Ignore**: `-fmodule*`, `-Wmodule*`, `-fdeps*` warnings

## Conan Workflow

Dependencies managed via `conanfile.py`. Conan generates:
- `build/conan_toolchain.cmake` - Toolchain file
- `build/CMakeUserPresets.json` - User presets

Run `conan install . --build=missing` before CMake configure if dependencies change.

## OpenCode Agent Config

Custom agents defined in `.opencode/agents/`:
- `docwriter.md` - Documentation agent (doc/ folder)
- `tester.md` - Testing agent (test/ folder)
