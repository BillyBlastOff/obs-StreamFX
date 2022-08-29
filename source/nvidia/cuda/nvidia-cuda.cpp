/*
 * Modern effects for a modern Streamer
 * Copyright (C) 2020 Michael Fabian Dirks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "nvidia-cuda.hpp"
#include "util/util-logging.hpp"

#include "warning-disable.hpp"
#include <mutex>
#include "warning-enable.hpp"

#ifdef _DEBUG
#define ST_PREFIX "<%s> "
#define D_LOG_ERROR(x, ...) P_LOG_ERROR(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_WARNING(x, ...) P_LOG_WARN(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_INFO(x, ...) P_LOG_INFO(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#define D_LOG_DEBUG(x, ...) P_LOG_DEBUG(ST_PREFIX##x, __FUNCTION_SIG__, __VA_ARGS__)
#else
#define ST_PREFIX "<nvidia::cuda::cuda> "
#define D_LOG_ERROR(...) P_LOG_ERROR(ST_PREFIX __VA_ARGS__)
#define D_LOG_WARNING(...) P_LOG_WARN(ST_PREFIX __VA_ARGS__)
#define D_LOG_INFO(...) P_LOG_INFO(ST_PREFIX __VA_ARGS__)
#define D_LOG_DEBUG(...) P_LOG_DEBUG(ST_PREFIX __VA_ARGS__)
#endif

#if defined(_WIN32) || defined(_WIN64)
#define ST_CUDA_NAME "nvcuda.dll"
#else
#define ST_CUDA_NAME "libcuda.so.1"
#endif

#define P_CUDA_LOAD_SYMBOL(NAME)                                                             \
	{                                                                                        \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#NAME));               \
		if (!NAME)                                                                           \
			throw std::runtime_error("Failed to load '" #NAME "' from '" ST_CUDA_NAME "'."); \
	}
#define P_CUDA_LOAD_SYMBOL_OPT(NAME)                                            \
	{                                                                           \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#NAME));  \
		if (!NAME)                                                              \
			D_LOG_WARNING("Loading of optional symbol '" #NAME "' failed.", 0); \
	}

#define P_CUDA_LOAD_SYMBOL_EX(NAME, OVERRIDE)                                                \
	{                                                                                        \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#OVERRIDE));           \
		if (!NAME)                                                                           \
			throw std::runtime_error("Failed to load '" #NAME "' from '" ST_CUDA_NAME "'."); \
	}
#define P_CUDA_LOAD_SYMBOL_OPT_EX(NAME, OVERRIDE)                                  \
	{                                                                              \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#OVERRIDE)); \
		if (!NAME)                                                                 \
			D_LOG_WARNING("Loading of optional symbol '" #NAME "' failed.", 0);    \
	}

#define P_CUDA_LOAD_SYMBOL_V2(NAME)                                                          \
	{                                                                                        \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#NAME "_v2"));         \
		if (!NAME)                                                                           \
			throw std::runtime_error("Failed to load '" #NAME "' from '" ST_CUDA_NAME "'."); \
	}
#define P_CUDA_LOAD_SYMBOL_OPT_V2(NAME)                                              \
	{                                                                                \
		NAME = reinterpret_cast<decltype(NAME)>(_library->load_symbol(#NAME "_v2")); \
		if (!NAME)                                                                   \
			D_LOG_WARNING("Loading of optional symbol '" #NAME "' failed.", 0);      \
	}

streamfx::nvidia::cuda::cuda::~cuda()
{
	D_LOG_DEBUG("Finalizing... (Addr: 0x%" PRIuPTR ")", this);
}

streamfx::nvidia::cuda::cuda::cuda() : _library()
{
	int32_t cuda_version = 0;

	D_LOG_DEBUG("Initializing... (Addr: 0x%" PRIuPTR ")", this);

	_library = streamfx::util::library::load(std::string_view(ST_CUDA_NAME));

	{ // 1. Load critical initialization functions.
		// Initialization
		P_CUDA_LOAD_SYMBOL(cuInit);

		// Version Management
		P_CUDA_LOAD_SYMBOL(cuDriverGetVersion);
	}

	{ // 2. Get the CUDA Driver version and log it.
		if (cuDriverGetVersion(&cuda_version) == result::SUCCESS) {
			int32_t major = cuda_version / 1000;
			int32_t minor = (cuda_version % 1000) / 10;
			int32_t patch = (cuda_version % 10);
			D_LOG_INFO("Driver reported CUDA version: %" PRId32 ".%" PRId32 ".%" PRId32, major, minor, patch);
		} else {
			D_LOG_WARNING("Failed to query NVIDIA CUDA Driver for version.", 0);
		}
	}

	{ // 3. Load remaining functions.
		// Device Management
		P_CUDA_LOAD_SYMBOL(cuDeviceGetName);
		P_CUDA_LOAD_SYMBOL(cuDeviceGetLuid);
		P_CUDA_LOAD_SYMBOL(cuDeviceGetUuid);

		// Primary Context Management
		P_CUDA_LOAD_SYMBOL(cuDevicePrimaryCtxRetain);
		P_CUDA_LOAD_SYMBOL_V2(cuDevicePrimaryCtxRelease);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuDevicePrimaryCtxSetFlags);

		// Context Management
		P_CUDA_LOAD_SYMBOL_V2(cuCtxCreate);
		P_CUDA_LOAD_SYMBOL_V2(cuCtxDestroy);
		P_CUDA_LOAD_SYMBOL_V2(cuCtxPushCurrent);
		P_CUDA_LOAD_SYMBOL_V2(cuCtxPopCurrent);
		P_CUDA_LOAD_SYMBOL_OPT(cuCtxGetCurrent);
		P_CUDA_LOAD_SYMBOL_OPT(cuCtxSetCurrent);
		P_CUDA_LOAD_SYMBOL(cuCtxGetStreamPriorityRange);
		P_CUDA_LOAD_SYMBOL(cuCtxSynchronize);

		// Module Management
		// - Not yet needed.

		// Memory Management
		P_CUDA_LOAD_SYMBOL_V2(cuMemAlloc);
		P_CUDA_LOAD_SYMBOL_V2(cuMemAllocPitch);
		P_CUDA_LOAD_SYMBOL_V2(cuMemFree);
		P_CUDA_LOAD_SYMBOL(cuMemcpy);
		P_CUDA_LOAD_SYMBOL_V2(cuMemcpy2D);
		P_CUDA_LOAD_SYMBOL_V2(cuMemcpy2DAsync);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuArrayGetDescriptor);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyAtoA);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyAtoD);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyAtoH);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyAtoHAsync);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyDtoA);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyDtoD);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyDtoH);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyDtoHAsync);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyHtoA);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyHtoAAsync);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyHtoD);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemcpyHtoDAsync);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemHostGetDevicePointer);
		P_CUDA_LOAD_SYMBOL_V2(cuMemsetD8);
		P_CUDA_LOAD_SYMBOL(cuMemsetD8Async);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemsetD16);
		P_CUDA_LOAD_SYMBOL_OPT(cuMemsetD16Async);
		P_CUDA_LOAD_SYMBOL_OPT_V2(cuMemsetD32);
		P_CUDA_LOAD_SYMBOL_OPT(cuMemsetD32Async);

		// Virtual Memory Management
		// - Not yet needed.

		// Stream Ordered Memory Allocator
		// - Not yet needed.

		// Unified Addressing
		// - Not yet needed.

		// Stream Management
		P_CUDA_LOAD_SYMBOL(cuStreamCreate);
		P_CUDA_LOAD_SYMBOL_V2(cuStreamDestroy);
		P_CUDA_LOAD_SYMBOL(cuStreamSynchronize);
		P_CUDA_LOAD_SYMBOL_OPT(cuStreamCreateWithPriority);
		P_CUDA_LOAD_SYMBOL_OPT(cuStreamGetPriority);

		// Event Management
		// - Not yet needed.

		// External Resource Interoperability (CUDA 11.1+)
		// - Not yet needed.

		// Stream Memory Operations
		// - Not yet needed.

		// Execution Control
		// - Not yet needed.

		// Graph Management
		// - Not yet needed.

		// Occupancy
		// - Not yet needed.

		// Texture Object Management
		// - Not yet needed.

		// Surface Object Management
		// - Not yet needed.

		// Peer Context Memory Access
		// - Not yet needed.

		// Graphics Interoperability
		P_CUDA_LOAD_SYMBOL(cuGraphicsMapResources);
		P_CUDA_LOAD_SYMBOL(cuGraphicsSubResourceGetMappedArray);
		P_CUDA_LOAD_SYMBOL(cuGraphicsUnmapResources);
		P_CUDA_LOAD_SYMBOL(cuGraphicsUnregisterResource);

		// Driver Entry Point Access
		// - Not yet needed.

		// Profiler Control
		// - Not yet needed.

		// OpenGL Interoperability
		// - Not yet needed.

		// VDPAU Interoperability
		// - Not yet needed.

		// EGL Interoperability
		// - Not yet needed.

#ifdef WIN32
		// Direct3D9 Interoperability
		// - Not yet needed.

		// Direct3D10 Interoperability
		P_CUDA_LOAD_SYMBOL(cuD3D10GetDevice);
		P_CUDA_LOAD_SYMBOL_OPT(cuGraphicsD3D10RegisterResource);

		// Direct3D11 Interoperability
		P_CUDA_LOAD_SYMBOL(cuD3D11GetDevice);
		P_CUDA_LOAD_SYMBOL_OPT(cuGraphicsD3D11RegisterResource);
#endif
	}

	// Initialize CUDA
	cuInit(0);
}

int32_t streamfx::nvidia::cuda::cuda::version()
{
	int32_t v = 0;
	cuDriverGetVersion(&v);
	return v;
}

std::shared_ptr<streamfx::nvidia::cuda::cuda> streamfx::nvidia::cuda::cuda::get()
{
	static std::weak_ptr<streamfx::nvidia::cuda::cuda> instance;
	static std::mutex                                  lock;

	std::unique_lock<std::mutex> ul(lock);
	if (instance.expired()) {
		auto hard_instance = std::make_shared<streamfx::nvidia::cuda::cuda>();
		instance           = hard_instance;
		return hard_instance;
	}
	return instance.lock();
}
