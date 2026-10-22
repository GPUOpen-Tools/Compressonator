//=====================================================================
// Copyright 2021-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
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
//  Pull changes:
//  Fixed build of cmp_core on Mac. (#164)
//=====================================================================
#ifndef CMP_MATH_VEC4_H
#define CMP_MATH_VEC4_H

#pragma warning(disable : 4201)

//====================================================
// Vector Class definitions for CPU & Intrinsics
//====================================================

#if defined(__linux__) || defined(__unix__) || defined(_WIN32) || defined(__APPLE__)

//============================================= VEC2 ==================================================

template <class T>
class Vec2T
{
public:
    T x;
    T y;

    // *****************************************
    //     Constructors
    // *****************************************

    /// Default constructor
    Vec2T()
        : x((T)0)
        , y((T)0){};

    /// Value constructor
    Vec2T(const T& vx, const T& vy)
        : x(vx)
        , y(vy){};

    /// Copy constructor
    Vec2T(const Vec2T<T>& val)
        : x(val.x)
        , y(val.y){};

    /// Single value constructor.  Sets all components to the given value
    Vec2T(const T& v)
        : x(v)
        , y(v){};

    // *****************************************
    //     Conversions/Assignment/Indexing
    // *****************************************

    /// cast to T*
    operator const T*() const
    {
        return (const T*)this;
    };

    /// cast to T*
    operator T*()
    {
        return (T*)this;
    };

    /// Indexing
    const T& operator[](int i) const
    {
        return ((const T*)this)[i];
    };
    T& operator[](int i)
    {
        return ((T*)this)[i];
    };

    /// Assignment
    const Vec2T<T>& operator=(const Vec2T<T>& rhs)
    {
        x = rhs.x;
        y = rhs.y;
        return *this;
    };

    // *****************************************
    //    Comparison
    // *****************************************

    /// Equality comparison
    bool operator==(const Vec2T<T>& rhs) const
    {
        return (x == rhs.x && y == rhs.y);
    };

    /// Inequality comparision
    bool operator!=(const Vec2T<T>& rhs) const
    {
        return (x != rhs.x || y != rhs.y);
    };

    // *****************************************
    //    Arithmetic
    // *****************************************

    /// Addition
    const Vec2T<T> operator+(const Vec2T<T>& rhs) const
    {
        return Vec2T<T>(x + rhs.x, y + rhs.y);
    };

    /// Subtraction
    const Vec2T<T> operator-(const Vec2T<T>& rhs) const
    {
        return Vec2T<T>(x - rhs.x, y - rhs.y);
    };

    /// Multiply
    const Vec2T<T> operator*(const Vec2T<T>& rhs) const
    {
        return Vec2T<T>(x * rhs.x, y * rhs.y);
    };

    /// Divide
    const Vec2T<T> operator/(const Vec2T<T>& rhs) const
    {
        return Vec2T<T>(x / rhs.x, y / rhs.y);
    };

    /// Multiply by scalar
    const Vec2T<T> operator*(const T& v) const
    {
        return Vec2T<T>(x * v, y * v);
    };

    /// Divide by scalar
    const Vec2T<T> operator/(const T& v) const
    {
        return Vec2T<T>(x / v, y / v);
    };

    /// Addition in-place
    Vec2T<T>& operator+=(const Vec2T<T>& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    };

    /// Subtract in-place
    Vec2T<T>& operator-=(const Vec2T<T>& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    };

    /// Scalar multiply in-place
    Vec2T<T>& operator*=(const T& v)
    {
        x *= v;
        y *= v;
        return *this;
    };

    /// Scalar divide in-place
    Vec2T<T>& operator/=(const T& v)
    {
        x /= v;
        y /= v;
        return *this;
    };
};

typedef Vec2T<float>  CMP_Vec2f;
typedef Vec2T<float>  CGU_Vec2f;
typedef Vec2T<float>  CGV_Vec2f;
typedef Vec2T<double> CMP_Vec2d;
typedef Vec2T<int>    CMP_Vec2i;

typedef Vec2T<int>          CGU_Vec2i;
typedef Vec2T<unsigned int> CGU_Vec2ui;

