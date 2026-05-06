#pragma once
#include <fmt/core.h>
#include <fmt/format.h>
#include <iostream>
#ifndef MAX_LOG_SIZE
#define MAX_LOG_SIZE 1024
#endif
#ifdef DEBUG
constexpr bool _is_debug = true;
#else 
constexpr bool _is_debug = false;
#endif

constexpr size_t max_log_size = MAX_LOG_SIZE;

