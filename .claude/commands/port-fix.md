# Port Fix

Apply semantic fixes that hipify cannot handle. These are correctness-critical changes that address architectural differences between NVIDIA and AMD GPUs.

## Input

The user may provide a specific file to fix: $ARGUMENTS

If no argument is given, scan and fix all files in `hip/`.

## Prerequisites

- The mechanical translation (`/port-translate`) should already be done
- The project should compile (or be close to compiling)

## Workflow

### 1. Scan for all semantic issues

Run a comprehensive scan and present findings before making changes:

```bash
echo "=== Warp size assumptions ==="
grep -rn "0xFFFFFFFF\b" hip/ --include="*.hip" --include="*.h" --include="*.hpp" --include="*.cuh"
grep -rn "\b32\b" hip/ --include="*.hip" --include="*.cuh" | grep -i "warp\|lane\|shfl\|ballot\|mask\|WARP"
grep -rn "blockDim\.[xyz]\s*/\s*32\|threadIdx\.[xyz]\s*%\s*32\|threadIdx\.[xyz]\s*&\s*31" hip/ --include="*.hip" --include="*.cuh"

echo "=== Warp intrinsics ==="
grep -rn "__shfl_sync\|__shfl_up_sync\|__shfl_down_sync\|__shfl_xor_sync\|__ballot_sync\|__any_sync\|__all_sync\|__activemask\|__match_any_sync\|__match_all_sync" hip/ --include="*.hip" --include="*.cuh"

echo "=== Architecture macros ==="
grep -rn "__CUDA_ARCH__\|__CUDACC__" hip/ --include="*.hip" --include="*.h" --include="*.hpp" --include="*.cuh"

echo "=== Launch bounds ==="
grep -rn "__launch_bounds__" hip/ --include="*.hip" --include="*.cuh"

echo "=== Shared memory sizing ==="
grep -rn "shared.*\[.*32\|shared.*\[.*WARP" hip/ --include="*.hip" --include="*.cuh"

echo "=== Dynamic shared memory ==="
grep -rn "extern\s*__shared__" hip/ --include="*.hip" --include="*.cuh"

echo "=== Cooperative groups ==="
grep -rn "cooperative_groups\|grid_group\|this_grid\|multi_grid" hip/ --include="*.hip" --include="*.cuh"

echo "=== Memory fences / barriers ==="
grep -rn "__threadfence\b\|__threadfence_block\|__threadfence_system" hip/ --include="*.hip" --include="*.cuh"

echo "=== Atomics ==="
grep -rn "atomicCAS\|atomicAdd\|atomicExch\|atomicMin\|atomicMax" hip/ --include="*.hip" --include="*.cuh"
```

Present all findings in a summary table and wait for user confirmation before proceeding.

### 2. Fix warp size assumptions (CRITICAL)

This is the most important fix. Warp size is 32 on NVIDIA, 64 on AMD CDNA.

**Pattern: Hardcoded warp size constant**
```cpp
// Before
#define WARP_SIZE 32

// After
#ifdef __HIP_PLATFORM_AMD__
#define WARP_SIZE 64
#else
#define WARP_SIZE 32
#endif
// Or simply: #define WARP_SIZE warpSize (if used only in device code)
```

**Pattern: Warp lane ID calculation**
```cpp
// Before
int laneId = threadIdx.x % 32;
int warpId = threadIdx.x / 32;

// After
int laneId = threadIdx.x % warpSize;
int warpId = threadIdx.x / warpSize;
```

**Pattern: Warp bitmask**
```cpp
// Before
unsigned mask = 0xFFFFFFFF;

// After
#ifdef __HIP_PLATFORM_AMD__
unsigned long long mask = 0xFFFFFFFFFFFFFFFFULL;
#else
unsigned mask = 0xFFFFFFFF;
#endif
```

**Pattern: Ballot return type**
```cpp
// Before
unsigned int ballot = __ballot_sync(0xFFFFFFFF, predicate);

// After
// On AMD, __ballot returns 64-bit
#ifdef __HIP_PLATFORM_AMD__
unsigned long long ballot = __ballot(predicate);
#else
unsigned int ballot = __ballot_sync(0xFFFFFFFF, predicate);
#endif
```

**Pattern: Warp reduction**
```cpp
// Before
for (int offset = 16; offset > 0; offset >>= 1)
    val += __shfl_down_sync(0xFFFFFFFF, val, offset);

// After
for (int offset = warpSize / 2; offset > 0; offset >>= 1)
    val += __shfl_down(val, offset);
```

**Pattern: Shared memory sized by warps**
```cpp
// Before
__shared__ float warpResults[BLOCK_SIZE / 32];

// After
__shared__ float warpResults[BLOCK_SIZE / WARP_SIZE];
// Make sure WARP_SIZE is defined correctly (see above)
```

### 3. Fix architecture macros

