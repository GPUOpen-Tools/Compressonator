//=====================================================================
// Copyright (c) 2020-2024, Advanced Micro Devices, Inc. All rights reserved.
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

#define CATCH_CONFIG_RUNNER
#include "single_include/catch2/catch.hpp"
#include "core_tests.h"
#include "pluginmanager.h"

// CMP_LoadTexture only resolves image formats through registered plugins, and the framework
// registers just DDS. Mirror the CLI so the security regression tests exercise the real
// bounds-checked loaders instead of failing with CMP_ERR_UNSUPPORTED_SOURCE_FORMAT.
extern void* make_Plugin_TGA();
#ifdef CMP_UNITTESTS_HAVE_EXR
extern void* make_Plugin_EXR();
#endif

extern PluginManager g_pluginManager;

int main(int argc, char* argv[])
{
    // Only the statically linked loaders are registered. The dynamic plugin scan is disabled
    // outright: the tests must not LoadLibrary arbitrary DLLs found in the working directory,
    // and CMP_LoadTexture()/CMP_ProcessTexture() reach CMP_RegisterHostPlugins(), which would
    // otherwise call getPluginList(".", TRUE) on its first registration.
    g_pluginManager.disableDynamicPluginScan();
    g_pluginManager.registerStaticPlugin("IMAGE", "TGA", (void*)make_Plugin_TGA);
#ifdef CMP_UNITTESTS_HAVE_EXR
    g_pluginManager.registerStaticPlugin("IMAGE", "EXR", (void*)make_Plugin_EXR);
#endif

    AssignExpectedColorsToBlocks();
    int result = Catch::Session().run(argc, argv);
    return result;
}