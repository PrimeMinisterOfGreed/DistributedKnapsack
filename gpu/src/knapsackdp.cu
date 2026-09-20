#include "knapsackdp.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cuda_runtime.h>
#include <stdexcept>
#include <string>

namespace
{
double g_last_dp_time_ms = 0.0;

void cuda_check(cudaError_t status, const char *what)
{
	if (status != cudaSuccess)
	{
		throw std::runtime_error(std::string(what) + ": " + cudaGetErrorString(status));
	}
}

int env_int(const char *name, int fallback)
{
	const char *env = std::getenv(name);
	if (env == nullptr)
	{
		return fallback;
	}
	char *end = nullptr;
	const long value = std::strtol(env, &end, 10);
	if (end == env || value <= 0 || value > 1'000'000)
	{
		return fallback;
	}
	return static_cast<int>(value);
}

struct DeviceLimits
{
	int maxThreadsPerBlock;
	int warpSize;
};

DeviceLimits device_limits()
{
	static const DeviceLimits limits = []() {
		DeviceLimits l{1024, 32};
		int device = 0;
		if (cudaGetDevice(&device) == cudaSuccess)
		{
			int value = 0;
			if (cudaDeviceGetAttribute(&value, cudaDevAttrMaxThreadsPerBlock, device) == cudaSuccess && value > 0)
			{
				l.maxThreadsPerBlock = value;
			}
			if (cudaDeviceGetAttribute(&value, cudaDevAttrWarpSize, device) == cudaSuccess && value > 0)
			{
				l.warpSize = value;
			}
		}
		return l;
	}();
	return limits;
}

int resolve_threads()
{
	const DeviceLimits limits = device_limits();
	const int requested = env_int("KP_GPU_THREADS", 256);
	int threads = std::min(requested, limits.maxThreadsPerBlock);
	threads = (threads / limits.warpSize) * limits.warpSize;
	return std::max(limits.warpSize, threads);
}

int default_blocks(int threads, int cols)
{
	// One thread per capacity element: each thread computes exactly one DP cell.
	return std::max(1, (cols + threads - 1) / threads);
}

__global__ void knapsack_step_kernel(const int *__restrict__ prev, int *__restrict__ cur, int weight, int value,
									 int cols)
{
	const int w = blockIdx.x * blockDim.x + threadIdx.x;
	if (w >= cols)
	{
		return;
	}
	const int exclude = prev[w];
	const int include = (weight <= w) ? prev[w - weight] + value : exclude;
	cur[w] = include > exclude ? include : exclude;
}

__global__ void knapsack_backtrack_kernel(const int *__restrict__ dp, const int *__restrict__ weights,
										  const int *__restrict__ values, int n, int capacity, int cols,
										  int *__restrict__ out)
{
	// out[0..n-1] = selected item indices, out[n] = count,
	// out[n+1] = max value, out[n+2] = total weight.
	int w = capacity;
	int totalValue = dp[static_cast<std::size_t>(n) * cols + capacity];
	const int maxValue = totalValue;
	int totalWeight = 0;
	int count = 0;
	for (int i = n; i > 0 && totalValue > 0; --i)
	{
		if (totalValue != dp[static_cast<std::size_t>(i - 1) * cols + w])
		{
			out[count++] = i - 1;
			totalValue -= values[i - 1];
			w -= weights[i - 1];
			totalWeight += weights[i - 1];
		}
	}
	out[n] = count;
	out[n + 1] = maxValue;
	out[n + 2] = totalWeight;
}
} // namespace

namespace kp::gpu
{
KnapsackSolution knapsackdp(const std::vector<int> &weights, const std::vector<int> &values, int capacity)
{
	const int n = static_cast<int>(weights.size());
	if (n == 0)
	{
		return {{}, 0, 0};
	}

	const int cols = capacity + 1;
	const std::size_t tableSize = static_cast<std::size_t>(n + 1) * cols;

	const int threads = resolve_threads();
	const int blocks = default_blocks(threads, cols);
	int *d_dp = nullptr;

	cuda_check(cudaMalloc(&d_dp, tableSize * sizeof(int)), "cudaMalloc");
	// Only row 0 must be zero (dp[0][w] = 0); rows 1..n are fully overwritten.
	cuda_check(cudaMemset(d_dp, 0, static_cast<std::size_t>(cols) * sizeof(int)), "cudaMemset");
	cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

	const auto dpStart = std::chrono::steady_clock::now();

	for (int i = 1; i <= n; ++i)
	{
		const int *prev = d_dp + static_cast<std::size_t>(i - 1) * cols;
		int *cur = d_dp + static_cast<std::size_t>(i) * cols;
		knapsack_step_kernel<<<blocks, threads>>>(prev, cur, weights[i - 1], values[i - 1], cols);
		cuda_check(cudaGetLastError(), "knapsack_step_kernel");
	}
	cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

	const auto dpEnd = std::chrono::steady_clock::now();
	const double dpMs = std::chrono::duration<double, std::milli>(dpEnd - dpStart).count();
	g_last_dp_time_ms = dpMs;
	std::printf("GPU DP execution: %.3f ms\n", dpMs);

	int *d_weights = nullptr;
	int *d_values = nullptr;
	int *d_result = nullptr;
	const std::size_t nSize = static_cast<std::size_t>(n);
	const std::size_t resultCount = nSize + 3;
	cuda_check(cudaMalloc(&d_weights, nSize * sizeof(int)), "cudaMalloc");
	cuda_check(cudaMalloc(&d_values, nSize * sizeof(int)), "cudaMalloc");
	cuda_check(cudaMalloc(&d_result, resultCount * sizeof(int)), "cudaMalloc");
	cuda_check(cudaMemcpy(d_weights, weights.data(), nSize * sizeof(int), cudaMemcpyHostToDevice), "cudaMemcpy");
	cuda_check(cudaMemcpy(d_values, values.data(), nSize * sizeof(int), cudaMemcpyHostToDevice), "cudaMemcpy");

	knapsack_backtrack_kernel<<<1, 1>>>(d_dp, d_weights, d_values, n, capacity, cols, d_result);
	cuda_check(cudaGetLastError(), "knapsack_backtrack_kernel");
	cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

	std::vector<int> result(resultCount);
	cuda_check(cudaMemcpy(result.data(), d_result, resultCount * sizeof(int), cudaMemcpyDeviceToHost), "cudaMemcpy");

	cuda_check(cudaFree(d_dp), "cudaFree");
	cuda_check(cudaFree(d_weights), "cudaFree");
	cuda_check(cudaFree(d_values), "cudaFree");
	cuda_check(cudaFree(d_result), "cudaFree");

	const int count = result[static_cast<std::size_t>(n)];
	const int maxValue = result[static_cast<std::size_t>(n) + 1];
	const int totalWeight = result[static_cast<std::size_t>(n) + 2];
	std::vector<int> includedItems(result.begin(), result.begin() + count);

	return {includedItems, maxValue, totalWeight};
}

double last_dp_time_ms()
{
	return g_last_dp_time_ms;
}
} // namespace kp::gpu
