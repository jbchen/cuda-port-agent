// HIP compatibility replacement for NVIDIA's helper_cuda.h
// Provides error checking macros and device selection utilities.

#ifndef HELPER_CUDA_H
#define HELPER_CUDA_H

#include <hip/hip_runtime.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef checkCudaErrors
#define checkCudaErrors(val) __checkHipErrors((val), #val, __FILE__, __LINE__)

inline void __checkHipErrors(hipError_t err, const char *func, const char *file, int line)
{
    if (err != hipSuccess) {
        fprintf(stderr, "HIP error at %s:%d code=%d(%s) \"%s\" \n", file, line, static_cast<int>(err),
                hipGetErrorString(err), func);
        exit(EXIT_FAILURE);
    }
}
#endif

#ifndef getLastCudaError
#define getLastCudaError(msg) __getLastHipError(msg, __FILE__, __LINE__)

inline void __getLastHipError(const char *errorMessage, const char *file, const int line)
{
    hipError_t err = hipGetLastError();
    if (err != hipSuccess) {
        fprintf(stderr, "%s(%i) : getLastHipError() HIP error : %s : (%d) %s.\n", file, line, errorMessage,
                static_cast<int>(err), hipGetErrorString(err));
        exit(EXIT_FAILURE);
    }
}
#endif

// Device selection: parse -device=N from command line, or pick device 0
inline int findCudaDevice(int argc, const char **argv)
{
    int devID = 0;
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-device=", 8) == 0 || strncmp(argv[i], "--device=", 9) == 0) {
            const char *p = strchr(argv[i], '=');
            if (p) devID = atoi(p + 1);
        }
    }
    hipDeviceProp_t props;
    checkCudaErrors(hipSetDevice(devID));
    checkCudaErrors(hipGetDeviceProperties(&props, devID));
    printf("> Using HIP Device [%d]: %s\n", devID, props.name);
    return devID;
}

// Command line helpers (replacing helper_string.h / helper_functions.h utilities)
inline bool checkCmdLineFlag(int argc, const char **argv, const char *flag)
{
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        // skip leading dashes
        while (*arg == '-') arg++;
        // check if the flag matches (exact or up to '=')
        size_t len = strlen(flag);
        if (strncmp(arg, flag, len) == 0 && (arg[len] == '\0' || arg[len] == '=')) {
            return true;
        }
    }
    return false;
}

inline int getCmdLineArgumentInt(int argc, const char **argv, const char *flag)
{
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        while (*arg == '-') arg++;
        size_t len = strlen(flag);
        if (strncmp(arg, flag, len) == 0 && arg[len] == '=') {
            return atoi(arg + len + 1);
        }
    }
    return 0;
}

inline bool getCmdLineArgumentString(int argc, const char **argv, const char *flag, char **val)
{
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        while (*arg == '-') arg++;
        size_t len = strlen(flag);
        if (strncmp(arg, flag, len) == 0 && arg[len] == '=') {
            *val = const_cast<char *>(arg + len + 1);
            return true;
        }
    }
    return false;
}

#endif // HELPER_CUDA_H
