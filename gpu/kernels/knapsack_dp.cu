#include <__clang_cuda_runtime_wrapper.h>
#include <cstdio>
#include <cuda.h>
#include <cuda_runtime.h>

__global__ void entrypoint()
{
	printf("Hello cuda");
}

int fun()
{
	entrypoint<<<1, 1>>>();
}