//============================================= VEC3 ==================================================
template <class T>
class Vec3T
{
public:
    union
    {
        struct
        {
            T x, y, z;
        };
        struct
        {
            T r, g, b;
        };
        struct
        {
            Vec2T<T> rg;
        };
    };

    // *****************************************
    //     Constructors
    // *****************************************

    /// Default constructor
    Vec3T()
        : x((T)0)
        , y((T)0)
        , z((T)0){};

    /// Value constructor
    Vec3T(const T& vx, const T& vy, const T& vz)
        : x(vx)
        , y(vy)
        , z(vz){};

    /// Copy constructor
    Vec3T(const Vec3T<T>& val)
        : x(val.x)
        , y(val.y)
        , z(val.z){};

    /// Single value constructor.  Sets all components to the given value
    Vec3T(const T& v)
        : x(v)
        , y(v)
        , z(v){};

    /// Array constructor.  Assumes a 3-component array
    Vec3T(const T* v)
        : x(v[0])
        , y(v[1])
        , z(v[2]){};

    // *****************************************
    //     Conversions/Assignment/Indexing
    // *****************************************

    /// cast to T*
    operator const T*() const
    {
        return (const T*)this;
    };

    /// cast to T*
    operator T*()
    {
        return (T*)this;
    };

    /// Assignment
    const Vec3T<T>& operator=(const Vec3T<T>& rhs)
    {
        x = rhs.x;
        y = rhs.y;
        z = rhs.z;
        return *this;
    };

    // *****************************************
    //    Comparison
    // *****************************************

    /// Equality comparison
    bool operator==(const Vec3T<T>& rhs) const
    {
        return (x == rhs.x && y == rhs.y && z == rhs.z);
    };

    /// Inequality comparision
    bool operator!=(const Vec3T<T>& rhs) const
    {
        return (x != rhs.x || y != rhs.y || z != rhs.z);
    };

    // *****************************************
    //    Arithmetic
    // *****************************************

    /// Addition by vector
    const Vec3T<T> operator+(const Vec3T<T>& rhs) const
    {
        return Vec3T<T>(x + rhs.x, y + rhs.y, z + rhs.z);
    };

    /// Subtraction by vector
    const Vec3T<T> operator-(const Vec3T<T>& rhs) const
    {
        return Vec3T<T>(x - rhs.x, y - rhs.y, z - rhs.z);
    };

    /// Multiply by vector
    const Vec3T<T> operator*(const Vec3T<T>& rhs) const
    {
        return Vec3T<T>(x * rhs.x, y * rhs.y, z * rhs.z);
    };

    /// Divide by vector
    const Vec3T<T> operator/(const Vec3T<T>& rhs) const
    {
        return Vec3T<T>(x / rhs.x, y / rhs.y, z / rhs.z);
    };

    /// Multiply by scalar
    const Vec3T<T> operator*(const T& v) const
    {
        return Vec3T<T>(x * v, y * v, z * v);
    };

    /// Divide by scalar
    const Vec3T<T> operator/(const T& v) const
    {
        return Vec3T<T>(x / v, y / v, z / v);
    };

    /// Add by scalar
    const Vec3T<T> operator+(const T& v) const
    {
        return Vec3T<T>(x + v, y + v, z + v);
    };

    /// Subtract by scalar
    const Vec3T<T> operator-(const T& v) const
    {
        return Vec3T<T>(x - v, y - v, z - v);
    };

    /// Addition in-place
    Vec3T<T>& operator+=(const Vec3T<T>& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    };

    /// Subtract in-place
    Vec3T<T>& operator-=(const Vec3T<T>& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    };

    /// Scalar multiply in-place
    Vec3T<T>& operator*=(const T& v)
    {
        x *= v;
        y *= v;
        z *= v;
        return *this;
    };

    /// Scalar Add in-place
    Vec3T<T>& operator+=(const T& v)
    {
        x += v;
        y += v;
        z += v;
        return *this;
    };

    /// Scalar divide in-place
    Vec3T<T>& operator/=(const T& v)
    {
        x /= v;
        y /= v;
        z /= v;
        return *this;
    };
};

