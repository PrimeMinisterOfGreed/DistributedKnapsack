cmake_minimum_required(VERSION 3.26)
project(tasker C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Boost REQUIRED COMPONENTS container mpi)

# Collect all source files from src directory
file(GLOB_RECURSE TASKER_SOURCES "src/*.cpp")

# Create executable
add_library(tasker ${TASKER_SOURCES} )

target_link_libraries(tasker PUBLIC Boost::container Boost::mpi MPI::MPI_CXX MPI::MPI_C)
# Include directories
target_include_directories(tasker PRIVATE src )
target_include_directories(tasker PUBLIC include)

