// HIP compatibility replacement for NVIDIA's helper_functions.h
// Provides timer utilities and command-line helpers.

#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

#include "helper_cuda.h"
#include <chrono>

// Timer interface compatible with NVIDIA SDK's StopWatchInterface
class StopWatchInterface
{
public:
    StopWatchInterface() : running_(false), elapsed_(0.0f) {}

    void start()
    {
        running_    = true;
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    void stop()
    {
        if (running_) {
            auto end = std::chrono::high_resolution_clock::now();
            elapsed_ += std::chrono::duration<float, std::milli>(end - start_time_).count();
            running_ = false;
        }
    }

    void reset()
    {
        elapsed_ = 0.0f;
        if (running_) {
            start_time_ = std::chrono::high_resolution_clock::now();
        }
    }

    float getTime() const
    {
        if (running_) {
            auto now = std::chrono::high_resolution_clock::now();
            return elapsed_ + std::chrono::duration<float, std::milli>(now - start_time_).count();
        }
        return elapsed_;
    }

private:
    bool                                                running_;
    float                                               elapsed_;
    std::chrono::high_resolution_clock::time_point      start_time_;
};

inline void sdkCreateTimer(StopWatchInterface **timer) { *timer = new StopWatchInterface(); }
inline void sdkDeleteTimer(StopWatchInterface **timer)
{
    delete *timer;
    *timer = nullptr;
}
inline void  sdkStartTimer(StopWatchInterface **timer) { (*timer)->start(); }
inline void  sdkStopTimer(StopWatchInterface **timer) { (*timer)->stop(); }
inline void  sdkResetTimer(StopWatchInterface **timer) { (*timer)->reset(); }
inline float sdkGetTimerValue(StopWatchInterface **timer) { return (*timer)->getTime(); }

#endif // HELPER_FUNCTIONS_H
