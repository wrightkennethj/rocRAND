// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <stdio.h>
#include <gtest/gtest.h>

#include <vector>
#include <cmath>

#include <hip/hip_runtime.h>

#define FQUALIFIERS __forceinline__ __host__ __device__
#include <rocrand_kernel.h>

#define HIP_CHECK(x) ASSERT_EQ(x, hipSuccess)

template <class GeneratorState>
__global__
void rocrand_init_kernel(GeneratorState * states,
                         const size_t states_size,
                         unsigned long long seed,
                         unsigned long long offset)
{
    const unsigned int state_id = hipBlockIdx_x * hipBlockDim_x + hipThreadIdx_x;
    const unsigned int subsequence = state_id;
    if(state_id < states_size)
    {
        GeneratorState state;
        rocrand_init(seed, subsequence, offset, &state);
        states[state_id] = state;
    }
}

template <class GeneratorState>
__global__
void rocrand_kernel(unsigned int * output, const size_t size)
{
    const unsigned int state_id = hipBlockIdx_x * hipBlockDim_x + hipThreadIdx_x;
    const unsigned int global_size = hipGridDim_x * hipBlockDim_x;

    GeneratorState state;
    const unsigned int subsequence = state_id;
    rocrand_init(0, subsequence, 0, &state);

    unsigned int index = state_id;
    while(index < size)
    {
        output[index] = rocrand(&state);
        index += global_size;
    }
}

template <class GeneratorState>
__global__
void rocrand_uniform_kernel(float * output, const size_t size)
{
    const unsigned int state_id = hipBlockIdx_x * hipBlockDim_x + hipThreadIdx_x;
    const unsigned int global_size = hipGridDim_x * hipBlockDim_x;

    GeneratorState state;
    const unsigned int subsequence = state_id;
    rocrand_init(0, subsequence, 0, &state);

    unsigned int index = state_id;
    while(index < size)
    {
        if(state_id % 4 == 0)
            output[index] = rocrand_uniform4(&state).x;
        else if(state_id % 2 == 0)
            output[index] = rocrand_uniform2(&state).x;
        else
            output[index] = rocrand_uniform(&state);
        index += global_size;
    }
}

template <class GeneratorState>
__global__
void rocrand_normal_kernel(float * output, const size_t size)
{
    const unsigned int state_id = hipBlockIdx_x * hipBlockDim_x + hipThreadIdx_x;
    const unsigned int global_size = hipGridDim_x * hipBlockDim_x;

    GeneratorState state;
    const unsigned int subsequence = state_id;
    rocrand_init(0, subsequence, 0, &state);

    unsigned int index = state_id;
    while(index < size)
    {
        if(state_id % 4 == 0)
            output[index] = rocrand_normal4(&state).x;
        else if(state_id % 2 == 0)
            output[index] = rocrand_normal2(&state).x;
        else
            output[index] = rocrand_normal(&state);
        index += global_size;
    }
}

template <class GeneratorState>
__global__
void rocrand_log_normal_kernel(float * output, const size_t size)
{
    const unsigned int state_id = hipBlockIdx_x * hipBlockDim_x + hipThreadIdx_x;
    const unsigned int global_size = hipGridDim_x * hipBlockDim_x;

    GeneratorState state;
    const unsigned int subsequence = state_id;
    rocrand_init(0, subsequence, 0, &state);

    unsigned int index = state_id;
    while(index < size)
    {
        if(state_id % 4 == 0)
            output[index] = rocrand_log_normal4(&state, 1.6f, 0.25f).x;
        else if(state_id % 2 == 0)
            output[index] = rocrand_log_normal2(&state, 1.6f, 0.25f).x;
        else
            output[index] = rocrand_log_normal(&state, 1.6f, 0.25f);
        index += global_size;
    }
}

