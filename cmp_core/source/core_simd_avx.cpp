//=====================================================================
// Copyright 2023-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//=====================================================================

#include <immintrin.h>

#include "core_simd.h"
#include "common_def.h"

#if defined(_WIN32) || defined(_WIN64)
#define ALIGN_32 __declspec(align(32))
#else
#define ALIGN_32 __attribute__((aligned(32)))
#endif

#ifdef __clang_major__
__attribute__((target("avx")))
#endif
CGU_FLOAT avx_bc1ComputeBestEndpoints(CGU_FLOAT endpointsOut[2],
                                      CGU_FLOAT endpointsIn[2],
                                      CGU_FLOAT prj[16],
                                      CGU_FLOAT prjError[16],
                                      CGU_FLOAT preMRep[16],
                                      int       numColours,
                                      int       numPoints)
{
    static const CGU_FLOAT searchStep = 0.025f;

    const CGU_FLOAT lowStart  = (endpointsIn[0] - 2.0f * searchStep > 0.0f) ? endpointsIn[0] - 2.0f * searchStep : 0.0f;
    const CGU_FLOAT highStart = (endpointsIn[1] + 2.0f * searchStep < 1.0f) ? endpointsIn[1] + 2.0f * searchStep : 1.0f;

    CGU_FLOAT minError = 128000.0f;

    CGU_FLOAT lowStep  = lowStart;
    CGU_FLOAT highStep = highStart;

    // init SIMD vectors

    __m256 prjAVX0 = _mm256_loadu_ps(prj);
    __m256 prjAVX1 = _mm256_loadu_ps(prj + 8);

    __m256 prjErrorAVX0 = _mm256_loadu_ps(prjError);
    __m256 prjErrorAVX1 = _mm256_loadu_ps(prjError + 8);

    __m256 preMRepAVX0 = _mm256_loadu_ps(preMRep);
    __m256 preMRepAVX1 = _mm256_loadu_ps(preMRep + 8);

    __m256 zeroVector = _mm256_setzero_ps();

    for (int low = 0; low < 8; ++low)
    {
        for (int high = 0; high < 8; ++high)
        {
            // init constant vectors

            CGV_FLOAT stepScalar = (highStep - lowStep) / (numPoints - 1);

            __m256 lowStepVector     = _mm256_set1_ps(lowStep);
            __m256 highStepVector    = _mm256_set1_ps(highStep);
            __m256 stepVector        = _mm256_set1_ps(stepScalar);
            __m256 stepHVector       = _mm256_set1_ps(0.5f * stepScalar);
            __m256 inverseStepVector = _mm256_set1_ps(1.0f / stepScalar);

            // Calculate "del"

            __m256 del0 = _mm256_sub_ps(prjAVX0, lowStepVector);
            __m256 del1 = _mm256_sub_ps(prjAVX1, lowStepVector);

            // Calculate V

            __m256 tempSum0 = _mm256_add_ps(del0, stepHVector);
            __m256 tempSum1 = _mm256_add_ps(del1, stepHVector);

            __m256 tempProduct0 = _mm256_mul_ps(inverseStepVector, tempSum0);
            __m256 tempProduct1 = _mm256_mul_ps(inverseStepVector, tempSum1);

            __m256 tempFloor0 = _mm256_floor_ps(tempProduct0);
            __m256 tempFloor1 = _mm256_floor_ps(tempProduct1);

            __m256 tempFloorProduct0 = _mm256_mul_ps(tempFloor0, stepVector);
            __m256 tempFloorProduct1 = _mm256_mul_ps(tempFloor1, stepVector);

            __m256 possibleV0 = _mm256_add_ps(tempFloorProduct0, lowStepVector);
            __m256 possibleV1 = _mm256_add_ps(tempFloorProduct1, lowStepVector);

            __m256 cmpMask0 = _mm256_cmp_ps(highStepVector, prjAVX0, _CMP_GT_OQ);
            __m256 cmpMask1 = _mm256_cmp_ps(highStepVector, prjAVX1, _CMP_GT_OQ);

            __m256 secondCmpMask0 = _mm256_cmp_ps(del0, zeroVector, _CMP_GT_OQ);
            __m256 secondCmpMask1 = _mm256_cmp_ps(del1, zeroVector, _CMP_GT_OQ);

            __m256 v0 = _mm256_blendv_ps(highStepVector, possibleV0, cmpMask0);
            __m256 v1 = _mm256_blendv_ps(highStepVector, possibleV1, cmpMask1);

            v0 = _mm256_blendv_ps(lowStepVector, v0, secondCmpMask0);
            v1 = _mm256_blendv_ps(lowStepVector, v1, secondCmpMask1);

            // Calculate error

            __m256 d0 = _mm256_sub_ps(prjAVX0, v0);
            __m256 d1 = _mm256_sub_ps(prjAVX1, v1);

            __m256 dSquared0 = _mm256_mul_ps(d0, d0);
            __m256 dSquared1 = _mm256_mul_ps(d1, d1);

            __m256 errSum0 = _mm256_mul_ps(preMRepAVX0, dSquared0);
            __m256 errSum1 = _mm256_mul_ps(preMRepAVX1, dSquared1);

            __m256 err0 = _mm256_add_ps(errSum0, prjErrorAVX0);
            __m256 err1 = _mm256_add_ps(errSum1, prjErrorAVX1);

            // Figure out final error values

            CGV_FLOAT ALIGN_32 errorResult[16];
            _mm256_store_ps(errorResult, err0);
            _mm256_store_ps(errorResult + 8, err1);

            CGV_FLOAT finalError = 0.0f;
            for (int i = 0; i < numColours; ++i)
            {
                finalError += errorResult[i];
                if (finalError >= minError)
                {
                    finalError = minError;
                    break;
                }
            }

            if (finalError < minError)
            {
                // save better result
                minError        = finalError;
                endpointsOut[0] = lowStep;
                endpointsOut[1] = highStep;
            }

            highStep -= searchStep;
        }

        lowStep += searchStep;
    }

    return minError;
}