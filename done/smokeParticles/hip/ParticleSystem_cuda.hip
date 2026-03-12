#include "hip/hip_runtime.h"
/* Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
This file contains simple wrapper functions that call the HIP kernels
*/

// includes
#include <cstdio>
#include <cstdlib>
#include <hip/hip_runtime_api.h>
#include <string.h>
#include "helper_hip.h"

#include "ParticleSystem.cuh"
#include "particles_kernel_device.cuh"
#include "thrust/device_ptr.h"
#include "thrust/for_each.h"
#include "thrust/iterator/zip_iterator.h"
#include "thrust/sort.h"

extern "C"
{

    // Host-side copy of noise buffer info for passing to functors
    static NoiseBuffer hostNoiseBuffer = {nullptr, 0, 0, 0};

    void setParameters(SimParams *hostParams)
    {
        // copy parameters to constant memory
        checkHipErrors(hipMemcpyToSymbol(HIP_SYMBOL(cudaParams), hostParams, sizeof(SimParams)));
    }

    // Round a / b to nearest higher integer value
    int iDivUp(int a, int b) { return (a % b != 0) ? (a / b + 1) : (a / b); }

    // compute grid and thread block size for a given number of elements
    void computeGridSize(int n, int blockSize, int &numBlocks, int &numThreads)
    {
        numThreads = min(blockSize, n);
        numBlocks  = iDivUp(n, numThreads);
    }

    inline float frand() { return rand() / (float)RAND_MAX; }

    // create 3D noise buffer (replaces texture objects unsupported on MI250X)
    void createNoiseTexture(int w, int h, int d)
    {
        size_t elements = (size_t)w * h * d;

        float4 *volumeData = (float4 *)malloc(elements * sizeof(float4));

        for (size_t i = 0; i < elements; i++) {
            volumeData[i] = make_float4(
                frand() * 2.0f - 1.0f,
                frand() * 2.0f - 1.0f,
                frand() * 2.0f - 1.0f,
                frand() * 2.0f - 1.0f);
        }

        float4 *d_noiseData;
        checkHipErrors(hipMalloc(&d_noiseData, elements * sizeof(float4)));
        checkHipErrors(hipMemcpy(d_noiseData, volumeData, elements * sizeof(float4), hipMemcpyHostToDevice));

        free(volumeData);

        hostNoiseBuffer.data = d_noiseData;
        hostNoiseBuffer.w = w;
        hostNoiseBuffer.h = h;
        hostNoiseBuffer.d = d;

        // Also copy to device __device__ variable
        checkHipErrors(hipMemcpyToSymbol(HIP_SYMBOL(noiseBuffer), &hostNoiseBuffer, sizeof(NoiseBuffer)));
    }

    void
    integrateSystem(float4 *oldPos, float4 *newPos, float4 *oldVel, float4 *newVel, float deltaTime, int numParticles)
    {
        thrust::device_ptr<float4> d_newPos(newPos);
        thrust::device_ptr<float4> d_newVel(newVel);
        thrust::device_ptr<float4> d_oldPos(oldPos);
        thrust::device_ptr<float4> d_oldVel(oldVel);

        thrust::for_each(
            thrust::make_zip_iterator(thrust::make_tuple(d_newPos, d_newVel, d_oldPos, d_oldVel)),
            thrust::make_zip_iterator(thrust::make_tuple(
                d_newPos + numParticles, d_newVel + numParticles, d_oldPos + numParticles, d_oldVel + numParticles)),
            integrate_functor(deltaTime, hostNoiseBuffer));
    }

    void calcDepth(float4 *pos,
                   float  *keys,    // output
                   uint   *indices, // output
                   float3  sortVector,
                   int     numParticles)
    {
        thrust::device_ptr<float4> d_pos(pos);
        thrust::device_ptr<float>  d_keys(keys);
        thrust::device_ptr<uint>   d_indices(indices);

        thrust::for_each(thrust::make_zip_iterator(thrust::make_tuple(d_pos, d_keys)),
                         thrust::make_zip_iterator(thrust::make_tuple(d_pos + numParticles, d_keys + numParticles)),
                         calcDepth_functor(sortVector));

        thrust::sequence(d_indices, d_indices + numParticles);
    }

    void sortParticles(float *sortKeys, uint *indices, uint numParticles)
    {
        thrust::sort_by_key(thrust::device_ptr<float>(sortKeys),
                            thrust::device_ptr<float>(sortKeys + numParticles),
                            thrust::device_ptr<uint>(indices));
    }

} // extern "C"
