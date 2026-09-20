#include "knapsackdp.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cuda_runtime.h>
#include <stdexcept>
#include <string>

namespace
{
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

int default_threads()
{
	return env_int("KP_GPU_THREADS", 256);
}

int default_blocks(int threads)
{
	const char *env = std::getenv("KP_GPU_BLOCKS");
	if (env != nullptr)
	{
		char *end = nullptr;
		const long value = std::strtol(env, &end, 10);
		if (end != env && value > 0 && value <= 1'000'000)
		{
			return static_cast<int>(value);
		}
	}

	int device = 0;
	int smCount = 1;
	int maxThreadsPerSm = 2048;
	if (cudaGetDevice(&device) == cudaSuccess &&
		cudaDeviceGetAttribute(&smCount, cudaDevAttrMultiProcessorCount, device) == cudaSuccess &&
		cudaDeviceGetAttribute(&maxThreadsPerSm, cudaDevAttrMaxThreadsPerMultiProcessor, device) == cudaSuccess)
	{
		const int perSm = std::max(1, maxThreadsPerSm / threads);
		return std::max(1, smCount * perSm);
	}
	return std::max(1, maxThreadsPerSm / threads);
}

__global__ void knapsack_step_kernel(const int *prev, int *cur, int weight, int value, int cols)
{
	const int stride = blockDim.x * gridDim.x;
	for (int w = blockIdx.x * blockDim.x + threadIdx.x; w < cols; w += stride)
	{
		const int exclude = prev[w];
		const int include = (weight <= w) ? prev[w - weight] + value : exclude;
		cur[w] = include > exclude ? include : exclude;
	}
}
} // namespace

namespace kp::gpu
{
KnapsackSolution knapsackdp(const std::vector<int> &weights, const std::vector<int> &values, int capacity)
{
	const int n = static_cast<int>(weights.size());
	const int cols = capacity + 1;
	const std::size_t tableSize = static_cast<std::size_t>(n + 1) * cols;

	const int threads = default_threads();
	const int blocks = default_blocks(threads);

	int *d_dp = nullptr;
	cuda_check(cudaMalloc(&d_dp, tableSize * sizeof(int)), "cudaMalloc");
	cuda_check(cudaMemset(d_dp, 0, tableSize * sizeof(int)), "cudaMemset");

	for (int i = 1; i <= n; ++i)
	{
		const int *prev = d_dp + static_cast<std::size_t>(i - 1) * cols;
		int *cur = d_dp + static_cast<std::size_t>(i) * cols;
		knapsack_step_kernel<<<blocks, threads>>>(prev, cur, weights[i - 1], values[i - 1], cols);
		cuda_check(cudaGetLastError(), "knapsack_step_kernel");
	}
	cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

	std::vector<int> dp_table(tableSize);
	cuda_check(cudaMemcpy(dp_table.data(), d_dp, tableSize * sizeof(int), cudaMemcpyDeviceToHost), "cudaMemcpy");
	cuda_check(cudaFree(d_dp), "cudaFree");

	std::vector<int> includedItems;
	int totalValue = dp_table[static_cast<std::size_t>(n) * cols + capacity];
	const int maxValue = totalValue;
	int totalWeight = 0;
	int w = capacity;

	for (int i = n; i > 0 && totalValue > 0; --i)
	{
		if (totalValue != dp_table[static_cast<std::size_t>(i - 1) * cols + w])
		{
			includedItems.push_back(i - 1);
			totalValue -= values[i - 1];
			w -= weights[i - 1];
			totalWeight += weights[i - 1];
		}
	}

	return {includedItems, maxValue, totalWeight};
}
} // namespace kp::gpu
