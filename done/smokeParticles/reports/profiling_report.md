# Profiling Report: smokeParticles

## Environment
- GPU: AMD Instinct MI250X/MI250 (single GCD, gfx90a)
- ROCm: 7.1.1
- Profiler: rocprofv3
- Application: smokeParticles --qatest (100 simulation steps, 262144 particles)

## Kernel Summary

| Kernel | Calls | Total Time (ms) | Avg Time (ms) | % of GPU Time |
|--------|-------|-----------------|----------------|---------------|
| `integrate_functor` (thrust::for_each) | 100 | 135.294 | 1.353 | 98.84% |
| `__amd_rocclr_copyBuffer` | 304 | 1.580 | 0.005 | 1.15% |
| `__amd_rocclr_initHeap` | 1 | 0.010 | 0.010 | <0.01% |

The `integrate_functor` kernel (particle integration with 3D noise lookup) completely dominates execution time at 98.84%.

Note: `calcDepth_functor` and `sortParticles` did not execute because depth sorting is not active in qatest mode. In the full rendering loop, these would contribute additional GPU time.

## Memory Transfer Summary

| Direction | Calls | Total Size | Total Time (ms) | Effective BW |
|-----------|-------|------------|-----------------|--------------|
| Host → Device | 3 | 6,291,456 B (~6 MB) | 0.457 | ~13.1 GB/s |
| Device → Host | 2 | 2,097,152 B (~2 MB) | 0.161 | ~12.4 GB/s |

Memory transfers are negligible (<0.5% of total time). The 6MB H→D includes noise buffer data (4MB) and initial particle data (2×1MB).

## Hot Kernel Analysis: `integrate_functor`

### Launch Configuration
- Grid: 65,536 blocks × 1 × 1
- Block: 256 threads × 1 × 1
- Total threads: 16,777,216
- Waves per block: 4 (256 threads / 64 threads-per-wave)
- Total waves: 262,144

### Register Usage
| Register Type | Count | Impact |
|---------------|-------|--------|
| Arch VGPRs | 52 | 4 waves/SIMD limit |
| Accum VGPRs | 76 | 3 waves/SIMD limit (**bottleneck**) |
| SGPRs | 112 | Not limiting |
| Scratch (spill) | 1,952 bytes/thread | High register spills |
| LDS | 0 bytes | Not used |

### Instruction Mix (per wave, averaged over 100 dispatches)
| Instruction Type | Per Wave | Notes |
|-----------------|----------|-------|
| VALU (vector ALU) | 15,446 | Main compute |
| SALU (scalar ALU) | 18,112 | Address computation, loop control |
| LDS (shared memory) | 3,369 | Thrust internals (not explicit LDS) |
| VMEM read (global load) | 2,701 | Noise buffer + particle data reads |
| VMEM write (global store) | 2,488 | Particle position/velocity writes |
| VALU:SALU ratio | **0.85:1** | More SALU than VALU (unusual) |

### Compute Utilization
- VALU utilization: **22.8%** (fraction of busy SIMD cycles with VALU active)
- Average active threads per VALU instruction: **34.7 / 64** (54.2%)
- LDS stall rate: 0.00%

### Occupancy
- Max occupancy (AGPR-limited): **3 waves/SIMD** = 37.5% theoretical
- Achieved occupancy: Limited by AGPR count (76 AGPRs → 512/128=4, but accum rounds to 3)
- Scratch spills: 1,952 bytes/thread indicates significant register pressure

### Bottleneck Analysis

**Primary bottleneck: MEMORY BOUND with HIGH REGISTER PRESSURE**

1. **Low VALU utilization (22.8%)**: The GPU spends most of its time waiting for memory operations. The trilinear noise lookup performs 8 random global memory reads per particle from the noise buffer, creating a scatter-read pattern that misses L1/L2 cache.

2. **High register pressure**: 52 arch + 76 accum VGPRs = 128 total. This limits occupancy to 3 waves/SIMD (37.5%), preventing the GPU from hiding memory latency with concurrent wavefronts.

