project(DistributedKnapsackGpu C CXX CUDA)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
find_package(CUDAToolkit REQUIRED)
# Global flags (applies to all CUDA targets)
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -allow-unsupported-compiler")
set(GPU_TARGET_NAME distributed_knapsack_gpu)
file(GLOB_RECURSE GPU_CUDA_SRC "src/*.cu")
file(GLOB_RECURSE GPU_SRC "src/*.cpp")

add_library(${GPU_TARGET_NAME} SHARED ${GPU_CUDA_SRC} ${GPU_SRC})

target_include_directories(${GPU_TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CUDAToolkit_INCLUDE_DIRS})
target_link_libraries(${GPU_TARGET_NAME} PUBLIC CUDA::cudart)

add_subdirectory(test)


