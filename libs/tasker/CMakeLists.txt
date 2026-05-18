cmake_minimum_required(VERSION 3.30)
project(tasker C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Boost REQUIRED COMPONENTS container)

# Collect all source files from src directory
file(GLOB_RECURSE TASKER_SOURCES "src/*.cpp")

# Create executable
add_library(tasker ${TASKER_SOURCES} )

# Include directories
target_include_directories(tasker PRIVATE src )
target_include_directories(tasker PUBLIC include)

