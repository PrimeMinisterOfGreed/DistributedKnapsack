find_package(GTest REQUIRED CONFIG)
include(GoogleTest)
enable_testing()

set(GPU_TEST_TARGET_NAME distributed_knapsack_gpu_tests)
file(GLOB GPU_TEST_SOURCES *.cpp)

add_executable(${GPU_TEST_TARGET_NAME} ${GPU_TEST_SOURCES})
target_include_directories(${GPU_TEST_TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(${GPU_TEST_TARGET_NAME} PRIVATE distributed_knapsack_gpu GTest::gtest GTest::gtest_main)

gtest_discover_tests(${GPU_TEST_TARGET_NAME})
