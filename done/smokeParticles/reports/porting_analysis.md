# Porting Analysis: smokeParticles

## Summary
- Total CUDA files: 1 (.cu) + 3 (.cuh)
- Total C++/H files with CUDA deps: 4 (.cpp/.h)
- Total pure C++/OpenGL files: 8 (no CUDA changes needed)
- Total CUDA LOC: 378 (kernel code); ~200 additional CUDA API lines in .cpp/.h files
- Estimated effort: **Medium**
- Key challenges: Thrust migration, CUDA-GL interop, 3D texture objects, NVIDIA helper headers

## File Inventory

| File | Type | LOC | CUDA APIs Used | Manual Fixes Needed |
|------|------|-----|----------------|---------------------|
| `ParticleSystem_cuda.cu` | CUDA kernel wrappers | 160 | cudaMemcpyToSymbol, cudaMalloc3DArray, cudaMemcpy3D, cudaCreateTextureObject, Thrust | Rename to .hip, API renames, Thrust->rocThrust |
| `particles_kernel_device.cuh` | Device functors | 121 | cudaTextureObject_t, tex3D, __constant__, Thrust, helper_math.h | API renames, header swaps |
| `particles_kernel.cuh` | Shared structs | 51 | vector_types.h | Header swap only |
| `ParticleSystem.cuh` | extern "C" declarations | 46 | None (just includes) | Minimal |
| `GpuArray.h` | GPU array template class | 287 | cudaMalloc, cudaFree, cudaMemcpy, cudaGraphicsGL* (6 functions) | All CUDA->HIP API renames |
| `ParticleSystem.cpp` | Host particle logic | 403 | cuda_gl_interop.h, cuda_runtime.h, helper_cuda.h | Include swaps, CUDART_PI_F |
| `particleDemo.cpp` | Main/UI (GLUT) | 947 | cuda_gl_interop.h, cuda_runtime.h, helper_cuda.h, findCudaDevice | Include swaps, device init |
| `GLSLProgram.cpp` | OpenGL shaders | - | None | No changes |
| `SmokeRenderer.cpp` | OpenGL rendering | - | None | No changes |
| `SmokeShaders.cpp` | Shader strings | - | None | No changes |
| `framebufferObject.cpp` | FBO wrapper | - | None | No changes |
| `renderbuffer.cpp` | Renderbuffer wrapper | - | None | No changes |
| `*.h` (math/vector) | Math utilities | - | None | No changes |

## Auto-Convertible (hipify handles these)

- CUDA runtime API renames (~40 occurrences):
  - `cudaMalloc` -> `hipMalloc`
  - `cudaFree` -> `hipFree`
  - `cudaMemcpy` -> `hipMemcpy`
  - `cudaMemcpyToSymbol` -> `hipMemcpyToSymbol`
  - `cudaMalloc3DArray` -> `hipMalloc3DArray`
  - `cudaMemcpy3D` -> `hipMemcpy3D`
  - `cudaCreateTextureObject` -> `hipCreateTextureObject`
  - `cudaTextureObject_t` -> `hipTextureObject_t`
  - `cudaGraphicsGLRegisterBuffer` -> `hipGraphicsGLRegisterBuffer`
  - `cudaGraphicsMapResources` -> `hipGraphicsMapResources`
  - `cudaGraphicsResourceGetMappedPointer` -> `hipGraphicsResourceGetMappedPointer`
  - `cudaGraphicsUnmapResources` -> `hipGraphicsUnmapResources`
  - `cudaGraphicsUnregisterResource` -> `hipGraphicsUnregisterResource`
- Type renames (~15 occurrences):
  - `cudaExtent` -> `hipExtent`
  - `cudaChannelFormatDesc` -> `hipChannelFormatDesc`
  - `cudaResourceDesc` -> `hipResourceDesc`
  - `cudaTextureDesc` -> `hipTextureDesc`
  - `cudaMemcpy3DParms` -> `hipMemcpy3DParms`
  - `cudaArray` -> `hipArray`
  - `cudaGraphicsResource` -> `hipGraphicsResource`
