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
#define ALIGN_64 __declspec(align(64))
#else
#define ALIGN_64 __attribute__((aligned(64)))
#endif

#ifdef __clang_major__
__attribute__((target("avx512f")))
#endif
CGU_FLOAT avx512_bc1ComputeBestEndpoints(CGU_FLOAT endpointsOut[2],
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

    __m512 prjVector      = _mm512_loadu_ps(prj);
    __m512 prjErrorVector = _mm512_loadu_ps(prjError);
    __m512 preMRepVector  = _mm512_loadu_ps(preMRep);

    __m512 zeroVector = _mm512_setzero_ps();

    for (int low = 0; low < 8; ++low)
    {
        for (int high = 0; high < 8; ++high)
        {
            // init constant vectors

            CGV_FLOAT stepScalar = (highStep - lowStep) / (numPoints - 1);

            __m512 lowStepVector     = _mm512_set1_ps(lowStep);
            __m512 highStepVector    = _mm512_set1_ps(highStep);
            __m512 stepVector        = _mm512_set1_ps(stepScalar);
            __m512 stepHVector       = _mm512_set1_ps(0.5f * stepScalar);
            __m512 inverseStepVector = _mm512_set1_ps(1.0f / stepScalar);

            // Calculate "del"

            __m512 del = _mm512_sub_ps(prjVector, lowStepVector);

            // Calculate V

            __m512 tempSum = _mm512_add_ps(del, stepHVector);

            __m512 tempProduct = _mm512_mul_ps(inverseStepVector, tempSum);

            __m512 tempFloor = _mm512_floor_ps(tempProduct);

            __m512 tempFloorProduct = _mm512_mul_ps(tempFloor, stepVector);

            __m512 possibleV = _mm512_add_ps(tempFloorProduct, lowStepVector);

            CGU_UINT16 cmpMask       = _mm512_cmp_ps_mask(highStepVector, prjVector, _CMP_GT_OQ);
            CGU_UINT16 secondCmpMask = _mm512_cmp_ps_mask(del, zeroVector, _CMP_GT_OQ);

            __m512 v = _mm512_mask_blend_ps(cmpMask, highStepVector, possibleV);
            v        = _mm512_mask_blend_ps(secondCmpMask, lowStepVector, v);

            // Calculate error

            __m512 d = _mm512_sub_ps(prjVector, v);

            __m512 dSquared = _mm512_mul_ps(d, d);

            __m512 errSum = _mm512_mul_ps(preMRepVector, dSquared);

            __m512 err = _mm512_add_ps(errSum, prjErrorVector);

            // Figure out final error values

            CGV_FLOAT ALIGN_64 errorResult[16];
            _mm512_store_ps(errorResult, err);

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