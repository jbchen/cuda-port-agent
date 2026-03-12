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
 * CUDA Device code for particle simulation.
 */

#ifndef _PARTICLES_KERNEL_H_
#define _PARTICLES_KERNEL_H_

#include <hip/hip_runtime.h>
#include <hip/hip_vector_types.h>
#include "particles_kernel.cuh"

// Vector math helpers not provided by HIP runtime
inline __host__ __device__ float dot(float3 a, float3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline __host__ __device__ float4 make_float4(float3 a, float w)
{
    return make_float4(a.x, a.y, a.z, w);
}

#include "thrust/device_ptr.h"
#include "thrust/for_each.h"
#include "thrust/iterator/zip_iterator.h"
#include "thrust/sort.h"

// Buffer-based 3D noise (replaces texture objects unsupported on MI250X gfx90a)
struct NoiseBuffer {
    float4 *data;
    int w, h, d;
};

__device__ NoiseBuffer noiseBuffer;
// simulation parameters
__constant__ SimParams cudaParams;

// look up in 3D noise buffer with trilinear interpolation and wrapping
__device__ float3 noise3D(float3 p, const NoiseBuffer &nb)
{
    // Wrap normalized coordinates to [0,1)
    float u = p.x - floorf(p.x);
    float v = p.y - floorf(p.y);
    float w = p.z - floorf(p.z);

    // Scale to grid coordinates
    float fx = u * nb.w;
    float fy = v * nb.h;
    float fz = w * nb.d;

    int ix0 = (int)floorf(fx);
    int iy0 = (int)floorf(fy);
    int iz0 = (int)floorf(fz);

    float dx = fx - ix0;
    float dy = fy - iy0;
    float dz = fz - iz0;

    // Wrap indices
    ix0 = ix0 % nb.w; int ix1 = (ix0 + 1) % nb.w;
    iy0 = iy0 % nb.h; int iy1 = (iy0 + 1) % nb.h;
    iz0 = iz0 % nb.d; int iz1 = (iz0 + 1) % nb.d;

    // Trilinear interpolation
    #define NOISE_IDX(x,y,z) ((z) * nb.w * nb.h + (y) * nb.w + (x))
    float4 c000 = nb.data[NOISE_IDX(ix0, iy0, iz0)];
    float4 c100 = nb.data[NOISE_IDX(ix1, iy0, iz0)];
    float4 c010 = nb.data[NOISE_IDX(ix0, iy1, iz0)];
    float4 c110 = nb.data[NOISE_IDX(ix1, iy1, iz0)];
    float4 c001 = nb.data[NOISE_IDX(ix0, iy0, iz1)];
    float4 c101 = nb.data[NOISE_IDX(ix1, iy0, iz1)];
    float4 c011 = nb.data[NOISE_IDX(ix0, iy1, iz1)];
    float4 c111 = nb.data[NOISE_IDX(ix1, iy1, iz1)];
    #undef NOISE_IDX

    // Interpolate along x
    float4 c00 = c000 * (1-dx) + c100 * dx;
    float4 c10 = c010 * (1-dx) + c110 * dx;
    float4 c01 = c001 * (1-dx) + c101 * dx;
    float4 c11 = c011 * (1-dx) + c111 * dx;

    // Interpolate along y
    float4 c0 = c00 * (1-dy) + c10 * dy;
    float4 c1 = c01 * (1-dy) + c11 * dy;

    // Interpolate along z
    float4 n = c0 * (1-dz) + c1 * dz;

    return make_float3(n.x, n.y, n.z);
}

// integrate particle attributes
struct integrate_functor
{
    float       deltaTime;
    NoiseBuffer nb;

    __host__ __device__ integrate_functor(float delta_time, NoiseBuffer noise_buf)
        : deltaTime(delta_time)
        , nb(noise_buf)
    {
    }

    template <typename Tuple> __device__ void operator()(Tuple t)
    {
        volatile float4 posData = thrust::get<2>(t);
        volatile float4 velData = thrust::get<3>(t);

        float3 pos = make_float3(posData.x, posData.y, posData.z);
        float3 vel = make_float3(velData.x, velData.y, velData.z);

        // update particle age
        float age      = posData.w;
        float lifetime = velData.w;

        if (age < lifetime) {
            age += deltaTime;
        }
        else {
            age = lifetime;
        }

        // apply accelerations
        vel += cudaParams.gravity * deltaTime;

        // apply procedural noise
        float3 noise = noise3D(pos * cudaParams.noiseFreq + cudaParams.time * cudaParams.noiseSpeed, nb);
        vel += noise * cudaParams.noiseAmp;

        // new position = old position + velocity * deltaTime
        pos += vel * deltaTime;

        vel *= cudaParams.globalDamping;

        // store new position and velocity
        thrust::get<0>(t) = make_float4(pos, age);
        thrust::get<1>(t) = make_float4(vel, velData.w);
    }
};

struct calcDepth_functor
{
    float3 sortVector;

    __host__ __device__ calcDepth_functor(float3 sort_vector)
        : sortVector(sort_vector)
    {
    }

    template <typename Tuple> __host__ __device__ void operator()(Tuple t)
    {
        volatile float4 p   = thrust::get<0>(t);
        float           key = -dot(make_float3(p.x, p.y, p.z),
                         sortVector); // project onto sort vector
        thrust::get<1>(t)   = key;
    }
};

#endif