- Enum/constant renames (~10 occurrences):
  - `cudaMemcpyHostToDevice` -> `hipMemcpyHostToDevice`
  - `cudaMemcpyDeviceToHost` -> `hipMemcpyDeviceToHost`
  - `cudaResourceTypeArray` -> `hipResourceTypeArray`
  - `cudaFilterModeLinear` -> `hipFilterModeLinear`
  - `cudaAddressModeWrap` -> `hipAddressModeWrap`
  - `cudaReadModeElementType` -> `hipReadModeElementType`
  - `cudaGraphicsMapFlagsWriteDiscard` -> `hipGraphicsMapFlagsWriteDiscard`
- Include swaps (~8 occurrences):
  - `cuda_runtime.h` -> `hip/hip_runtime.h`
  - `cuda_gl_interop.h` -> `hip/hip_runtime.h` (HIP GL interop is in main header)

## Manual Fixes Required

### Critical (will cause incorrect results if missed)

- **None** -- No warp-size assumptions, no shuffle/ballot operations, no hardcoded warp masks

### Required (will cause build failures)

- [ ] **Thrust -> rocThrust migration** (`ParticleSystem_cuda.cu`, `particles_kernel_device.cuh`)
  - `#include "thrust/..."` headers remain the same with rocThrust (Thrust API is preserved)
  - Must link against `-lrocthrust` and ensure rocThrust is installed
  - The `--extended-lambda` nvcc flag is not needed with hipcc (lambdas work natively)

- [ ] **CUDA-OpenGL interop** (`GpuArray.h`, `ParticleSystem.cpp`, `particleDemo.cpp`)
  - `cuda_gl_interop.h` -> HIP GL interop functions are in `hip/hip_runtime.h` or `hip/hip_gl_interop.h`
  - All `cudaGraphicsGL*` functions have `hipGraphicsGL*` equivalents
  - **Note:** HIP-GL interop requires ROCm to be built with OpenGL support

- [ ] **3D Texture objects** (`ParticleSystem_cuda.cu:74-118`, `particles_kernel_device.cuh:43-50`)
  - `cudaCreateChannelDesc<float4>()` -> `hipCreateChannelDesc(32,32,32,32, hipChannelFormatKindFloat)`
  - `make_cudaExtent` -> `make_hipExtent`
  - `make_cudaPitchedPtr` -> `make_hipPitchedPtr`
  - `tex3D<float4>(texObj, x, y, z)` -> same syntax in HIP

- [ ] **NVIDIA helper headers** (used in 5+ files)
  - `helper_cuda.h` -> Must provide HIP equivalent: `checkCudaErrors()` -> custom error-checking macro
  - `helper_gl.h` -> Pure OpenGL, can be reused if not CUDA-dependent
  - `helper_functions.h` -> SDK timer (sdkCreateTimer, etc.) -- pure host code, may need minor adaptation
  - `helper_math.h` -> HIP provides `make_float3`, `make_float4`, etc. natively
  - `math_constants.h` -> Available in HIP
  - `vector_types.h` -> Available in HIP natively
  - `paramgl.h` -> Pure GLUT/OpenGL, no changes needed

- [ ] **`__constant__` memory** (`particles_kernel_device.cuh:45`)
  - `__constant__ SimParams cudaParams;` -> rename symbol and update `cudaMemcpyToSymbol` call
  - `cudaMemcpyToSymbol(cudaParams, ...)` -> `hipMemcpyToSymbol(hipParams, ...)`

- [ ] **Device init** (`particleDemo.cpp:878,926`)
  - `findCudaDevice()` -> `hipSetDevice()` or equivalent HIP device selection

- [ ] **`CUDART_PI_F` macro** (`ParticleSystem.cpp:50-52`)
  - Replace with `HIP_PI_F` or keep the manual `#define` fallback (already guarded)

