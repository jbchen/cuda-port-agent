# Port Profile

Profile the HIP application with rocprofv3, identify hot kernels, and produce an optimization report.

## Input

The user may provide arguments to pass to the application: $ARGUMENTS

If no argument is given, profile the default benchmark run of the built application in `hip/build/`.

## Prerequisites

- The project must already be built in `hip/build/`
- If it isn't built, tell the user to run `/port-translate` first
- Profiler: `/opt/rocm-7.1.1/bin/rocprofv3`

## Workflow

### 1. Verify the binary exists

```bash
ls -la hip/build/nbody  # or whatever the executable is called
```

If the binary doesn't exist, tell the user to build first.

### 2. Run kernel trace profiling

Collect kernel dispatch traces with timing and a summary. This identifies the hot kernels.

```bash
mkdir -p reports/profile

/opt/rocm-7.1.1/bin/rocprofv3 \
    --kernel-trace \
    --memory-copy-trace \
    --stats \
    -S \
    -u msec \
    -d reports/profile/trace \
    -o trace \
    -- hip/build/<executable> -benchmark -numbodies=32768 2>&1 | tee reports/profile/trace_stdout.log
```

The `--stats` flag generates per-kernel statistics (count, total time, avg, min, max). The `-S` flag prints a summary to stderr at the end.

Review the trace output CSV files:
```bash
# Kernel dispatch stats
cat reports/profile/trace/*kernel_dispatch*.csv
# Memory copy stats
cat reports/profile/trace/*memory_copy*.csv
```

Parse the kernel stats to find:
- Which kernels take the most total time (hot kernels)
- Kernel launch counts
- Average kernel duration
- Memory copy overhead vs compute time

### 3. Collect hardware performance counters for hot kernels

Based on the hot kernels identified in step 2, collect detailed PMC counters. Use a counter input file for organized collection.

Create a counter collection file:

```bash
cat > reports/profile/counters.txt << 'EOF'
pmc: SQ_WAVES SQ_INSTS_VALU SQ_INSTS_SALU SQ_INSTS_LDS SQ_INSTS_GDS SQ_INSTS_FLAT_LDS_ONLY SQ_WAIT_INST_LDS
pmc: SQ_ACTIVE_INST_VALU SQ_THREAD_CYCLES_VALU SQ_BUSY_CYCLES SQ_INSTS_VMEM_RD SQ_INSTS_VMEM_WR
pmc: TCP_TOTAL_READ_sum TCP_TOTAL_WRITE_sum TCP_TOTAL_CACHE_ACCESSES_sum TCP_TCC_READ_REQ_sum TCP_TCC_WRITE_REQ_sum TCP_TOTAL_ACCESSES_sum
pmc: TCC_HIT_sum TCC_MISS_sum TCC_EA_RDREQ_sum TCC_EA_WRREQ_sum TCC_EA_RDREQ_32B_sum
pmc: SQ_INSTS_FLAT SQ_INSTS_SMEM SQ_WAIT_ANY SQ_WAVES_RESTORED SQ_WAVES_SAVED
pmc: FETCH_SIZE TA_FLAT_READ_WAVEFRONTS_sum TA_FLAT_WRITE_WAVEFRONTS_sum WRITE_SIZE
EOF
```

Then collect:

```bash
/opt/rocm-7.1.1/bin/rocprofv3 \
    -i reports/profile/counters.txt \
    -d reports/profile/counters \
    -o counters \
    -- hip/build/<executable> -benchmark -numbodies=32768 2>&1 | tee reports/profile/counters_stdout.log
```

Read and analyze the counter output:
```bash
cat reports/profile/counters/*.csv
```

If counter collection fails (some counters may not be available on all architectures), reduce the counter list and retry. Remove counters that caused errors and re-run.

### 4. Compute derived metrics

From the raw counters, compute these key metrics for each hot kernel:

**Compute utilization:**
- VALU utilization = `SQ_ACTIVE_INST_VALU / (SQ_BUSY_CYCLES * NUM_SIMDS)`
- Average active threads = `SQ_THREAD_CYCLES_VALU / SQ_ACTIVE_INST_VALU` (out of 64 for gfx90a wavefront)
- VALU:SALU ratio = `SQ_INSTS_VALU / SQ_INSTS_SALU`

