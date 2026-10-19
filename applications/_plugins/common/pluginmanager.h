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
//
/// \file PluginManager.h
/// \version 2.20
//
//=====================================================================

#ifndef _PLUGINMANAGER_H
#define _PLUGINMANAGER_H

#include <string>

// C RunTime Header Files
#include <stdlib.h>
#ifndef __APPLE__
#include <malloc.h>
#endif
#include <memory.h>
#ifdef _WIN32
#include <tchar.h>
#include <direct.h>
#endif
#include <vector>

#include "pluginbase.h"

#define MAX_PLUGIN_FILENAME_STR 512
#define MAX_PLUGIN_NAME_STR 256
#define MAX_PLUGIN_UUID_STR 37
#define MAX_PLUGIN_TYPE_STR 64
#define MAX_PLUGIN_CATEGORY_STR 64

#define DEFAULT_PLUGINLIST_DIR "./plugins"

class PluginDetails
{
public:
    PluginDetails()
    {
        clearMembers();
    }
    ~PluginDetails();

    // Set for the duration of PluginManager's own destructor. Only then is it unsafe to
    // FreeLibrary() a GPU-driver-backed plugin (see the note in ~PluginDetails()); every
    // other teardown path - clearPluginList() from a getPluginList() refresh, RemovePlugin()
    // at runtime - must still unload, otherwise modules leak for the life of the process.
    static bool s_inProcessTeardown;

    void* makeNewInstance();
    void  setFileName(char* nm);
    char* getFileName()
    {
        return filename;
    }
    char* getName()
    {
        return pluginName;
    }
    char* getUUID()
    {
        return pluginUUID;
    }
    char* getType()
    {
        return pluginType;
    }
    char* getCategory()
    {
        return pluginCategory;
    }
    unsigned long getOptions()
    {
        return pluginOptions;
    }
    void               setName(char* nm);
    void               setUUID(char* nm);
    void               setType(char* nm);
    void               setCategory(char* nm);
    void               setOptions(unsigned long uoptions);
    bool               isStatic;
    bool               isRegistered;  // true when all dll interfaces has been registered using LoadLibraryA and GetProcAddress
    PLUGIN_FACTORYFUNC funcHandle;

private:
    void clearMembers()
    {
#ifdef _WIN32
        dllHandle = NULL;
#endif
        isStatic     = false;
        isRegistered = false;

        filename[0]       = 0;
        pluginType[0]     = 0;
        pluginName[0]     = 0;
        pluginUUID[0]     = 0;
        pluginCategory[0] = 0;
        pluginOptions     = 0;
        funcHandle        = NULL;
    }

    char          filename[MAX_PLUGIN_FILENAME_STR];
    char          pluginType[MAX_PLUGIN_TYPE_STR];
    char          pluginName[MAX_PLUGIN_NAME_STR];
    char          pluginUUID[MAX_PLUGIN_UUID_STR];
    char          pluginCategory[MAX_PLUGIN_CATEGORY_STR];
    unsigned long pluginOptions;

#ifdef _WIN32
    HINSTANCE dllHandle;
#endif
};

class PluginManager
{
public:
    PluginManager();
    ~PluginManager();

    void getPluginList(char* dirPath, bool append = false);
    //! Permanently disables the dynamic (directory scanning + LoadLibrary) plugin search, so
    //! that only statically registered plugins are used. Intended for hosts such as the unit
    //! test harness that must never load DLLs found in the working directory, including via
    //! indirect calls made by CMP_RegisterHostPlugins().
    void disableDynamicPluginScan();
    //! Disables the PATH-search fallback in getPluginList(), so plugins are only ever found in
    //! the application directory. Intended for hosts whose own startup DLL trust policy only
    //! scans the application directory (see applications/common/dll_security_check.h) - falling
    //! back to a plugin found via PATH would silently bypass that policy. This class has no own
    //! notion of that policy, so the host must opt in explicitly.
    void restrictPluginSearchToApplicationDirectory();
    //! Opts this instance in to toggling PluginDetails::s_inProcessTeardown around
    //! clearPluginList() in the destructor. Only the single, process-lifetime global
    //! g_pluginManager (cmp_framework/compute_base.cpp) is destroyed at true CRT static-
    //! teardown time, where the GPU-driver FreeLibrary deadlock applies - a locally scoped
    //! PluginManager (e.g. cmp_cas.cpp's main()-local instance) must not flip this process-wide
    //! static just because it happens to also be destroyed, or it could suppress FreeLibrary for
    //! an unrelated, still-running global instance.
    void          markAsProcessGlobalInstance();
    void          getPluginListTypeNames(char* pluginType, std::vector<std::string>& TypeNames);
    void          getPluginListOptionNames(char* pluginType, unsigned long options, std::vector<std::string>& TypeNames);
    void          registerStaticPlugin(char* pluginType, char* pluginName, void* makePlugin);
    void          registerStaticPlugin(char* pluginType, char* pluginName, char* uuid, void* makePlugin);
    bool          PluginSupported(char* type, char* name);
    void          getPluginDetails(PluginDetails* curPlugin);
    int           getNumPlugins();
    void*         makeNewPluginInstance(int index);
    unsigned long getPluginOption(int index);
    char*         getPluginName(int index);
    char*         getPluginUUID(int index);
    char*         getPluginCategory(int index);
    char*         getPluginType(int index);
    void*         GetPlugin(char* type, const char* name);
    void*         GetPlugin(char* uuid);
    bool          RemovePlugin(char* type, char* name);

private:
    bool                        m_pluginlistset;
    bool                        m_dynamicscandisabled;
    bool                        m_pathsearchrestricted;
    bool                        m_isProcessGlobalInstance;
    char                        m_pluginfolder[MAX_PLUGIN_FILENAME_STR];
    void                        clearPluginList();
    bool                        fileExists(const std::string& abs_filename);
    std::vector<PluginDetails*> pluginRegister;
};

#endif
