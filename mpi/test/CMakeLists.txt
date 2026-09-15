find_package(GTest REQUIRED CONFIG)

# COPA MPI test is temporarily excluded from this target.
file(GLOB MPI_TEST_SOURCES  *.cpp *.c)
list(FILTER MPI_TEST_SOURCES EXCLUDE REGEX ".*copa.*")

add_executable(${UT_TARGET_NAME}_mpi ${MPI_TEST_SOURCES} ${SRC_LIST})
target_sources(${UT_TARGET_NAME}_mpi PUBLIC
    FILE_SET CXX_MODULES
    TYPE CXX_MODULES
    BASE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
    FILES ${KP_MODULES}
)
target_include_directories(${UT_TARGET_NAME}_mpi PUBLIC ${TGT_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/mpi ${PROJECT_SOURCE_DIR}/mpi/Knapsackmpi)
target_link_libraries(${UT_TARGET_NAME}_mpi PUBLIC ${TGT_LIBRARIES} distributed_knapsack_mpi PRIVATE GTest::gtest GTest::gtest_main fmt::fmt MPI::MPI_CXX Boost::mpi Boost::serialization)