**Memory analysis:**
- L1 cache hit rate = `1 - (TCP_TCC_READ_REQ_sum / TCP_TOTAL_READ_sum)` (if available)
- L2 cache hit rate = `TCC_HIT_sum / (TCC_HIT_sum + TCC_MISS_sum)`
- Bandwidth = `(FETCH_SIZE + WRITE_SIZE) * 32 / kernel_time_ns` (GB/s)

**LDS (shared memory) analysis:**
- LDS instructions per wave = `SQ_INSTS_LDS / SQ_WAVES`
- LDS stall rate = `SQ_WAIT_INST_LDS / SQ_BUSY_CYCLES`

**Occupancy estimate:**
- Waves per CU = `SQ_WAVES / (kernel_time * NUM_CUs * GPU_FREQ)`

### 5. Optionally run with different configurations

If the default block size might not be optimal, profile with different configurations:

```bash
# Try different body counts to see scaling
for N in 8192 16384 32768 65536; do
    echo "=== N=$N ==="
    /opt/rocm-7.1.1/bin/rocprofv3 \
        --kernel-trace -S -u msec \
        -d reports/profile/scaling_${N} \
        -o scaling \
        -- hip/build/<executable> -benchmark -numbodies=$N 2>&1
done
```

### 6. Produce the profiling report

Save to `reports/profiling_report.md`:

```markdown
# Profiling Report

## Environment
- GPU: <device name>
- ROCm: <version>
- Profiler: rocprofv3

## Kernel Summary

| Kernel | Calls | Total Time | Avg Time | % of GPU Time |
|--------|-------|------------|----------|---------------|
| ... | ... | ... | ... | ... |

## Memory Transfer Summary

| Direction | Calls | Total Size | Total Time | Bandwidth |
|-----------|-------|------------|------------|-----------|
| H→D | ... | ... | ... | ... |
| D→H | ... | ... | ... | ... |

## Hot Kernel Analysis: <kernel_name>

### Timing
- Average duration: X ms
- Launch configuration: gridDim=..., blockDim=...

### Compute
- VALU instructions per wave: X
- SALU instructions per wave: X
- VALU utilization: X%
- Average active threads per wave: X / 64

### Memory
- VMEM reads per wave: X
- VMEM writes per wave: X
- LDS instructions per wave: X
- L1 cache hit rate: X%
- L2 cache hit rate: X%
- LDS stall rate: X%

### Bottleneck Analysis
<Based on the metrics, identify the primary bottleneck:>
- **Compute bound**: High VALU utilization, low memory stalls
- **Memory bound**: Low VALU utilization, high memory traffic
- **LDS bound**: High LDS stall rate
- **Latency bound**: Low occupancy, many stalls
- **Launch overhead**: Very short kernels with many launches

## Optimization Recommendations

### Priority 1: <highest impact recommendation>
<Detailed description with specific code changes>

### Priority 2: <next recommendation>
...

## Scaling Analysis (if run)

| Bodies | Kernel Time | GFLOP/s | Efficiency |
|--------|-------------|---------|------------|
| 8192 | ... | ... | ... |
| 16384 | ... | ... | ... |
| 32768 | ... | ... | ... |
| 65536 | ... | ... | ... |
```

### 7. Present findings

After generating the report, present the key findings to the user:
- Which kernel is the hottest and what percentage of GPU time it takes
- What the primary bottleneck is (compute, memory, latency, etc.)
- Top 3 specific optimization recommendations with expected impact
- Whether the kernel is making good use of the MI250X hardware

If the kernel is memory-bound, suggest:
- Coalescing improvements
- Shared memory (LDS) usage optimization
- Data layout changes (AoS → SoA)
- Reducing unnecessary memory traffic

If the kernel is compute-bound, suggest:
- Instruction mix improvements (replace expensive ops)
- Loop unrolling tuning
- Occupancy improvements
- Using packed FP32 (MFMA) if applicable

If the kernel is latency-bound, suggest:
- Increasing occupancy (reduce register usage, reduce shared memory)
- Increasing problem size
- Kernel fusion to amortize launch overhead
