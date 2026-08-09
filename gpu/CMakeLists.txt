cmake_minimum_required(VERSION 3.26)

# Must be set before project(): CUDA is a C++ dialect and would otherwise
# inherit the parent's CXX standard (23) during compiler detection.
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

project(distributed_knapsack_gpu C CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CUDAToolkit REQUIRED)

# Collect CUDA kernel (.cu) and host source (.cpp) files
file(GLOB_RECURSE GPU_CU "kernels/*.cu")
file(GLOB_RECURSE GPU_CPP "src/*.cpp")

# Compute the target GPU architecture (bundle for common NVIDIA cards)
set(CMAKE_CUDA_ARCHITECTURES "all-major" CACHE STRING "CUDA target architectures")

# Create a library with CUDA support
add_library(distributed_knapsack_gpu SHARED ${GPU_CU} ${GPU_CPP})

target_include_directories(distributed_knapsack_gpu PUBLIC src kernels ${CUDAToolkit_INCLUDE_DIRS})
target_link_libraries(distributed_knapsack_gpu
        PUBLIC ${CUDAToolkit_LIBRARIES}
    )

target_compile_options(distributed_knapsack_gpu PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-Wall>)


