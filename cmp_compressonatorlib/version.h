//=====================================================================
// Copyright 2016-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
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

// Version.h : version numbers
//
//  SDK                             Jenkins
//  VERSION_MAJOR_MAJOR             MAJOR
//  VERSION_MAJOR_MINOR             MINOR
//  VERSION_MINOR_MAJOR             BUILD
//  VERSION_MINOR_MINOR             UPDATE
//

#if !defined(_VERSION_H_INCLUDED_)
#define _VERSION_H_INCLUDED_

#define VERSION_MAJOR_MAJOR 4
#define VERSION_MAJOR_MINOR 6
#define VERSION_MINOR_MAJOR 0
#define VERSION_MINOR_MINOR 0
#define VERSION VERSION_MAJOR_MAJOR, VERSION_MAJOR_MINOR, VERSION_MINOR_MAJOR, VERSION_MINOR_MINOR
#define VERSION_TEXT "4, 6, 0"
#define VERSION_TEXT_SHORT "4.6"
#define COMPRESSONATOR_TEXT "COMPRESSONATOR"

#endif  // _VERSION_H_INCLUDED_