```cpp
// Before
#if __CUDA_ARCH__ >= 700
    // Use Volta+ features
#endif

// After
#if defined(__HIP_DEVICE_COMPILE__)
    // AMD GPU path — check if feature is available
    // Note: __HIP_ARCH_HAS_* macros available for feature detection
#elif defined(__CUDA_ARCH__) && __CUDA_ARCH__ >= 700
    // NVIDIA Volta+ path
#endif
```

HIP provides feature detection macros:
- `__HIP_ARCH_HAS_GLOBAL_INT32_ATOMICS__`
- `__HIP_ARCH_HAS_SHARED_FLOAT_ATOMIC_EXCH__`
- `__HIP_ARCH_HAS_DOUBLES__`
- `__HIP_ARCH_HAS_WARP_VOTE__`
- `__HIP_ARCH_HAS_WARP_BALLOT__`
- `__HIP_ARCH_HAS_WARP_SHUFFLE__`
- `__HIP_ARCH_HAS_DYNAMIC_PARALLEL__`

### 4. Fix sync functions

HIP on AMD does not always require the `_sync` suffix — wavefronts on AMD are implicitly synchronous within a wave.

```cpp
// These work in HIP but the non-sync versions are preferred on AMD:
__shfl_sync(mask, val, srcLane)    →  __shfl(val, srcLane)
__shfl_down_sync(mask, val, delta) →  __shfl_down(val, delta)
__shfl_up_sync(mask, val, delta)   →  __shfl_up(val, delta)
__shfl_xor_sync(mask, val, mask)   →  __shfl_xor(val, mask)
__ballot_sync(mask, predicate)     →  __ballot(predicate)
__any_sync(mask, predicate)        →  __any(predicate)
__all_sync(mask, predicate)        →  __all(predicate)
```

Note: For portability (code that compiles on both NVIDIA and AMD), you can keep `_sync` variants — HIP maps them. But the mask argument is ignored on AMD, so don't rely on partial-warp masks for correctness on AMD.

### 5. Fix cooperative groups (if present)

```cpp
// Basic thread_block and tiled_partition work in HIP:
#include <hip/hip_cooperative_groups.h>
namespace cg = cooperative_groups;
auto block = cg::this_thread_block();
auto tile = cg::tiled_partition<32>(block);  // Change 32→64 if needed on AMD

// grid_group requires cooperative launch:
// hipLaunchCooperativeKernel(&kernel, gridDim, blockDim, args, sharedMem, stream)
// Check that the device supports it: hipDeviceGetAttribute(&val, hipDeviceAttributeCooperativeLaunch, dev)
```

### 6. Fix launch bounds

`__launch_bounds__` has the same syntax but optimal values differ on AMD:

```cpp
// NVIDIA: __launch_bounds__(maxThreadsPerBlock, minBlocksPerMultiprocessor)
// AMD: same syntax, but "multiprocessor" = Compute Unit (CU)

// Recommendation: keep existing values as a starting point,
// add a comment noting they may need tuning for AMD
__launch_bounds__(256, 4)  // TODO: tune for MI300x CU occupancy
```

### 7. Fix memory order / fence differences (if relevant)

```cpp
// __threadfence() works in HIP but behavior may differ
// For explicit memory ordering on AMD:
__threadfence();        // Device-wide fence — works in HIP
__threadfence_block();  // Block-wide fence — works in HIP
__threadfence_system(); // System-wide fence — works in HIP
```

### 8. Rebuild and test

```bash
cd hip/build/
cmake .. -DCMAKE_HIP_ARCHITECTURES=gfx942
make -j$(nproc) 2>&1 | tee ../../reports/post_fix_build.log
```

If the build succeeds, run validation:
```bash
./app [test_args] 2>&1 | tee ../../reports/validation.log
```

Compare output against the CUDA reference if available.

### 9. Produce fix report

Save to `reports/semantic_fixes.md`:

```markdown
# Semantic Fix Report

## Warp Size Fixes
| File | Line | Original | Fixed | Pattern |
|------|------|----------|-------|---------|
| ... | ... | ... | ... | hardcoded 32 / mask / reduction |

## Architecture Macro Fixes
| File | Line | Change |
|------|------|--------|
| ... | ... | ... |

## Sync Function Fixes
| File | Count | Notes |
|------|-------|-------|
| ... | ... | ... |

## Other Fixes
- <description>

## Build Status
- [ ] Compiles without errors
- [ ] Compiles without warnings

## Validation Status
- [ ] Runs without crashes
- [ ] Output matches CUDA reference (within tolerance)
- [ ] All tests pass
- Numerical differences: <if any, describe>

## Remaining Concerns
- <any areas that may need runtime testing or performance tuning>
```

If everything passes, congratulate the user and suggest running benchmarks to compare performance. If there are correctness issues, investigate and iterate.
