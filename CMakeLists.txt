cmake_minimum_required(VERSION 3.12)
project(DistributedKnapsack)

find_package(OpenMP REQUIRED)
find_package(MPI REQUIRED)
find_package(Boost REQUIRED COMPONENTS mpi program_options)
# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all source files from src directory
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create executable
add_executable(distributed_knapsack ${SOURCES} )

# Include directories
target_include_directories(distributed_knapsack PRIVATE src include)

# Link OpenMP libraries
target_link_libraries(distributed_knapsack PRIVATE OpenMP::OpenMP_CXX MPI::MPI_CXX Boost::mpi Boost::program_options fmt)