3. **Low thread utilization (34.7/64)**: Only ~54% of lanes are active on average. This suggests control flow divergence (the `age < lifetime` branch) or inefficient predication.

4. **Inverted VALU:SALU ratio (0.85:1)**: More scalar than vector instructions indicates excessive address arithmetic from the trilinear interpolation (8 index calculations, modular arithmetic for wrapping).

**Not a bottleneck:**
- LDS: Zero stall cycles, unused by the kernel itself
- Memory transfers: Negligible (<0.5% of total time)

## Optimization Recommendations

### Priority 1: Add `__launch_bounds__` to reduce register pressure
**Expected impact: ~2× occupancy improvement**

The thrust::for_each kernel has no launch bounds, causing the compiler to use excessive registers. Since the kernel is launched with block size 256:

```cpp
// In particles_kernel_device.cuh, integrate_functor::operator():
// Cannot directly add launch_bounds to a thrust functor, but can
// influence via compiler flags:
// hipcc --offload-arch=gfx90a -mllvm -amdgpu-vgpr-limit=64
```

Alternatively, convert the thrust::for_each to a raw HIP kernel with explicit `__launch_bounds__(256, 8)` to target 8 waves/SIMD. This would force the compiler to reduce AGPR usage from 76 to ~4, dramatically improving occupancy.

### Priority 2: Optimize trilinear noise interpolation
**Expected impact: ~30-50% fewer VMEM reads, reduced SALU**

The current implementation does 8 independent global memory reads per particle for trilinear interpolation with modular arithmetic for wrapping:

```cpp
// Current: 8 random reads + modulo operations
int ix0 = (int)floorf(fx) % nb.w;
int ix1 = (ix0 + 1) % nb.w;
// ... 8 reads from nb.data[...]
```

Optimizations:
- **Precompute wrapped indices** using bitwise AND if dimensions are power-of-2 (they are: 64×64×64): `ix0 & (w-1)` instead of `ix0 % w`
- **Cache adjacent noise values** by reading 2×2×2 block into registers before interpolation
- **Use LDS** to share noise data across threads in the same block that sample nearby coordinates

### Priority 3: Replace thrust::for_each with raw HIP kernel
**Expected impact: Better compiler control, enable launch_bounds**

Thrust's `for_each` wraps the functor in multiple template layers, inflating register usage and preventing direct use of `__launch_bounds__`. A raw kernel would:
- Allow explicit `__launch_bounds__(256, 8)`
- Reduce SALU overhead from thrust iterator machinery
- Allow fine-grained control over register allocation

### Priority 4: Power-of-2 noise dimensions
**Expected impact: ~15% fewer SALU instructions**

The noise buffer dimensions (w, h, d) should be power-of-2 for efficient index wrapping:
```cpp
// Replace modulo with bitwise AND
int ix0 = (int)floorf(fx) & (nb.w - 1);  // w must be power-of-2
```
This eliminates expensive integer division/modulo in the inner loop.

## Raw Counter Data

### Pass 1: Instruction Counters (per CU, averaged over 100 dispatches)
| Counter | Value |
|---------|-------|
| SQ_WAVES | 128 |
| SQ_INSTS_VALU | 1,977,088 |
| SQ_INSTS_SALU | 2,318,336 |
| SQ_INSTS_LDS | 431,232 |
| SQ_INSTS_FLAT_LDS_ONLY | 0 |
| SQ_WAIT_INST_LDS | 0 |

### Pass 2: Compute Cycles (per CU, averaged over 100 dispatches)
| Counter | Value |
|---------|-------|
| SQ_ACTIVE_INST_VALU | 1,979,392 |
| SQ_THREAD_CYCLES_VALU | 68,597,504 |
| SQ_BUSY_CYCLES | 2,169,800 |
| SQ_INSTS_VMEM_RD | 345,728 |
| SQ_INSTS_VMEM_WR | 318,464 |
