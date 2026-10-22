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

#include "cmp_math_common.h"
#include "cmp_math_vec4.h"

#ifndef ASPM_GPU

//---------------------------------------------
// CPU: Computes square root of  a float value
//---------------------------------------------
float cpu_sqrtf(float* pIn)
{
    //printf("native : ");
    return sqrtf(*pIn);
}

#ifdef CMP_USE_XMMINTRIN
#ifndef __linux__
//---------------------------------------------
// SSE: Computes square root of  a float value
//---------------------------------------------
union WrappedSSE
{
    __m128 val;
    float  f[4];
};
float sse_sqrtf(float* pIn)
{
    //printf("sse    : ");

    WrappedSSE v;
    v.val       = _mm_load1_ps(pIn);
    v.val       = _mm_sqrt_ss(v.val);
    return v.f[0];
}
#endif
#endif

//-------------------------------------------------
// CPU: Computes 1 / (square root of a float value)
//-------------------------------------------------
float cpu_rsqf(float* f)
{
    float sf = cmp_sqrtf2(f);
    if (sf != 0)
        return 1 / cmp_sqrtf2(f);
    else
        return 0.0f;
}

#ifdef CMP_USE_XMMINTRIN
#ifndef __linux__
//-------------------------------------------------
// SSE: Computes 1 / (square root of a float value)
//-------------------------------------------------
#ifdef CMP_USE_RSQ_RSQR
float sse_rsqf(float* v)
{
    __m128 val = _mm_load1_ps(v);
    val        = _mm_rsqrt_ss(val);
    float frsq = val.m128_f32[0];
    return (0.5f * frsq) * (3.0f - (*v * frsq) * frsq);
};
#else
float sse_rsqf(float* v)
{
    WrappedSSE val;
    WrappedSSE val1;
    val.val  = _mm_set_ss(*v);  // Copy float and zero the upper 3 elements
    val1.val = _mm_set_ss(1.0f);
    val.val         = _mm_sqrt_ss(val.val);
    val.val         = _mm_div_ss(val1.val, val.val);
    return (val.f[0]);
};
#endif
#endif
#endif

//---------------------------------------------
// CPU: Computes min of two float values
//---------------------------------------------
float cpu_minf(float l1, float r1)
{
    return (l1 < r1 ? l1 : r1);
}

#ifdef CMP_USE_XMMINTRIN
#ifndef __linux__
float sse_minf(float a, float b)
{
    // Branchless SSE min.
    _mm_store_ss(&a, _mm_min_ss(_mm_set_ss(a), _mm_set_ss(b)));
    return a;
}
#endif
#endif

//---------------------------------------------
// CPU: Computes max of two float values
//---------------------------------------------
float cpu_maxf(float l1, float r1)
{
    return (l1 > r1 ? l1 : r1);
}

#ifdef CMP_USE_XMMINTRIN
#ifndef __linux__
float sse_maxf(float a, float b)
{
    // Branchless SSE max.
    _mm_store_ss(&a, _mm_max_ss(_mm_set_ss(a), _mm_set_ss(b)));
    return a;
}
#endif
#endif

//================================================
// Clamp the value in the range [minval .. maxval]
//================================================
float cpu_clampf(float value, float minval, float maxval)
{
    if (value < minval)
    {
        return (minval);
    }
    else if (value > maxval)
    {
        return (maxval);
    }
    return value;
}

#ifdef CMP_USE_XMMINTRIN
#ifndef __linux__
float sse_clampf(float val, float minval, float maxval)
{
    _mm_store_ss(&val, _mm_min_ss(_mm_max_ss(_mm_set_ss(val), _mm_set_ss(minval)), _mm_set_ss(maxval)));
    return val;
}
#endif
#endif

void cpu_averageRGB(unsigned char* src_rgba_block)
{
    float medianR = 0.0f, medianG = 0.0f, medianB = 0.0f;

    for (CGU_UINT32 k = 0; k < 16; k++)
    {
        CGU_UINT32 R = (src_rgba_block[k] & 0xff0000) >> 16;
        CGU_UINT32 G = (src_rgba_block[k] & 0xff00) >> 8;
        CGU_UINT32 B = src_rgba_block[k] & 0xff;

        medianR += R;
        medianG += G;
        medianB += B;
    }

    medianR /= 16;
    medianG /= 16;
    medianB /= 16;

    // Now skew the colour weightings based on the gravity center of the block
    float largest = cmp_maxf2(cmp_maxf2(medianR, medianG), medianB);

    if (largest > 0)
    {
        medianR /= largest;
        medianG /= largest;
        medianB /= largest;
    }
    else
        medianR = medianG = medianB = 1.0f;
}