typedef Vec3T<bool>          CGU_Vec3bool;
typedef Vec3T<float>         CGU_Vec3f;
typedef Vec3T<float>         CGV_Vec3f;
typedef Vec3T<int>           CGU_Vec3i;
typedef Vec3T<unsigned char> CGU_Vec3uc;
typedef Vec3T<unsigned char> CGV_Vec3uc;

typedef Vec3T<float>         CMP_Vec3f;
typedef Vec3T<double>        CMP_Vec3d;
typedef Vec3T<int>           CMP_Vec3i;
typedef Vec3T<unsigned char> CMP_Vec3uc;
typedef Vec3T<unsigned int>  CMP_Vec3ui;

//============================================= VEC4 ==================================================

template <class T>
class Vec4T
{
public:
    union
    {
        struct
        {
            T x, y, z, w;
        };
        struct
        {
            T r, g, b, a;
        };
        struct
        {
            Vec3T<T> rgb;  // a is undefined
        };
    };

    // *****************************************
    //     Constructors
    // *****************************************

    /// Default constructor
    Vec4T()
        : x((T)0)
        , y((T)0)
        , z((T)0)
        , w((T)0){};

    /// Value constructor
    Vec4T(const T& vx, const T& vy, const T& vz, const T& vw)
        : x(vx)
        , y(vy)
        , z(vz)
        , w(vw){};

    /// Copy constructor
    Vec4T(const Vec4T<T>& val)
        : x(val.x)
        , y(val.y)
        , z(val.z)
        , w(val.w){};

    /// Single value constructor.  Sets all components to the given value
    Vec4T(const T& v)
        : x(v)
        , y(v)
        , z(v)
        , w(v){};

    /// Array constructor.  Assumes a 4-component array
    Vec4T(const T* v)
        : x(v[0])
        , y(v[1])
        , z(v[2])
        , w(v[3]){};

    // *****************************************
    //     Conversions/Assignment/Indexing
    // *****************************************

    /// cast to T*
    operator const T*() const
    {
        return (const T*)this;
    };

    /// cast to T*
    operator T*()
    {
        return (T*)this;
    };

    /// negate to -T
    //Vec4T<T> operator-() const;

    /// Assignment
    const Vec4T<T>& operator=(const T& v)
    {
        x = v;
        y = v;
        z = v;
        w = v;
        return *this;
    };

    const Vec4T<T>& operator=(const Vec4T<T>& rhs)
    {
        x = rhs.x;
        y = rhs.y;
        z = rhs.z;
        w = rhs.w;
        return *this;
    };

    // *****************************************
    //    Comparison
    // *****************************************

    /// Equality comparison
    bool operator==(const Vec4T<T>& rhs) const
    {
        return (x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w);
    };

    /// Inequality comparision
    bool operator!=(const Vec4T<T>& rhs) const
    {
        return (x != rhs.x || y != rhs.y || z != rhs.z || w != rhs.w);
    };

    // *****************************************
    //    Arithmetic
    // *****************************************