TEST(rocrand_kernel_philox4x32_10, rocrand_state_philox4x32_10_type)
{
    EXPECT_EQ(sizeof(rocrand_state_philox4x32_10), 16 * sizeof(float));
    EXPECT_EQ(sizeof(rocrand_state_philox4x32_10[32]), 32 * sizeof(rocrand_state_philox4x32_10));
}

TEST(rocrand_kernel_philox4x32_10, rocrand_init)
{
    // Just get access to internal state
    class rocrand_state_philox4x32_10_test : public rocrand_state_philox4x32_10
    {
        typedef rocrand_state_philox4x32_10 base_type;
        typedef rocrand_state_philox4x32_10::philox4x32_10_state internal_state_type;

    public:

        __host__ rocrand_state_philox4x32_10_test() {}

        __host__ internal_state_type internal_state() const
        {
            return m_state;
        }
    };

    typedef rocrand_state_philox4x32_10 state_type;
    typedef rocrand_state_philox4x32_10_test state_type_test;

    unsigned long long seed = 0xdeadbeefbeefdeadULL;
    unsigned long long offset = 4 * ((UINT_MAX * 17UL) + 17);

    const size_t states_size = 256;
    state_type * states;
    HIP_CHECK(hipMalloc((void **)&states, states_size * sizeof(state_type)));
    HIP_CHECK(hipDeviceSynchronize());

    hipLaunchKernelGGL(
        HIP_KERNEL_NAME(rocrand_init_kernel),
        dim3(8), dim3(32), 0, 0,
        states, states_size,
        seed, offset
    );
    HIP_CHECK(hipPeekAtLastError());

    std::vector<state_type_test> states_host(states_size);
    HIP_CHECK(
        hipMemcpy(
            states_host.data(), states,
            states_size * sizeof(state_type),
            hipMemcpyDeviceToHost
        )
    );
    HIP_CHECK(hipDeviceSynchronize());
    HIP_CHECK(hipFree(states));

    unsigned int subsequence = 0;
    for(auto& state : states_host)
    {
        auto s = state.internal_state();
        EXPECT_EQ(s.key.x, 0xbeefdeadU);
        EXPECT_EQ(s.key.y, 0xdeadbeefU);

        EXPECT_EQ(s.counter.x, 0U);
        EXPECT_EQ(s.counter.y, 17U);
        EXPECT_EQ(s.counter.z, subsequence);
        EXPECT_EQ(s.counter.w, 0U);

        EXPECT_TRUE(
            s.result.x != 0U
            || s.result.y != 0U
            || s.result.z != 0U
            || s.result.w
        );

        EXPECT_EQ(s.substate, 0U);

        subsequence++;
    }
}

TEST(rocrand_kernel_philox4x32_10, rocrand)
{
    typedef rocrand_state_philox4x32_10 state_type;

    const size_t output_size = 8192;
    unsigned int * output;
    HIP_CHECK(hipMalloc((void **)&output, output_size * sizeof(unsigned int)));
    HIP_CHECK(hipDeviceSynchronize());

    hipLaunchKernelGGL(
        HIP_KERNEL_NAME(rocrand_kernel<state_type>),
        dim3(8), dim3(32), 0, 0,
        output, output_size
    );
    HIP_CHECK(hipPeekAtLastError());

    std::vector<unsigned int> output_host(output_size);
    HIP_CHECK(
        hipMemcpy(
            output_host.data(), output,
            output_size * sizeof(unsigned int),
            hipMemcpyDeviceToHost
        )
    );
    HIP_CHECK(hipDeviceSynchronize());
    HIP_CHECK(hipFree(output));

    double mean = 0;
    for(auto v : output_host)
    {
        mean += static_cast<double>(v) / UINT_MAX;
    }
    mean = mean / output_size;
    EXPECT_NEAR(mean, 0.5, 0.1);
}

