//=====================================================================
// Copyright (c) 2026, Advanced Micro Devices, Inc. All rights reserved.
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
//
// Codec worker-thread lifecycle regression tests.
//
// -----------------------------------------------------------------------
// Defect summary
// -----------------------------------------------------------------------
//   Symptom   : Compressonator GUI 4.6.0 terminated silently (no error
//               dialog) minutes AFTER a successful BC6H compression, most
//               often while sitting idle with the compressed image view open.
//   Crash sig : c0000005 (access violation) at a constant module offset on a
//               non-GUI worker thread -- i.e. a deterministic code defect, not
//               heap corruption. Faulting code was BC6HBlockEncoder::
//               CompressBlock()'s 16-iteration MapToF16() loop reading
//               m_isSigned through a null/stale `this`.
//   Trigger   : any compress where NumThreads == 1 (GUI/CLI "-NumThreads 1"),
//               which sets m_Use_MultiThreading = false.
//
// Root cause
// ----------
// CInitializeBC6HLibrary() starts a BC6HThreadProcEncode worker thread for
// EVERY encoding slot, including the single-threaded case. The destructor,
// however, only signalled `exit = TRUE` and join()ed those workers when
// m_Use_MultiThreading was true; in the single-threaded case it simply
// detach()ed the worker and then freed m_EncodeParameterStorage and
// m_encoder[]. The orphaned thread keeps spin-polling `tp->exit` / `tp->run`
// inside freed memory; as soon as the allocator reuses that block with bytes
// that read back as `exit == 0 && run == TRUE` it calls
// tp->encoder->CompressBlock() through a dangling/NULL encoder pointer.
// BC7 / GT / APC / ASTC skipped their entire shutdown block in the same case,
// leaving the workers running and leaking the thread-handle array.
//
// Fix
// ---
// The destructors of CCodec_BC6H / BC7 / GT / APC / ASTC now unconditionally
// wait for in-flight work, set `exit = TRUE`, join() every live worker and
// release the handle array before any parameter storage or encoder object is
// freed. No thread is ever detached.
//
// How these tests detect a regression
// -----------------------------------
// The use-after-free itself is timing- and allocator-dependent, so asserting
// on a crash would be flaky. Instead these tests assert the deterministic
// invariant that the defect violates: after CMP_ConvertTexture() returns, the
// codec has been destroyed, so the process must own no more OS threads than it
// did before the call. A detached-and-never-joined worker spins forever and
// shows up immediately as a permanent thread-count increase.
//=====================================================================

#include "single_include/catch2/catch.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "compressonator.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#elif defined(__linux__)
#include <dirent.h>
#endif

#define CODEC_THREADS_INFO()                                                        \
    INFO("Defect   : codec worker threads detached instead of joined on shutdown"); \
    INFO("Trigger  : NumThreads == 1 (m_Use_MultiThreading == false)");             \
    INFO("Impact   : use-after-free on m_EncodeParameterStorage / m_encoder,");     \
    INFO("           silent process termination (c0000005) after compression");     \
    INFO("Affected : CCodec_BC6H, CCodec_BC7, CCodec_GT, CCodec_APC, CCodec_ASTC"); \
    INFO("Status   : FIXED -- destructors always signal exit and join()")

// Sentinel returned when the platform has no supported thread enumeration.
static const size_t kThreadCountUnsupported = ~static_cast<size_t>(0);

