
file(GLOB MPI_TEST_SOURCES  *.cpp *.c)
add_executable(${UT_TARGET_NAME}_mpi ${MPI_TEST_SOURCES} ${SRC_LIST})
target_include_directories(${UT_TARGET_NAME}_mpi PUBLIC ${TGT_INCLUDE_DIRS})
target_link_libraries(${UT_TARGET_NAME}_mpi PUBLIC ${TGT_LIBRARIES} PRIVATE GTest::gtest GTest::gtest_main tasker fmt::fmt MPI::MPI_CXX )