TEST(rocrand_kernel_philox4x32_10, rocrand_uniform)
{
    typedef rocrand_state_philox4x32_10 state_type;

    const size_t output_size = 8192;
    float * output;
    HIP_CHECK(hipMalloc((void **)&output, output_size * sizeof(float)));
    HIP_CHECK(hipDeviceSynchronize());

    hipLaunchKernelGGL(
        HIP_KERNEL_NAME(rocrand_uniform_kernel<state_type>),
        dim3(8), dim3(32), 0, 0,
        output, output_size
    );
    HIP_CHECK(hipPeekAtLastError());

    std::vector<float> output_host(output_size);
    HIP_CHECK(
        hipMemcpy(
            output_host.data(), output,
            output_size * sizeof(float),
            hipMemcpyDeviceToHost
        )
    );
    HIP_CHECK(hipDeviceSynchronize());
    HIP_CHECK(hipFree(output));

    double mean = 0;
    for(auto v : output_host)
    {
        mean += static_cast<double>(v);
    }
    mean = mean / output_size;
    EXPECT_NEAR(mean, 0.5, 0.1);
}

TEST(rocrand_kernel_philox4x32_10, rocrand_normal)
{
    typedef rocrand_state_philox4x32_10 state_type;

    const size_t output_size = 8192;
    float * output;
    HIP_CHECK(hipMalloc((void **)&output, output_size * sizeof(float)));
    HIP_CHECK(hipDeviceSynchronize());

    hipLaunchKernelGGL(
        HIP_KERNEL_NAME(rocrand_normal_kernel<state_type>),
        dim3(8), dim3(32), 0, 0,
        output, output_size
    );
    HIP_CHECK(hipPeekAtLastError());

    std::vector<float> output_host(output_size);
    HIP_CHECK(
        hipMemcpy(
            output_host.data(), output,
            output_size * sizeof(float),
            hipMemcpyDeviceToHost
        )
    );
    HIP_CHECK(hipDeviceSynchronize());
    HIP_CHECK(hipFree(output));

    double mean = 0;
    for(auto v : output_host)
    {
        mean += static_cast<double>(v);
    }
    mean = mean / output_size;
    EXPECT_NEAR(mean, 0.0, 0.2);

    double stddev = 0;
    for(auto v : output_host)
    {
        stddev += std::pow(static_cast<double>(v) - mean, 2);
    }
    stddev = stddev / output_size;
    EXPECT_NEAR(stddev, 1.0, 0.2);
}

TEST(rocrand_kernel_philox4x32_10, rocrand_log_normal)
{
    typedef rocrand_state_philox4x32_10 state_type;

    const size_t output_size = 8192;
    float * output;
    HIP_CHECK(hipMalloc((void **)&output, output_size * sizeof(float)));
    HIP_CHECK(hipDeviceSynchronize());

    hipLaunchKernelGGL(
        HIP_KERNEL_NAME(rocrand_log_normal_kernel<state_type>),
        dim3(8), dim3(32), 0, 0,
        output, output_size
    );
    HIP_CHECK(hipPeekAtLastError());

    std::vector<float> output_host(output_size);
    HIP_CHECK(
        hipMemcpy(
            output_host.data(), output,
            output_size * sizeof(float),
            hipMemcpyDeviceToHost
        )
    );
    HIP_CHECK(hipDeviceSynchronize());
    HIP_CHECK(hipFree(output));

    double mean = 0;
    for(auto v : output_host)
    {
        mean += static_cast<double>(v);
    }
    mean = mean / output_size;

    double stddev = 0;
    for(auto v : output_host)
    {
        stddev += std::pow(v - mean, 2);
    }
    stddev = std::sqrt(stddev / output_size);

    double logmean = std::log(mean * mean / std::sqrt(stddev + mean * mean));
    double logstd = std::sqrt(std::log(1.0f + stddev/(mean * mean)));

    EXPECT_NEAR(1.6, logmean, 1.6 * 0.2);
    EXPECT_NEAR(0.25, logstd, 0.25 * 0.2);
}