    /// Addition by vector
    const Vec4T<T> operator+(const Vec4T<T>& rhs) const
    {
        return Vec4T<T>(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
    };

    /// Subtraction  by vector
    const Vec4T<T> operator-(const Vec4T<T>& rhs) const
    {
        return Vec4T<T>(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
    };

    /// Multiply  by vector
    const Vec4T<T> operator*(const Vec4T<T>& rhs) const
    {
        return Vec4T<T>(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w);
    };

    /// Divide by vector
    const Vec4T<T> operator/(const Vec4T<T>& rhs) const
    {
        return Vec4T<T>(x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w);
    };

    /// Multiply by scalar RHS
    const Vec4T<T> operator*(const T& v) const
    {
        return Vec4T<T>(x * v, y * v, z * v, w * v);
    };

    /// Divide by scalar
    const Vec4T<T> operator/(const T& v) const
    {
        return Vec4T<T>(x / v, y / v, z / v, w / v);
    };

    /// Add by scalar
    const Vec4T<T> operator+(const T& v) const
    {
        return Vec4T<T>(x + v, y + v, z + v, w + v);
    };

    /// Subtract by scalar
    const Vec4T<T> operator-(const T& v) const
    {
        return Vec4T<T>(x - v, y - v, z - v, w - v);
    };

    /** Negation operator. Returns the negative of the vector. */
    const Vec4T<T> operator-() const
    {
        return Vec4T(-x, -y, -z, -w);
    }

    /// Left bit shift vector by a scalar
    const Vec4T<T> operator<<(const T& v) const
    {
        return Vec4T<T>(x << v, y << v, z << v, w << v);
    }

    /// Right bit shift vector by a scalar
    const Vec4T<T> operator>>(const T& v) const
    {
        return Vec4T<T>(x >> v, y >> v, z >> v, w >> v);
    }

    /// Addition in-place
    Vec4T<T>& operator+=(const Vec4T<T>& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        w += rhs.w;
        return *this;
    };

    /// Subtract in-place
    Vec4T<T>& operator-=(const Vec4T<T>& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        w -= rhs.w;
        return *this;
    };

    /// Scalar multiply in-place
    Vec4T<T>& operator*=(const T& v)
    {
        x *= v;
        y *= v;
        z *= v;
        w *= v;
        return *this;
    };

    /// Scalar divide in-place
    Vec4T<T>& operator/=(const T& v)
    {
        x /= v;
        y /= v;
        z /= v;
        w /= v;
        return *this;
    };
};

// template <typename T>
// std::ostream& operator<<(std::ostream& out, Vec4T<T>& v)
// {
//     return out << v.x << ", " << v.y << ", " << v.z;
// }

#ifdef CMP_USE_XMMINTRIN

#include <stdio.h>
#include "xmmintrin.h"
#include <math.h>
#include <float.h>

// SSE Vec4
#ifndef _WIN32
class CMP_SSEVec4f
#else
#include "intrin.h"
class __declspec(align(16)) CMP_SSEVec4f
#endif
{
public:
    union
    {
        __m128 vec128;  // float Vector 128 bits in total (16 Bytes) = array of 4 floats
#if !defined(_WIN32) || defined(__clang_version__)
        float f32[4];
#endif
    };

    // constructors
    inline CMP_SSEVec4f(){};
    inline CMP_SSEVec4f(float x, float y, float z, float w)
        : vec128(_mm_setr_ps(x, y, z, w)){};
    inline CMP_SSEVec4f(__m128 vec)
        : vec128(vec)
    {
    }
    inline CMP_SSEVec4f(const float* data)
        : vec128(_mm_load_ps(data)){};
    inline CMP_SSEVec4f(float scalar)
        : vec128(_mm_load1_ps(&scalar)){};

    // copy and assignment
    inline CMP_SSEVec4f(const CMP_SSEVec4f& init)
        : vec128(init.vec128){};
    inline const CMP_SSEVec4f& operator=(const CMP_SSEVec4f& lhs)
    {
        vec128 = lhs.vec128;
        return *this;
    };

    // conversion to m128 type for direct use in _mm intrinsics
    inline operator __m128()
    {
        return vec128;
    };
    inline operator const __m128() const
    {
        return vec128;
    };

    // indexing
#if !defined(_WIN32) || defined(__clang_version__)
    inline const float& operator[](int i) const
    {
        return f32[i];
    };
    inline float& operator[](int i)
    {
        return f32[i];
    };
#else
    inline const float& operator[](int i) const
    {
        return vec128.m128_f32[i];
    };
    inline float& operator[](int i)
    {
        return vec128.m128_f32[i];
    };
#endif

    // addition
    inline CMP_SSEVec4f operator+(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_add_ps(vec128, rhs.vec128));
    };
    inline CMP_SSEVec4f& operator+=(const CMP_SSEVec4f& rhs)
    {
        vec128 = _mm_add_ps(vec128, rhs.vec128);
        return *this;
    };

    // multiplication
    inline CMP_SSEVec4f operator*(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_mul_ps(vec128, rhs.vec128));
    };
    inline CMP_SSEVec4f& operator*=(const CMP_SSEVec4f& rhs)
    {
        vec128 = _mm_mul_ps(vec128, rhs.vec128);
        return *this;
    };

