# Port Analyze

Scan a CUDA project and produce a porting feasibility report.

## Input

The user will provide a path to a CUDA project: $ARGUMENTS

If no path is given, assume the CUDA source is in `source/`.

## Workflow

### 1. Inventory source files

Recursively find all CUDA-related files:

```bash
find <project_path> -name "*.cu" -o -name "*.cuh" -o -name "*.h" -o -name "*.hpp" -o -name "*.cpp" | head -100
```

Count lines of CUDA code:
```bash
find <project_path> \( -name "*.cu" -o -name "*.cuh" \) -exec wc -l {} + | tail -1
```

<!--
### 2. Run hipify-perl in dry-run / examine mode

Use hipify-perl's examine mode to get a conversion statistics summary without modifying files:

```bash
hipexamine-perl.sh <project_path>
```

This reports: number of CUDA→HIP refs found, unconverted calls, warnings, and per-API-category breakdown.
-->

### 3. Scan for hard porting problems

Search for patterns that need manual intervention. For each file, report findings:

**Warp-size assumptions:**
```bash
grep -rn "0xFFFFFFFF\|warpSize\|__shfl\|__ballot\|__any_sync\|__all_sync\|__activemask\|lane_id\|WARP_SIZE\|/\s*32\b" --include="*.cu" --include="*.cuh"
```

**Hardcoded architecture checks:**
```bash
grep -rn "__CUDA_ARCH__\|SM_[0-9]\|sm_[0-9]" --include="*.cu" --include="*.cuh" --include="*.h"
```

**Inline PTX assembly:**
```bash
grep -rn "asm\s*(" --include="*.cu" --include="*.cuh"
```

**Cooperative groups:**
```bash
grep -rn "cooperative_groups\|grid_group\|multi_grid\|this_grid" --include="*.cu" --include="*.cuh"
```

**CUDA driver API:**
```bash
grep -rn "cuModule\|cuFunction\|cuCtx\|cuDevice[^S]" --include="*.cu" --include="*.cuh" --include="*.cpp"
```

**Library usage:**
```bash
grep -rn "cublas\|cufft\|curand\|cusparse\|cusolver\|cudnn\|nccl\|thrust\|cub::" --include="*.cu" --include="*.cuh" --include="*.cpp" --include="*.h" -i
```

**Texture / surface objects:**
```bash
grep -rn "texture<\|surface<\|tex1D\|tex2D\|tex3D\|cudaCreateTextureObject\|cudaTextureObject_t" --include="*.cu" --include="*.cuh"
```

**Dynamic parallelism (kernels launching kernels):**
```bash
grep -rn "<<<.*>>>" --include="*.cu" | grep -v "main\|host\|test"
```

### 4. Analyze the build system

Check what build system is used:
```bash
ls <project_path>/CMakeLists.txt <project_path>/Makefile <project_path>/Makefile.* 2>/dev/null
```

If CMake, scan for CUDA-specific cmake calls:
```bash
grep -rn "enable_language.*CUDA\|find_package.*CUDA\|CUDA_ARCHITECTURES\|cuda_add_executable\|cuda_add_library" --include="CMakeLists.txt" --include="*.cmake"
```

If Makefile, scan for nvcc usage:
```bash
grep -rn "nvcc\|NVCC\|CUDA_PATH\|cuda_home" --include="Makefile*"
```

### 5. Check for external dependencies

```bash
grep -rn "#include" --include="*.cu" --include="*.cuh" | grep -v "cuda\|std\|cstdlib\|cstdio\|iostream\|vector\|string\|math\|assert\|float\|limits\|algorithm" | sort -u
```

### 6. Produce the porting report

Save to `reports/porting_analysis.md` with this structure:

```markdown
# Porting Analysis: <project_name>

## Summary
- Total CUDA files: X (.cu) + Y (.cuh)
- Total CUDA LOC: Z
- hipify auto-conversion rate: N% (from hipexamine output)
- Estimated effort: Easy / Medium / Hard
- Estimated time: X hours

## File Inventory
| File | LOC | Auto-convertible | Manual fixes needed |
|------|-----|-------------------|---------------------|
| ... | ... | ... | ... |

## Auto-Convertible (hipify handles these)
- API call renames: X occurrences
- Include swaps: Y occurrences
- Type renames: Z occurrences

## Manual Fixes Required

### Critical (will cause incorrect results if missed)
- [ ] Warp size assumptions: <list files and lines>
- [ ] Ballot/shuffle mask width: <list files and lines>

### Required (will cause build failures)
- [ ] Architecture macro checks: <list files and lines>
- [ ] Build system changes: <description>
- [ ] Library mapping: <list libraries>

### Optional (for performance)
- [ ] Launch bounds tuning
- [ ] Shared memory bank conflict patterns
- [ ] Occupancy optimization

## Unsupported Features (if any)
- <list any features with no HIP equivalent>

## Library Dependencies
| CUDA Library | HIP Equivalent | Status |
|-------------|----------------|--------|
| ... | ... | Available / Missing / Partial |

## Build System
- Current: <CMake/Make/other>
- Changes needed: <description>

## Recommended Porting Order
1. <file> — <reason to start here>
2. <file> — <depends on #1>
...
```

Present the report to the user and wait for approval before proceeding with translation.