float cpu_lerp2(CMP_Vec4uc C1, CMP_Vec4uc CA, CMP_Vec4uc CB, CMP_Vec4uc C2, CMP_MATH_BYTE* encode1, CMP_MATH_BYTE* encode2)
{
    // Initial Setup
    CMP_Vec4uc P[4];
    int        diff1;
    int        diff2;
    int        min1     = 0x1FF;
    int        min2     = 0x1FF;
    float      gradA[4] = {0.0f, 0.3f, 0.7f, 1.0f};
    float      D[3];

    D[0] = (float)C2.x - C1.x;
    D[1] = (float)C2.y - C1.y;
    D[2] = (float)C2.z - C1.z;

    for (int i = 0; i < 4; i++)
    {
        P[i].x = (CMP_MATH_BYTE)(C1.x + (gradA[i] * D[0]));
        P[i].y = (CMP_MATH_BYTE)(C1.y + (gradA[i] * D[1]));
        P[i].z = (CMP_MATH_BYTE)(C1.z + (gradA[i] * D[2]));

        // Now check if its closer to C1
        diff1 = abs(CA.x - P[i].x) + abs(CA.y - P[i].y) + abs(CA.z - P[i].z);
        if (diff1 < min1)
        {
            *encode1 = (CMP_MATH_BYTE)i;
            min1     = diff1;
        }

        // Now check if its closer to C2
        diff2 = abs(CB.x - P[i].x) + abs(CB.y - P[i].y) + abs(CB.z - P[i].z);
        if (diff2 < min2)
        {
            *encode2 = (CMP_MATH_BYTE)i;
            min2     = diff2;
        }
    }

    return float(min1 + min2);
}

#ifdef CMP_USE_XMMINTRIN
#ifndef __linux__
#ifdef __clang_major__
__attribute__((target("sse3")))
#endif
float sse_lerp2(CMP_Vec4uc C1, CMP_Vec4uc CA, CMP_Vec4uc CB, CMP_Vec4uc C2, CMP_MATH_BYTE* encode1, CMP_MATH_BYTE* encode2)
{
    // Initial Setup
    __m128 iC1, iC2, iCA, iCB;  //Load auchars into _m128
    __m128 iP[4];
    __m128 idiff1, idiff2;
    float  min     = 511.0f;
    __m128 imin1   = _mm_set_ps1(min);
    __m128 imin2   = _mm_set_ps1(min);
    __m128 absMask = _mm_set_ps1(-0.0);
    __m128 zero    = _mm_set_ps1(0.0);

    //Aplha channels have been disabled
    iC1 = _mm_set_ps(0, C1.z, C1.y, C1.x);
    iC2 = _mm_set_ps(0, C2.z, C2.y, C2.x);
    iCA = _mm_set_ps(0, CA.z, CA.y, CA.x);
    iCB = _mm_set_ps(0, CB.z, CB.y, CB.x);

    //Direction Vector
    __m128 iD;
    iD = _mm_sub_ps(iC2, iC1);

    float gradA[4] = {0.0f, 0.3f, 0.7f, 1.0f};

    for (int i = 0; i < 4; i++)
    {
        __m128 currentGradA = _mm_set_ps1(gradA[i]);
        iP[i]               = _mm_add_ps(_mm_mul_ps(currentGradA, iD), iC1);

        //Calculate SAD (Sum of Absolute Difference)
        __m128 iSub1 = _mm_sub_ps(iCA, iP[i]);
        __m128 iSub2 = _mm_sub_ps(iCB, iP[i]);
        __m128 iAbs1 = _mm_andnot_ps(absMask, iSub1);  // Computes the absolute value
        __m128 iAbs2 = _mm_andnot_ps(absMask, iSub2);
        idiff1       = _mm_hadd_ps(iAbs1, zero);
        idiff1       = _mm_hadd_ps(idiff1, zero);
        idiff2       = _mm_hadd_ps(iAbs2, zero);
        idiff2       = _mm_hadd_ps(idiff2, zero);

        if (_mm_comilt_ss(idiff1, imin1))
        {
            *encode1 = (CMP_MATH_BYTE)i;
            imin1    = idiff1;
        }

        if (_mm_comilt_ss(idiff2, imin2))
        {
            *encode2 = (CMP_MATH_BYTE)i;
            imin2    = idiff2;
        }
    }
    // Convert __m128 back to floats
    float result1[4];
    float result2[4];
    _mm_storeu_ps(result1, imin1);
    _mm_storeu_ps(result2, imin2);
    return (result1[0] + result2[0]);
}

