cmake_minimum_required(VERSION 3.26)
project(DistributedKnapsack C CXX)


find_package(OpenMP REQUIRED)
find_package(MPI REQUIRED)
find_package(fmt REQUIRED)
find_package(Python3 3.12 COMPONENTS Development Interpreter REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS mpi container)
find_package(Eigen3 REQUIRED)
find_package(spdlog REQUIRED)
# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Collect all source files from src directory
file(GLOB_RECURSE KP_SRC "src/*.cpp")
# Collect module-like sources and expose them as CXX modules
file(GLOB_RECURSE KP_MODULES "src/*.cxx")
# Create executable
add_library(distributed_knapsack SHARED ${KP_SRC})
# Add module files as a CXX_MODULES file set (CMake 3.26+)
target_sources(distributed_knapsack PRIVATE
    FILE_SET CXX_MODULES
    TYPE CXX_MODULES
    FILES ${KP_MODULES}
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(distributed_knapsack PUBLIC ENABLE_TIMERS=1)
endif()

# Include directories
target_include_directories(distributed_knapsack PUBLIC src include ${OpenMP_CXX_INCLUDE_DIRS} ${OpenMP_C_INCLUDE_DIRS} ${MPI_CXX_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${fmt_INCLUDE_DIRS} ${EIGEN3_INCLUDE_DIR} ${spdlog_INCLUDE_DIRS})
target_compile_options(distributed_knapsack PUBLIC ${OpenMP_CXX_FLAGS} ${OpenMP_C_FLAGS} ${MPI_CXX_COMPILE_OPTIONS} ${Boost_COMPILE_OPTIONS} -fmodules)
# Link OpenMP libraries
target_link_libraries(distributed_knapsack PUBLIC OpenMP::OpenMP_CXX OpenMP::OpenMP_C MPI::MPI_CXX Boost::mpi fmt::fmt spdlog::spdlog pybind11::pybind11 Eigen3::Eigen)

add_subdirectory(test)
