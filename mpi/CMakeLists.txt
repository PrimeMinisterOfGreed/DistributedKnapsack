set(MPI_TARGET_NAME distributed_knapsack_mpi)
file(GLOB_RECURSE MPI_IMPL_SOURCES "Knapsackmpi/*.cpp")

add_library(${MPI_TARGET_NAME} STATIC ${MPI_IMPL_SOURCES})
set_target_properties(${MPI_TARGET_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(${MPI_TARGET_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Knapsackmpi
    ${Boost_INCLUDE_DIRS}
)
target_compile_options(${MPI_TARGET_NAME} PUBLIC ${OpenMP_CXX_FLAGS} ${Boost_COMPILE_OPTIONS})
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(${MPI_TARGET_NAME} PUBLIC ENABLE_TIMERS=1)
endif()
target_link_libraries(${MPI_TARGET_NAME} PUBLIC
    MPI::MPI_CXX Boost::mpi Boost::serialization fmt::fmt spdlog::spdlog OpenMP::OpenMP_CXX
)
