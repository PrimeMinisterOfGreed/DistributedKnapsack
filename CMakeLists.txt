cmake_minimum_required(VERSION 3.26)
project(DistributedKnapsack C CXX)

add_subdirectory(libs/tasker)

find_package(OpenMP REQUIRED)
find_package(MPI REQUIRED)
find_package(fmt REQUIRED)
find_package(Boost REQUIRED COMPONENTS mpi program_options container)
find_package(Eigen3 REQUIRED)
# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all source files from src directory
file(GLOB_RECURSE KP_SRC "src/*.cpp")
# Create executable
add_executable(distributed_knapsack ${KP_SRC} )

# Include directories
target_include_directories(distributed_knapsack PUBLIC src include ${OpenMP_CXX_INCLUDE_DIRS} ${OpenMP_C_INCLUDE_DIRS} ${MPI_CXX_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${fmt_INCLUDE_DIRS} ${EIGEN3_INCLUDE_DIR})

# Link OpenMP libraries
target_link_libraries(distributed_knapsack PUBLIC OpenMP::OpenMP_CXX OpenMP::OpenMP_C  MPI::MPI_CXX Boost::mpi Boost::program_options fmt::fmt Eigen3::Eigen )

add_subdirectory(test)