static size_t ProcessThreadCount()
{
#if defined(_WIN32)
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return kThreadCountUnsupported;

    const DWORD processId = GetCurrentProcessId();
    size_t      count     = 0;

    THREADENTRY32 entry = {};
    entry.dwSize        = sizeof(entry);

    if (Thread32First(snapshot, &entry))
    {
        do
        {
            // th32OwnerProcessID is only valid if the entry is large enough to contain it.
            if (entry.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(entry.th32OwnerProcessID) && entry.th32OwnerProcessID == processId)
            {
                ++count;
            }
            entry.dwSize = sizeof(entry);
        } while (Thread32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return count;
#elif defined(__linux__)
    DIR* dir = opendir("/proc/self/task");
    if (!dir)
        return kThreadCountUnsupported;

    size_t count = 0;
    while (struct dirent* entry = readdir(dir))
    {
        if (entry->d_name[0] != '.')
            ++count;
    }

    closedir(dir);
    return count;
#else
    return kThreadCountUnsupported;
#endif
}

// Polls the live thread count until it drops back to `expected` or the deadline expires.
// Returns the last sample so the caller can report the actual value on failure.
static size_t WaitForThreadCount(size_t expected, int timeoutMs)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    size_t count = ProcessThreadCount();
    while (count > expected && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        count = ProcessThreadCount();
    }
    return count;
}

static void CompressOnce(CMP_FORMAT srcFormat, CMP_FORMAT destFormat, CMP_DWORD numThreads)
{
    const uint32_t width  = 64;
    const uint32_t height = 64;

    CMP_Texture srcTexture = {};
    srcTexture.dwSize      = sizeof(CMP_Texture);
    srcTexture.dwWidth     = width;
    srcTexture.dwHeight    = height;
    srcTexture.format      = srcFormat;

    const bool srcIsFloat = (srcFormat == CMP_FORMAT_RGBA_32F);

    srcTexture.dwPitch    = srcIsFloat ? width * 4 * (CMP_DWORD)sizeof(float) : width * 4;
    srcTexture.dwDataSize = srcTexture.dwPitch * height;
    srcTexture.pData      = (CMP_BYTE*)calloc(1, srcTexture.dwDataSize);
    REQUIRE(srcTexture.pData != nullptr);

    if (srcIsFloat)
    {
        float* pixels = (float*)srcTexture.pData;
        for (uint32_t i = 0; i < width * height; ++i)
        {
            pixels[i * 4 + 0] = 4.0f;
            pixels[i * 4 + 1] = 2.0f;
            pixels[i * 4 + 2] = 1.0f;
            pixels[i * 4 + 3] = 1.0f;
        }
    }
    else
    {
        memset(srcTexture.pData, 0x7F, srcTexture.dwDataSize);
    }

    CMP_Texture destTexture = {};
    destTexture.dwSize      = sizeof(CMP_Texture);
    destTexture.dwWidth     = width;
    destTexture.dwHeight    = height;
    destTexture.dwPitch     = width;
    destTexture.format      = destFormat;
    destTexture.dwDataSize  = CMP_CalculateBufferSize(&destTexture);
    destTexture.pData       = (CMP_BYTE*)calloc(1, destTexture.dwDataSize);
    REQUIRE(destTexture.pData != nullptr);

    CMP_CompressOptions options = {};
    options.dwSize              = sizeof(options);
    options.fquality            = 0.05f;
    options.dwnumThreads        = numThreads;

    const CMP_ERROR result = CMP_ConvertTexture(&srcTexture, &destTexture, &options, nullptr);

    free(srcTexture.pData);
    free(destTexture.pData);

    REQUIRE(result == CMP_OK);
}

// Runs `iterations` compressions and requires that the process thread count
// returns to its pre-compression baseline afterwards.
static void RequireNoLeakedWorkers(CMP_FORMAT srcFormat, CMP_FORMAT destFormat, CMP_DWORD numThreads, int iterations)
{
    if (ProcessThreadCount() == kThreadCountUnsupported)
    {
        WARN("Thread enumeration is not supported on this platform - codec thread lifecycle check skipped.");
        return;
    }

    // Warm-up pass so that any one-off lazy initialisation threads exist before
    // the baseline is sampled, and so a leak is measured against a settled state.
    CompressOnce(srcFormat, destFormat, numThreads);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    const size_t baseline = ProcessThreadCount();

    for (int i = 0; i < iterations; ++i)
        CompressOnce(srcFormat, destFormat, numThreads);

    const size_t after = WaitForThreadCount(baseline, 5000);

    CODEC_THREADS_INFO();
    INFO("Destination format : " << (int)destFormat);
    INFO("NumThreads         : " << numThreads);
    INFO("Compress calls     : " << iterations);
    INFO("Baseline threads   : " << baseline);
    INFO("Threads after      : " << after);

    REQUIRE(after <= baseline);
}

TEST_CASE("CodecThreadLifecycle_Metadata", "[CodecThreads][ThreadLifecycle]")
{
    SUCCEED(
        "Codec worker-thread lifecycle | detached-instead-of-joined encoder threads with NumThreads==1 | "
        "use-after-free of m_EncodeParameterStorage/m_encoder -> silent process exit (c0000005) | "
        "Codecs: BC6H, BC7, GT, APC, ASTC | Status: FIXED");
}

TEST_CASE("CodecThreadLifecycle_ThreadEnumerationIsAvailable", "[CodecThreads][ThreadLifecycle]")
{
    const size_t count = ProcessThreadCount();

    if (count == kThreadCountUnsupported)
    {
        WARN("Thread enumeration is not supported on this platform - the codec thread leak tests will be no-ops.");
        return;
    }

    // A running test process always owns at least the main thread.
    REQUIRE(count >= 1);
}

// This is the direct regression test for the reported GUI crash: NumThreads == 1
// is exactly what the GUI/CLI passes as "-NumThreads 1".
TEST_CASE("CodecThreadLifecycle_BC6H_SingleThread_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle][BC6H]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_32F, CMP_FORMAT_BC6H, 1, 1);
}

TEST_CASE("CodecThreadLifecycle_BC6H_SF_SingleThread_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle][BC6H]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_32F, CMP_FORMAT_BC6H_SF, 1, 1);
}

// Repeated compression accumulates one orphaned worker per call when the
// destructor detaches instead of joining, so this fails much harder than the
// single-shot case and guards against a partial fix.
TEST_CASE("CodecThreadLifecycle_BC6H_RepeatedSingleThread_NoAccumulation", "[CodecThreads][ThreadLifecycle][BC6H]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_32F, CMP_FORMAT_BC6H, 1, 5);
}

TEST_CASE("CodecThreadLifecycle_BC6H_MultiThread_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle][BC6H]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_32F, CMP_FORMAT_BC6H, 4, 1);
}

// NumThreads == 0 means "auto detect", which also produces a multi-threaded pool.
TEST_CASE("CodecThreadLifecycle_BC6H_AutoThreads_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle][BC6H]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_32F, CMP_FORMAT_BC6H, 0, 1);
}

TEST_CASE("CodecThreadLifecycle_BC7_SingleThread_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle][BC7]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_8888, CMP_FORMAT_BC7, 1, 1);
}

TEST_CASE("CodecThreadLifecycle_BC7_RepeatedSingleThread_NoAccumulation", "[CodecThreads][ThreadLifecycle][BC7]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_8888, CMP_FORMAT_BC7, 1, 5);
}

TEST_CASE("CodecThreadLifecycle_BC7_MultiThread_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle][BC7]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_8888, CMP_FORMAT_BC7, 4, 1);
}

#if (OPTION_BUILD_ASTC == 1)
TEST_CASE("CodecThreadLifecycle_ASTC_SingleThread_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle][ASTC]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_8888, CMP_FORMAT_ASTC, 1, 1);
}
#endif

#ifdef USE_GTC
// CCodec_GT was one of the five codecs (BC6H, BC7, GT, APC, ASTC) named in the
// defect summary above as skipping its shutdown block in the single-threaded case.
TEST_CASE("CodecThreadLifecycle_GT_SingleThread_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle][GT]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_8888, CMP_FORMAT_GTC, 1, 1);
}

TEST_CASE("CodecThreadLifecycle_GT_RepeatedSingleThread_NoAccumulation", "[CodecThreads][ThreadLifecycle][GT]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_8888, CMP_FORMAT_GTC, 1, 5);
}

TEST_CASE("CodecThreadLifecycle_GT_MultiThread_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle][GT]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_8888, CMP_FORMAT_GTC, 4, 1);
}
#endif

#ifdef USE_APC
// CCodec_APC was likewise named in the defect summary above.
TEST_CASE("CodecThreadLifecycle_APC_SingleThread_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle][APC]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_8888, CMP_FORMAT_APC, 1, 1);
}

TEST_CASE("CodecThreadLifecycle_APC_RepeatedSingleThread_NoAccumulation", "[CodecThreads][ThreadLifecycle][APC]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_8888, CMP_FORMAT_APC, 1, 5);
}

TEST_CASE("CodecThreadLifecycle_APC_MultiThread_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle][APC]")
{
    RequireNoLeakedWorkers(CMP_FORMAT_RGBA_8888, CMP_FORMAT_APC, 4, 1);
}
#endif

// Interleaving codecs catches a shutdown path that only works when the same
// codec type is created and destroyed repeatedly.
TEST_CASE("CodecThreadLifecycle_MixedCodecs_SingleThread_NoLeakedWorkers", "[CodecThreads][ThreadLifecycle]")
{
    if (ProcessThreadCount() == kThreadCountUnsupported)
    {
        WARN("Thread enumeration is not supported on this platform - codec thread lifecycle check skipped.");
        return;
    }

    CompressOnce(CMP_FORMAT_RGBA_32F, CMP_FORMAT_BC6H, 1);
    CompressOnce(CMP_FORMAT_RGBA_8888, CMP_FORMAT_BC7, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    const size_t baseline = ProcessThreadCount();

    CompressOnce(CMP_FORMAT_RGBA_32F, CMP_FORMAT_BC6H, 1);
    CompressOnce(CMP_FORMAT_RGBA_8888, CMP_FORMAT_BC7, 1);
    CompressOnce(CMP_FORMAT_RGBA_32F, CMP_FORMAT_BC6H_SF, 1);

    const size_t after = WaitForThreadCount(baseline, 5000);

    CODEC_THREADS_INFO();
    INFO("Baseline threads : " << baseline);
    INFO("Threads after    : " << after);

    REQUIRE(after <= baseline);
}
