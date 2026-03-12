# CUDA to ROCm Porting Agent

## Environment

- **Target GPU**: MI250x
- **ROCm Version**: 7.1
- **HIP Compiler**: amdclang++ (via hipcc)
- **Build Systems**: CMake (preferred), Make
- **Profiler**: rocprofv3
- **hipify tool**: hipify-perl (available in ROCm install at /opt/rocm-7.1.1/bin/hipify-perl)

## Project Structure

```
source/              # Original CUDA source (read-only reference)
hip/                 # HIP-ported source (working copy)
build/               # Build output
reports/             # Porting analysis reports
benchmarks/          # Performance comparison results
```

## Porting Philosophy

1. **Never modify the original CUDA source** — always work in `hip/`
2. **Incremental porting** — translate one file at a time, build and test after each
3. **Correctness first, performance second** — get it working, then optimize
4. **Document every manual change** — add comments explaining what was changed and why

## Critical CUDA → HIP Gotchas

These are the issues that cause the most bugs during porting. The agent must check for ALL of these:

### Warp Size (32 → 64)
This is the #1 source of silent correctness bugs. AMD GCN/CDNA wavefronts are 64 threads, not 32.
- Replace hardcoded `32` in warp-level code with `warpSize` or `__AMDGCN_WAVEFRONT_SIZE`
- Warp shuffle operations: masks like `0xFFFFFFFF` assume 32 threads — needs `0xFFFFFFFFFFFFFFFF` for 64
- `__ballot_sync` returns `unsigned int` (32-bit) in CUDA but `unsigned long long` (64-bit) in HIP
- Manual warp reductions with `for (int offset = 16; offset > 0; offset >>= 1)` must start at 32 on AMD
- Shared memory sized as `[BLOCK_SIZE / 32]` must become `[BLOCK_SIZE / warpSize]`

### Architecture Macros
| CUDA | HIP |
|------|-----|
| `__CUDA_ARCH__` | `__HIP_DEVICE_COMPILE__` |
| `__CUDACC__` | `__HIPCC__` |
| `SM_XX` checks | Use `__gfx9XX__` or feature macros |

### Launch Configuration
- Max threads per block: 1024 (same), but optimal sizes differ
- `__launch_bounds__(maxThreads, minBlocks)` — keep but may need tuning
- Grid/block dimensions: same API but different performance characteristics

### Memory
- `cudaMallocManaged` → `hipMallocManaged` (works but performance characteristics differ)
- Unified memory migration behavior differs between NVIDIA and AMD
- `__constant__` memory: same syntax via HIP, but size limits may vary
- Pinned memory: `cudaHostAlloc` → `hipHostMalloc` (flag names differ)

### Libraries Mapping
| CUDA Library | HIP Library | Link Flag |
|-------------|-------------|-----------|
| cuBLAS | hipBLAS | -lhipblas |
| cuFFT | hipFFT | -lhipfft |
| cuRAND | hipRAND | -lhiprand |
| cuSPARSE | hipSPARSE | -lhipsparse |
| cuSOLVER | hipSOLVER | -lhipsolver |
| cuDNN | MIOpen | -lMIOpen |
| NCCL | RCCL | -lrccl |
| Thrust | hipThrust / rocThrust | -lrocthrust |
| CUB | hipCUB / rocPRIM | -lrocprim |

### Texture / Surface Objects
- HIP supports texture objects but API has subtle differences
- `cudaCreateTextureObject` → `hipCreateTextureObject` (mostly compatible)
- Texture reference API is deprecated in both — convert to texture objects if possible

### Cooperative Groups
- Basic cooperative groups (`thread_block`, `tiled_partition`) are supported in HIP
- `grid_group` (grid-wide sync) has limited support — check ROCm version
- `multi_grid_group` is not supported

### Error Handling
- `hipError_t` is NOT a direct alias for `cudaError_t`
- Use `hipGetErrorString()` instead of `cudaGetErrorString()`
- Error code numeric values differ — never compare against hardcoded integers

## Build System Patterns

### CMake: CUDA → HIP

Replace:
```cmake
enable_language(CUDA)
set(CMAKE_CUDA_ARCHITECTURES 70 80 90)
add_executable(app kernel.cu main.cpp)
```

With:
```cmake
find_package(hip REQUIRED)
# Rename .cu files to .hip
set_source_files_properties(kernel.hip PROPERTIES HIP_SOURCE_PROPERTY_FORMAT 1)
hip_add_executable(app kernel.hip main.cpp)
target_link_libraries(app hip::device)
```

Or use the portable approach:
```cmake
cmake_minimum_required(VERSION 3.21)
project(app LANGUAGES CXX)
find_package(hip REQUIRED)
add_executable(app kernel.hip main.cpp)
target_link_libraries(app hip::device)
set_source_files_properties(kernel.hip PROPERTIES LANGUAGE HIP)
```

### Makefile: CUDA → HIP

Replace:
```makefile
NVCC = nvcc
NVCC_FLAGS = -arch=sm_80
%.o: %.cu
	$(NVCC) $(NVCC_FLAGS) -c $< -o $@
```

With:
```makefile
HIPCC = hipcc
HIP_FLAGS = --offload-arch=gfx942   # MI300x
%.o: %.hip
	$(HIPCC) $(HIP_FLAGS) -c $< -o $@
```

## Verification

After porting, always:
1. Build with `hipcc` — fix all compiler errors and warnings
2. Run the original test suite / validation
3. Compare numerical output against CUDA reference (allow small floating-point tolerance)
4. Check for runtime errors with `HIP_VISIBLE_DEVICES=0 ./app`
5. Optionally run with `HIPCHECK=1` for additional runtime checks
