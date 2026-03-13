# CUDA to ROCm Porting Agent

This is a coding agent which ports CUDA project into ROCm/hip.

## Overview

It was created with claude code and tested with 2 sample applications from CUDA samples:

* nbody
* smokeParticles

Please see more details in ./CLAUDE.md.

## Porting Steps

To use it with claude code, following the steps:

1. copy the project to be ported to source subdirectory
2. /port-analyze source/
3. /port-translate
4. /port-fix
5. /port-profile

Last step will produce the performance profiling (rocprofv3) report and place it under report/ subdirectory.
