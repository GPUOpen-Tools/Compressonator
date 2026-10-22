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

#include <xmmintrin.h>
#include <smmintrin.h>

#include "core_simd.h"
#include "common_def.h"

#if defined(_WIN32) || defined(_WIN64)
#define ALIGN_16 __declspec(align(16))
#else
#define ALIGN_16 __attribute__((aligned(16)))
#endif

#ifdef __clang_major__
__attribute__((target("sse4.1")))
#endif
CGU_FLOAT sse_bc1ComputeBestEndpoints(CGU_FLOAT endpointsOut[2],
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

    __m128 prjSSE0 = _mm_loadu_ps(prj);
    __m128 prjSSE1 = _mm_loadu_ps(prj + 4);
    __m128 prjSSE2 = _mm_loadu_ps(prj + 8);
    __m128 prjSSE3 = _mm_loadu_ps(prj + 12);

    __m128 prjErrorSSE0 = _mm_loadu_ps(prjError);
    __m128 prjErrorSSE1 = _mm_loadu_ps(prjError + 4);
    __m128 prjErrorSSE2 = _mm_loadu_ps(prjError + 8);
    __m128 prjErrorSSE3 = _mm_loadu_ps(prjError + 12);

    __m128 preMRepSSE0 = _mm_loadu_ps(preMRep);
    __m128 preMRepSSE1 = _mm_loadu_ps(preMRep + 4);
    __m128 preMRepSSE2 = _mm_loadu_ps(preMRep + 8);
    __m128 preMRepSSE3 = _mm_loadu_ps(preMRep + 12);

    __m128 zeroVector = _mm_setzero_ps();

    for (int low = 0; low < 8; ++low)
    {
        for (int high = 0; high < 8; ++high)
        {
            // init constant vectors

            CGV_FLOAT stepScalar = (highStep - lowStep) / (numPoints - 1);

            __m128 lowStepVector     = _mm_set1_ps(lowStep);
            __m128 highStepVector    = _mm_set1_ps(highStep);
            __m128 stepVector        = _mm_set1_ps(stepScalar);
            __m128 stepHVector       = _mm_set1_ps(0.5f * stepScalar);
            __m128 inverseStepVector = _mm_set1_ps(1.0f / stepScalar);

            // Calculate "del"

            __m128 del0 = _mm_sub_ps(prjSSE0, lowStepVector);
            __m128 del1 = _mm_sub_ps(prjSSE1, lowStepVector);
            __m128 del2 = _mm_sub_ps(prjSSE2, lowStepVector);
            __m128 del3 = _mm_sub_ps(prjSSE3, lowStepVector);

            // Calculate V

            __m128 tempSum0 = _mm_add_ps(del0, stepHVector);
            __m128 tempSum1 = _mm_add_ps(del1, stepHVector);
            __m128 tempSum2 = _mm_add_ps(del2, stepHVector);
            __m128 tempSum3 = _mm_add_ps(del3, stepHVector);

            __m128 tempProduct0 = _mm_mul_ps(inverseStepVector, tempSum0);
            __m128 tempProduct1 = _mm_mul_ps(inverseStepVector, tempSum1);
            __m128 tempProduct2 = _mm_mul_ps(inverseStepVector, tempSum2);
            __m128 tempProduct3 = _mm_mul_ps(inverseStepVector, tempSum3);

            __m128 tempFloor0 = _mm_floor_ps(tempProduct0);
            __m128 tempFloor1 = _mm_floor_ps(tempProduct1);
            __m128 tempFloor2 = _mm_floor_ps(tempProduct2);
            __m128 tempFloor3 = _mm_floor_ps(tempProduct3);

            __m128 tempFloorProduct0 = _mm_mul_ps(tempFloor0, stepVector);
            __m128 tempFloorProduct1 = _mm_mul_ps(tempFloor1, stepVector);
            __m128 tempFloorProduct2 = _mm_mul_ps(tempFloor2, stepVector);
            __m128 tempFloorProduct3 = _mm_mul_ps(tempFloor3, stepVector);

            __m128 possibleV0 = _mm_add_ps(tempFloorProduct0, lowStepVector);
            __m128 possibleV1 = _mm_add_ps(tempFloorProduct1, lowStepVector);
            __m128 possibleV2 = _mm_add_ps(tempFloorProduct2, lowStepVector);
            __m128 possibleV3 = _mm_add_ps(tempFloorProduct3, lowStepVector);

            __m128 cmpMask0 = _mm_cmpgt_ps(highStepVector, prjSSE0);
            __m128 cmpMask1 = _mm_cmpgt_ps(highStepVector, prjSSE1);
            __m128 cmpMask2 = _mm_cmpgt_ps(highStepVector, prjSSE2);
            __m128 cmpMask3 = _mm_cmpgt_ps(highStepVector, prjSSE3);

            __m128 secondCmpMask0 = _mm_cmpgt_ps(del0, zeroVector);
            __m128 secondCmpMask1 = _mm_cmpgt_ps(del1, zeroVector);
            __m128 secondCmpMask2 = _mm_cmpgt_ps(del2, zeroVector);
            __m128 secondCmpMask3 = _mm_cmpgt_ps(del3, zeroVector);

            __m128 v0 = _mm_blendv_ps(highStepVector, possibleV0, cmpMask0);
            __m128 v1 = _mm_blendv_ps(highStepVector, possibleV1, cmpMask1);
            __m128 v2 = _mm_blendv_ps(highStepVector, possibleV2, cmpMask2);
            __m128 v3 = _mm_blendv_ps(highStepVector, possibleV3, cmpMask3);

            v0 = _mm_blendv_ps(lowStepVector, v0, secondCmpMask0);
            v1 = _mm_blendv_ps(lowStepVector, v1, secondCmpMask1);
            v2 = _mm_blendv_ps(lowStepVector, v2, secondCmpMask2);
            v3 = _mm_blendv_ps(lowStepVector, v3, secondCmpMask3);

            // Calculate error

            __m128 d0 = _mm_sub_ps(prjSSE0, v0);
            __m128 d1 = _mm_sub_ps(prjSSE1, v1);
            __m128 d2 = _mm_sub_ps(prjSSE2, v2);
            __m128 d3 = _mm_sub_ps(prjSSE3, v3);

            __m128 dSquared0 = _mm_mul_ps(d0, d0);
            __m128 dSquared1 = _mm_mul_ps(d1, d1);
            __m128 dSquared2 = _mm_mul_ps(d2, d2);
            __m128 dSquared3 = _mm_mul_ps(d3, d3);

            __m128 errSum0 = _mm_mul_ps(preMRepSSE0, dSquared0);
            __m128 errSum1 = _mm_mul_ps(preMRepSSE1, dSquared1);
            __m128 errSum2 = _mm_mul_ps(preMRepSSE2, dSquared2);
            __m128 errSum3 = _mm_mul_ps(preMRepSSE3, dSquared3);

            __m128 err0 = _mm_add_ps(errSum0, prjErrorSSE0);
            __m128 err1 = _mm_add_ps(errSum1, prjErrorSSE1);
            __m128 err2 = _mm_add_ps(errSum2, prjErrorSSE2);
            __m128 err3 = _mm_add_ps(errSum3, prjErrorSSE3);

            // Figure out final error values

            CGV_FLOAT ALIGN_16 errorResult[16];
            _mm_store_ps(errorResult, err0);
            _mm_store_ps(errorResult + 4, err1);
            _mm_store_ps(errorResult + 8, err2);
            _mm_store_ps(errorResult + 12, err3);

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