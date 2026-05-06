#pragma once
#include<boost/mpi.hpp>
enum MessageTag{
 CUR_LEN_TAG = 3,
 SIZES_TAG = 1,
 CHUNK_TAG = 2,
 AVAILABLE_RESP_TAG = 5,
 RESULT_TAG =4 ,
 RESULT_SIZE = 64,
 STOP_TAG = 10,
 MAX_ELEM_ALLOC = 12,
 BRUTE_TASK_TAG = 14,
 UNLOCK_TAG=99,
};