- [ ] **Build system (CMakeLists.txt)** -- Full rewrite required:
  - Remove `project(... CUDA)`, `find_package(CUDAToolkit)`
  - Add `find_package(hip REQUIRED)`, `find_package(rocthrust REQUIRED)`
  - Rename `ParticleSystem_cuda.cu` -> `ParticleSystem_cuda.hip`
  - Remove `--extended-lambda`, `CUDA_SEPARABLE_COMPILATION`, `CMAKE_CUDA_ARCHITECTURES`
  - Add `--offload-arch=gfx90a` for MI250x
  - Link `hip::device`, `roc::rocthrust`
  - Replace `${CUDAToolkit_INCLUDE_DIRS}` with HIP include dirs

### Optional (for performance)
- [ ] Launch bounds tuning (N/A -- no explicit kernel launches, Thrust handles this)
- [ ] rocThrust execution policy tuning

## Unsupported Features
- **None** -- All CUDA features used in this project have HIP equivalents

## Warp-Level Analysis
- **No warp-level code found** -- This is a clean project for porting
- No `__shfl_*`, `__ballot_*`, `__any_sync`, `__all_sync`, `__activemask`
- No hardcoded warp size constants (32)
- No warp-level reductions
- All GPU computation uses Thrust algorithms (for_each, sort_by_key, sequence)

## Library Dependencies

| CUDA Library | HIP Equivalent | Status |
|-------------|----------------|--------|
| Thrust | rocThrust | Available (full API compatibility) |
| CUDA Runtime | HIP Runtime | Available |
| CUDA-GL Interop | HIP-GL Interop | Available (requires ROCm GL support) |
| CUDA Texture Objects | HIP Texture Objects | Available |

## External Dependencies (non-CUDA)

| Dependency | Purpose | Porting Impact |
|-----------|---------|----------------|
| OpenGL | Rendering | No change needed |
| GLUT (freeglut) | Window/UI | No change needed |
| NVIDIA CUDA Samples Common | Helpers (timer, error checking, math) | Must provide HIP equivalents |

## Build System

- **Current:** CMake with `enable_language(CUDA)`, `find_package(CUDAToolkit)`
- **Target:** CMake with `find_package(hip)`, `find_package(rocthrust)`
- **Changes needed:** Full CMakeLists.txt rewrite (see Required section above)

## Recommended Porting Order

1. **`particles_kernel.cuh`** -- Shared data structures, no CUDA APIs. Trivial: just swap `vector_types.h` include.
2. **`particles_kernel_device.cuh`** -- Device functors with texture lookups and Thrust. Core GPU logic. Rename CUDA types, swap headers.
3. **`ParticleSystem_cuda.cu` -> `.hip`** -- Kernel wrappers with 3D texture setup and Thrust calls. Depends on #1 and #2.
4. **`ParticleSystem.cuh`** -- Extern "C" declarations. Trivial, just include adjustments.
5. **`GpuArray.h`** -- CUDA-GL interop template class. Systematic API renames. Self-contained.
6. **`ParticleSystem.cpp`** -- Host code with CUDA includes. Depends on #4 and #5.
7. **`particleDemo.cpp`** -- Main entry point. Depends on #6. Device init and include swaps.
8. **`CMakeLists.txt`** -- Build system. Do last once all source files compile.
9. **Helper header shims** -- Create minimal HIP wrappers for `helper_cuda.h`, `helper_math.h` if needed. Can be done early as a foundation step.

## Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| HIP-GL interop not working | Medium | Test with `--qatest` mode first (no GL interop) |
| rocThrust API incompatibility | Low | API is very stable; test with simple sort first |
| 3D texture object differences | Low | HIP texture API closely mirrors CUDA |
| NVIDIA helper headers | Medium | Write thin HIP adapter or use HIP-samples equivalents |
| Missing OpenGL on headless system | Low | Use `--qatest` flag for headless validation |