    // scalar multiplication
    //inline CMP_SSEVec4f operator*( float rhs ) const { return CMP_SSEVec4f( _mm_mul_ps(vec128, _mm_load1_ps(&rhs)) ); };
    //inline CMP_SSEVec4f& operator*=( float rhs )  { vec128 = _mm_mul_ps(vec128, _mm_load1_ps(&rhs)); return *this; };

    // subtraction
    inline CMP_SSEVec4f operator-(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_sub_ps(vec128, rhs.vec128));
    };
    inline CMP_SSEVec4f& operator-=(const CMP_SSEVec4f& rhs)
    {
        vec128 = _mm_sub_ps(vec128, rhs.vec128);
        return *this;
    };

    // division
    inline CMP_SSEVec4f operator/(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_div_ps(vec128, rhs.vec128));
    };
    inline CMP_SSEVec4f& operator/=(const CMP_SSEVec4f& rhs)
    {
        vec128 = _mm_div_ps(vec128, rhs.vec128);
        return *this;
    };

    // scalar division
    inline CMP_SSEVec4f operator/(float rhs) const
    {
        return CMP_SSEVec4f(_mm_div_ps(vec128, _mm_load1_ps(&rhs)));
    };
    inline CMP_SSEVec4f& operator/=(float rhs)
    {
        vec128 = _mm_div_ps(vec128, _mm_load1_ps(&rhs));
        return *this;
    };

    // comparison
    // these return 0 or 0xffffffff in each component
    inline CMP_SSEVec4f operator<(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_cmplt_ps(vec128, rhs.vec128));
    };
    inline CMP_SSEVec4f operator>(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_cmpgt_ps(vec128, rhs.vec128));
    };
    inline CMP_SSEVec4f operator<=(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_cmple_ps(vec128, rhs.vec128));
    };
    inline CMP_SSEVec4f operator>=(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_cmpge_ps(vec128, rhs.vec128));
    };
    inline CMP_SSEVec4f operator==(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_cmpeq_ps(vec128, rhs.vec128));
    };

    // bitwise operators
    inline CMP_SSEVec4f operator|(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_or_ps(vec128, rhs.vec128));
    };
    inline CMP_SSEVec4f operator&(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_and_ps(vec128, rhs.vec128));
    };
    inline CMP_SSEVec4f operator^(const CMP_SSEVec4f& rhs) const
    {
        return CMP_SSEVec4f(_mm_xor_ps(vec128, rhs.vec128));
    };
    inline const CMP_SSEVec4f& operator|=(const CMP_SSEVec4f& rhs)
    {
        vec128 = _mm_or_ps(vec128, rhs.vec128);
        return *this;
    };
    inline const CMP_SSEVec4f& operator&=(const CMP_SSEVec4f& rhs)
    {
        vec128 = _mm_and_ps(vec128, rhs.vec128);
        return *this;
    };

    // for some horrible reason,there's no bitwise not instruction for SSE,
    // so we have to do xor with 0xfffffff in order to fake it.
    // TO get a 0xffffffff, we execute 0=0
    inline CMP_SSEVec4f operator~() const
    {
        __m128 zero    = _mm_setzero_ps();
        __m128 is_true = _mm_cmpeq_ps(zero, zero);
        return _mm_xor_ps(is_true, vec128);
    };
};

#endif

typedef Vec4T<float>         CMP_Vec4f;
typedef Vec4T<double>        CMP_Vec4d;
typedef Vec4T<int>           CMP_Vec4i;
typedef Vec4T<unsigned int>  CMP_Vec4ui;  // unsigned 16 bit x,y,x,w
typedef Vec4T<unsigned char> CMP_Vec4uc;  // unsigned 8  bit x,y,x,w

typedef Vec4T<unsigned char> CGU_Vec4uc;  // unsigned 8  bit x,y,x,w
typedef Vec4T<unsigned char> CGV_Vec4uc;  // unsigned 8  bit x,y,x,w

#endif  // not ASPM_GPU

#endif  // Header Guard