#ifdef __clang_major__
__attribute__((target("fma")))
#endif
float fma_lerp2(CMP_Vec4uc C1, CMP_Vec4uc CA, CMP_Vec4uc CB, CMP_Vec4uc C2, CMP_MATH_BYTE* encode1, CMP_MATH_BYTE* encode2)
{
    // Initial Setup
    __m128 iC1, iC2, iCA, iCB;  //Load auchars into _m128
    __m128 iP[4];
    __m128 idiff1, idiff2;
    float  min     = 511.0f;
    __m128 imin1   = _mm_set_ps1(min);
    __m128 imin2   = _mm_set_ps1(min);
    __m128 absMask = _mm_set_ps1(-0.0);
    __m128 zero    = _mm_set_ps1(0.0);

    //Aplha channels have been disabled
    iC1 = _mm_set_ps(0, C1.z, C1.y, C1.x);
    iC2 = _mm_set_ps(0, C2.z, C2.y, C2.x);
    iCA = _mm_set_ps(0, CA.z, CA.y, CA.x);
    iCB = _mm_set_ps(0, CB.z, CB.y, CB.x);

    //Direction Vector
    __m128 iD;
    iD = _mm_sub_ps(iC2, iC1);

    float gradA[4] = {0.0f, 0.3f, 0.7f, 1.0f};

    for (int i = 0; i < 4; i++)
    {
        __m128 currentGradA = _mm_set_ps1(gradA[i]);
        iP[i]               = _mm_fmadd_ps(currentGradA, iD, iC1);

        //Calculate SAD (Sum of Absolute Difference)
        __m128 iSub1 = _mm_sub_ps(iCA, iP[i]);
        __m128 iSub2 = _mm_sub_ps(iCB, iP[i]);
        __m128 iAbs1 = _mm_andnot_ps(absMask, iSub1);  // Computes the absolute value
        __m128 iAbs2 = _mm_andnot_ps(absMask, iSub2);
        idiff1       = _mm_hadd_ps(iAbs1, zero);
        idiff1       = _mm_hadd_ps(idiff1, zero);
        idiff2       = _mm_hadd_ps(iAbs2, zero);
        idiff2       = _mm_hadd_ps(idiff2, zero);

        if (_mm_comilt_ss(idiff1, imin1))
        {
            *encode1 = (CMP_MATH_BYTE)i;
            imin1    = idiff1;
        }

        if (_mm_comilt_ss(idiff2, imin2))
        {
            *encode2 = (CMP_MATH_BYTE)i;
            imin2    = idiff2;
        }
    }

    // Convert __m128 back to floats
    float result1[4];
    float result2[4];
    _mm_storeu_ps(result1, imin1);
    _mm_storeu_ps(result2, imin2);
    return (result1[0] + result2[0]);
}

void cmp_set_fma3_features()
{
    //printf("FMA3 has been enabled.");
    cmp_lerp2 = fma_lerp2;
}
#endif
#endif

void cmp_set_cpu_features()
{
    // features list in Alphabetical order
    cmp_clampf2 = cpu_clampf;
    cmp_lerp2   = cpu_lerp2;
    cmp_maxf2   = cpu_maxf;
    cmp_minf2   = cpu_minf;
    cmp_rsqf2   = cpu_rsqf;
    cmp_sqrtf2  = cpu_sqrtf;
}

#ifdef CMP_USE_XMMINTRIN
#ifndef __linux__
void cmp_set_sse2_features()
{
    cmp_clampf2 = sse_clampf;
    cmp_lerp2   = sse_lerp2;
    cmp_maxf2   = sse_maxf;
    cmp_minf2   = sse_minf;
    cmp_rsqf2   = sse_rsqf;
    cmp_sqrtf2  = sse_sqrtf;
}
#endif
#endif

//---------------------------------
// User Interface to the CMP_MATH
//---------------------------------
float (*cmp_sqrtf2)(float*)                                                                                                    = cpu_sqrtf;
float (*cmp_rsqf2)(float*)                                                                                                     = cpu_rsqf;
float (*cmp_minf2)(float l1, float r1)                                                                                         = cpu_minf;
float (*cmp_maxf2)(float l1, float r1)                                                                                         = cpu_maxf;
float (*cmp_clampf2)(float value, float minval, float maxval)                                                                  = cpu_clampf;
float (*cmp_lerp2)(CMP_Vec4uc C1, CMP_Vec4uc CA, CMP_Vec4uc CB, CMP_Vec4uc C2, CMP_MATH_BYTE* encode1, CMP_MATH_BYTE* encode2) = cpu_lerp2;
void (*cmp_averageRGB)(unsigned char* src_rgba_block)                                                                          = cpu_averageRGB;
//}

#else
// OpenCL interfaces
float cmp_sqrtf2(float*){};
float cmp_rsqf2(float*){};
float cmp_minf2(float l1, float r1){};
float cmp_maxf2(float l1, float r1){};
float cmp_clampf2(float value, float minval, float maxval){};
void  cmp_lerp2(CMP_Vec4uc C1, CMP_Vec4uc CA, CMP_Vec4uc CB, CMP_Vec4uc C2, CMP_MATH_BYTE* encode1, CMP_MATH_BYTE* encode2){};
void  cmp_averageRGB(unsigned char* src_rgba_block){};
#endif  // not def OPENCL
