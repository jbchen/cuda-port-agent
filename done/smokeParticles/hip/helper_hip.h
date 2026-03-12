/*
 * HIP helper functions for error checking and device selection.
 * Replaces NVIDIA's helper_cuda.h for HIP-ported projects.
 */

#ifndef HELPER_HIP_H_
#define HELPER_HIP_H_

#include <hip/hip_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef EXIT_WAIVED
#define EXIT_WAIVED 2
#endif

// Error checking macro
#define checkHipErrors(val) __checkHipErrors((val), #val, __FILE__, __LINE__)

template <typename T>
inline void __checkHipErrors(T result, const char *func, const char *file, int line) {
    if (result != hipSuccess) {
        fprintf(stderr, "HIP error at %s:%d code=%d(%s) \"%s\" \n",
                file, line, static_cast<int>(result),
                hipGetErrorString(static_cast<hipError_t>(result)), func);
        exit(EXIT_FAILURE);
    }
}

#define getLastHipError(msg) __getLastHipError(msg, __FILE__, __LINE__)

inline void __getLastHipError(const char *errorMessage, const char *file, int line) {
    hipError_t err = hipGetLastError();
    if (err != hipSuccess) {
        fprintf(stderr, "%s(%d): getLastHipError() HIP error: %s : (%d) %s.\n",
                file, line, errorMessage, static_cast<int>(err),
                hipGetErrorString(err));
        exit(EXIT_FAILURE);
    }
}

// Float to int rounding - replacement for ftoi() from helper_cuda.h
inline int ftoi(float value) {
    return (value >= 0 ? static_cast<int>(value + 0.5)
                       : static_cast<int>(value - 0.5));
}

// Device selection - replacement for findCudaDevice()
inline int findHipDevice(int argc, const char **argv) {
    int devID = 0;
    hipDeviceProp_t deviceProp;

    // Check if a specific device was requested via command line
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "device=", 7) == 0) {
            devID = atoi(argv[i] + 7);
            break;
        }
    }

    checkHipErrors(hipSetDevice(devID));
    checkHipErrors(hipGetDeviceProperties(&deviceProp, devID));
    printf("GPU Device %d: \"%s\" with compute capability %d.%d\n\n",
           devID, deviceProp.name, deviceProp.major, deviceProp.minor);

    return devID;
}

#endif  // HELPER_HIP_H_
