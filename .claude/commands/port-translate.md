# Port Translate

Perform the mechanical CUDA→HIP translation using hipify, then fix the build system.

## Input

The user may provide a specific file or directory: $ARGUMENTS

If no argument is given, translate the entire project from `source/` into `hip/`.

## Prerequisites

- The porting analysis report should already exist in `reports/porting_analysis.md`
- If it doesn't exist, tell the user to run `/port-analyze` first

## Workflow

### 1. Copy source to working directory

```bash
# Create clean working copy
rm -rf hip/
cp -r source/ hip/
```

### 2. Run hipify-perl on all CUDA files

```bash
# Convert in-place in the hip/ directory
hipconvertinplace-perl.sh hip/
```

Review the hipify output. Note:
- Number of conversions made
- Any warnings about unconverted calls
- Any files that were skipped

### 3. Rename file extensions

```bash
# Rename .cu → .hip
find hip/ -name "*.cu" -exec bash -c 'mv "$0" "${0%.cu}.hip"' {} \;

# Rename .cuh → .hip.h (or keep as .cuh — HIP supports both)
# Keeping .cuh is simpler for include compatibility
```

### 4. Fix includes

In all converted files, ensure the HIP runtime header is included:

```cpp
// Replace:
#include <cuda_runtime.h>
#include <cuda.h>
#include <cuda_runtime_api.h>

// With:
#include <hip/hip_runtime.h>
```

Also fix library includes:
```cpp
// cuBLAS → hipBLAS
#include <cublas_v2.h>       →  #include <hipblas/hipblas.h>

// cuFFT → hipFFT
#include <cufft.h>           →  #include <hipfft/hipfft.h>

// cuRAND → hipRAND
#include <curand.h>          →  #include <hiprand/hiprand.h>

// cuSPARSE → hipSPARSE
#include <cusparse.h>        →  #include <hipsparse/hipsparse.h>

// Thrust → hipThrust
#include <thrust/...>        →  #include <thrust/...>  (same, rocThrust is compatible)

// CUB → hipCUB
#include <cub/...>           →  #include <hipcub/hipcub.hpp>
```

### 5. Fix the build system

#### If CMake:

Create a new `hip/CMakeLists.txt` based on the original. Key changes:

1. Remove `enable_language(CUDA)` and `find_package(CUDA)`
2. Add `find_package(hip REQUIRED)` and any HIP library packages
3. Replace `cuda_add_executable` / `add_executable` with `hip_add_executable` or set HIP language on sources
4. Update source file extensions from `.cu` to `.hip`
5. Replace `-arch=sm_XX` flags with `--offload-arch=gfx942` (MI300x)
6. Update link libraries (see library mapping in CLAUDE.md)
7. Remove CUDA-specific cmake variables (`CMAKE_CUDA_ARCHITECTURES`, etc.)

Example transformation:
```cmake
# Before
cmake_minimum_required(VERSION 3.18)
project(myapp LANGUAGES CXX CUDA)
set(CMAKE_CUDA_ARCHITECTURES 70 80)
add_executable(myapp main.cpp kernel.cu)
target_link_libraries(myapp cublas)

# After
cmake_minimum_required(VERSION 3.21)
project(myapp LANGUAGES CXX)
find_package(hip REQUIRED)
find_package(hipblas REQUIRED)
add_executable(myapp main.cpp kernel.hip)
set_source_files_properties(kernel.hip PROPERTIES LANGUAGE HIP)
target_link_libraries(myapp hip::device roc::hipblas)
```

#### If Makefile:

1. Replace `NVCC` / `nvcc` with `HIPCC` / `hipcc`
2. Replace `-arch=sm_XX` with `--offload-arch=gfx942`
3. Update source file extensions in rules and dependencies
4. Update library link flags (see CLAUDE.md)
5. Update include paths: add `-I/opt/rocm/include` if needed

### 6. Fix helper / utility headers

Many CUDA projects include NVIDIA's helper headers (`helper_cuda.h`, `helper_functions.h`, etc.). These need to be either:
- Replaced with HIP equivalents (check if the project has them)
- Adapted manually (usually thin wrappers around error checking)

Common pattern — replace the CUDA error check macro:
```cpp
// CUDA version
#define checkCudaErrors(val) check((val), #val, __FILE__, __LINE__)

// HIP version
#define checkHipErrors(val) { \
    hipError_t err = (val); \
    if (err != hipSuccess) { \
        fprintf(stderr, "HIP Error: %s at %s:%d\n", \
                hipGetErrorString(err), __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
}
```

### 7. Attempt first build

```bash
cd hip/
mkdir -p build && cd build
cmake .. -DCMAKE_HIP_ARCHITECTURES=gfx942
make -j$(nproc) 2>&1 | tee ../../reports/first_build.log
```

Or for Makefile:
```bash
cd hip/
make 2>&1 | tee ../reports/first_build.log
```

### 8. Triage build errors

Categorize errors from the build log:

1. **Missing includes** — fix include paths
2. **Unknown type names** — likely missed API renames, fix manually
3. **Undeclared identifiers** — CUDA-specific identifiers hipify missed
4. **Link errors** — missing library link flags

Fix errors one category at a time, rebuilding after each round.

### 9. Report results

Save a translation summary to `reports/translation_summary.md`:

```markdown
# Translation Summary

## hipify Statistics
- Files processed: X
- Auto-converted API calls: Y
- Warnings: Z

## Build System
- Original: CMake / Make
- Changes: <summary>

## Build Status
- [ ] Compiles without errors
- [ ] Compiles without warnings
- Remaining issues: <list if any>

## Manual Fixes Applied
| File | Change | Reason |
|------|--------|--------|
| ... | ... | ... |

## Next Steps
- Run `/port-fix` to address warp size and other semantic issues
```

If the build succeeds, tell the user the project compiles and they should run `/port-fix` next for semantic correctness fixes. If it doesn't, list the remaining errors and offer to fix them.
