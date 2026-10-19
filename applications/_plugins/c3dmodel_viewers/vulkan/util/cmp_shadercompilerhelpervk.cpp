// AMD AMDUtils code
//
// Copyright(c) 2018 Advanced Micro Devices, Inc.All rights reserved.
//
// Vulkan Samples
//
// Copyright (C) 2015-2016 Valve Corporation
// Copyright (C) 2015-2016 LunarG, Inc.
// Copyright (C) 2015-2016 Google, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "cmp_shadercompilerhelpervk.h"

#include <cassert>
#include <fstream>
#include <iostream>

#include <glslang/Public/ShaderLang.h>       // TShader, TProgram, EShLanguage, EShMessages, InitializeProcess, ...
#include <glslang/Include/ResourceLimits.h>  // TBuiltInResource (moved out of transitive includes in newer glslang)
#include <SPIRV/GlslangToSpv.h>

#ifdef _WIN32
#include <windows.h>
#endif


void dbg_print(const char* msg) {
#ifdef _WIN32
    OutputDebugStringA(msg);
#else
#ifdef _DEBUG
    std::cout << msg << std::endl;
#endif
#endif
}


void init_resources(TBuiltInResource &Resources) {
    Resources.maxLights = 32;
    Resources.maxClipPlanes = 6;
    Resources.maxTextureUnits = 32;
    Resources.maxTextureCoords = 32;
    Resources.maxVertexAttribs = 64;
    Resources.maxVertexUniformComponents = 4096;
    Resources.maxVaryingFloats = 64;
    Resources.maxVertexTextureImageUnits = 32;
    Resources.maxCombinedTextureImageUnits = 80;
    Resources.maxTextureImageUnits = 32;
    Resources.maxFragmentUniformComponents = 4096;
    Resources.maxDrawBuffers = 32;
    Resources.maxVertexUniformVectors = 128;
    Resources.maxVaryingVectors = 8;
    Resources.maxFragmentUniformVectors = 16;
    Resources.maxVertexOutputVectors = 16;
    Resources.maxFragmentInputVectors = 15;
    Resources.minProgramTexelOffset = -8;
    Resources.maxProgramTexelOffset = 7;
    Resources.maxClipDistances = 8;
    Resources.maxComputeWorkGroupCountX = 65535;
    Resources.maxComputeWorkGroupCountY = 65535;
    Resources.maxComputeWorkGroupCountZ = 65535;
    Resources.maxComputeWorkGroupSizeX = 1024;
    Resources.maxComputeWorkGroupSizeY = 1024;
    Resources.maxComputeWorkGroupSizeZ = 64;
    Resources.maxComputeUniformComponents = 1024;
    Resources.maxComputeTextureImageUnits = 16;
    Resources.maxComputeImageUniforms = 8;
    Resources.maxComputeAtomicCounters = 8;
    Resources.maxComputeAtomicCounterBuffers = 1;
    Resources.maxVaryingComponents = 60;
    Resources.maxVertexOutputComponents = 64;
    Resources.maxGeometryInputComponents = 64;
    Resources.maxGeometryOutputComponents = 128;
    Resources.maxFragmentInputComponents = 128;
    Resources.maxImageUnits = 8;
    Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
    Resources.maxCombinedShaderOutputResources = 8;
    Resources.maxImageSamples = 0;
    Resources.maxVertexImageUniforms = 0;
    Resources.maxTessControlImageUniforms = 0;
    Resources.maxTessEvaluationImageUniforms = 0;
    Resources.maxGeometryImageUniforms = 0;
    Resources.maxFragmentImageUniforms = 8;
    Resources.maxCombinedImageUniforms = 8;
    Resources.maxGeometryTextureImageUnits = 16;
    Resources.maxGeometryOutputVertices = 256;
    Resources.maxGeometryTotalOutputComponents = 1024;
    Resources.maxGeometryUniformComponents = 1024;
    Resources.maxGeometryVaryingComponents = 64;
    Resources.maxTessControlInputComponents = 128;
    Resources.maxTessControlOutputComponents = 128;
    Resources.maxTessControlTextureImageUnits = 16;
    Resources.maxTessControlUniformComponents = 1024;
    Resources.maxTessControlTotalOutputComponents = 4096;
    Resources.maxTessEvaluationInputComponents = 128;
    Resources.maxTessEvaluationOutputComponents = 128;
    Resources.maxTessEvaluationTextureImageUnits = 16;
    Resources.maxTessEvaluationUniformComponents = 1024;
    Resources.maxTessPatchComponents = 120;
    Resources.maxPatchVertices = 32;
    Resources.maxTessGenLevel = 64;
    Resources.maxViewports = 16;
    Resources.maxVertexAtomicCounters = 0;
    Resources.maxTessControlAtomicCounters = 0;
    Resources.maxTessEvaluationAtomicCounters = 0;
    Resources.maxGeometryAtomicCounters = 0;
    Resources.maxFragmentAtomicCounters = 8;
    Resources.maxCombinedAtomicCounters = 8;
    Resources.maxAtomicCounterBindings = 1;
    Resources.maxVertexAtomicCounterBuffers = 0;
    Resources.maxTessControlAtomicCounterBuffers = 0;
    Resources.maxTessEvaluationAtomicCounterBuffers = 0;
    Resources.maxGeometryAtomicCounterBuffers = 0;
    Resources.maxFragmentAtomicCounterBuffers = 1;
    Resources.maxCombinedAtomicCounterBuffers = 1;
    Resources.maxAtomicCounterBufferSize = 16384;
    Resources.maxTransformFeedbackBuffers = 4;
    Resources.maxTransformFeedbackInterleavedComponents = 64;
    Resources.maxCullDistances = 8;
    Resources.maxCombinedClipAndCullDistances = 8;
    Resources.maxSamples = 4;
    Resources.limits.nonInductiveForLoops = 1;
    Resources.limits.whileLoops = 1;
    Resources.limits.doWhileLoops = 1;
    Resources.limits.generalUniformIndexing = 1;
    Resources.limits.generalAttributeMatrixVectorIndexing = 1;
    Resources.limits.generalVaryingIndexing = 1;
    Resources.limits.generalSamplerIndexing = 1;
    Resources.limits.generalVariableIndexing = 1;
    Resources.limits.generalConstantMatrixVectorIndexing = 1;
}

EShLanguage FindLanguage(const VkShaderStageFlagBits shader_type) {
    switch (shader_type) {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return EShLangVertex;

    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return EShLangTessControl;

    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return EShLangTessEvaluation;

    case VK_SHADER_STAGE_GEOMETRY_BIT:
        return EShLangGeometry;

    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return EShLangFragment;

    case VK_SHADER_STAGE_COMPUTE_BIT:
        return EShLangCompute;

    default:
        return EShLangVertex;
    }
}

void init_glslang() {
#ifndef __ANDROID__
    glslang::InitializeProcess();
#endif
}

void finalize_glslang() {
#ifndef __ANDROID__
    glslang::FinalizeProcess();
#endif
}

//
// Compile a given string containing GLSL into SPV for use by VK
// Return value of false means an error was encountered.
//
bool GLSLtoSPV(const VkShaderStageFlagBits shader_type, const char *pshader, const char *pEntryPoint, std::vector<unsigned int> &spirv) {
#ifndef __ANDROID__
    EShLanguage stage = FindLanguage(shader_type);
    glslang::TShader shader(stage);
    glslang::TProgram program;
    const char *shaderStrings[1];
    TBuiltInResource Resources;
    init_resources(Resources);

    // Enable SPIR-V and Vulkan rules when parsing GLSL
    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

    shaderStrings[0] = pshader;
    shader.setStrings(shaderStrings, 1);

    if (!shader.parse(&Resources, 100, false, messages)) {
        dbg_print(shader.getInfoLog());
        dbg_print(shader.getInfoDebugLog());
        return false;  // something didn't work
    }

    program.addShader(&shader);

    //
    // Program-level processing...
    //

    if (!program.link(messages)) {
        dbg_print(shader.getInfoLog());
        dbg_print(shader.getInfoDebugLog());
        fflush(stdout);
        return false;
    }

    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);
#else
    // On Android, use shaderc instead.
    shaderc::Compiler compiler;
    shaderc::SpvCompilationResult module =
        compiler.CompileGlslToSpv(pshader, strlen(pshader), MapShadercType(shader_type), "shader");
    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        LOGE("Error: Id=%d, Msg=%s", module.GetCompilationStatus(), module.GetErrorMessage().c_str());
        return false;
    }
    spirv.assign(module.cbegin(), module.cend());
#endif
    return true;
}

//
// Compile a given string containing GLSL into SPV for use by VK
// Return value of false means an error was encountered.
//
bool HLSLtoSPV(const VkShaderStageFlagBits shader_type, const char *pshader, const char *pEntryPoint, std::vector<unsigned int> &spirv) {
    EShLanguage stage = FindLanguage(shader_type);
    glslang::TShader shader(stage);
    glslang::TProgram program;
    const char *shaderStrings[1];
    TBuiltInResource Resources;
    init_resources(Resources);

    // Enable SPIR-V and Vulkan rules when parsing GLSL
    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl);

    shaderStrings[0] = pshader;
    shader.setStrings(shaderStrings, 1);
    shader.setEntryPoint(pEntryPoint);
#ifdef ENABLE_HLSL
    shader.setHlslIoMapping(true);
#endif
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

    if (!shader.parse(&Resources, 100, false, messages)) {
        dbg_print(shader.getInfoLog());
        dbg_print(shader.getInfoDebugLog());
        return false;  // something didn't work
    }

    program.addShader(&shader);

    //
    // Program-level processing...
    //
    if (!program.link(messages)) {
        dbg_print(shader.getInfoLog());
        dbg_print(shader.getInfoDebugLog());
        fflush(stdout);
        return false;
    }

    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);

    return true;
}


VkResult VKCompile(VkDevice device, ShaderSourceType sourceType, const VkShaderStageFlagBits shader_type, const char *pshader, const char *pEntryPoint, std::map<std::string, std::string> defines, VkPipelineShaderStageCreateInfo *pShader) {
    VkResult res;

    std::string shaderCode(pshader);

    if (sourceType == SST_GLSL) {
        // the first line in a GLSL shader must be the #version, insert the #defines right after this line
        size_t index = shaderCode.find_first_of('\n');

        std::string version = shaderCode.substr(0, index) + "\n";
        std::string code = shaderCode.substr(index, shaderCode.size() - index);

        shaderCode = version;

        for (auto it = defines.begin(); it != defines.end(); it++)
            shaderCode += "#define " + it->first + " " + it->second + "\n";

        dbg_print(shaderCode.c_str());

        shaderCode += code;
    } else if (sourceType == SST_HLSL) {
        std::string code = shaderCode;

        shaderCode = "";

        for (auto it = defines.begin(); it != defines.end(); it++)
            shaderCode += "#define " + it->first + " " + it->second + "\n";

        dbg_print(shaderCode.c_str());

        shaderCode += code;
    }

    std::string shaderSuffix;
    if (shader_type& VK_SHADER_STAGE_FRAGMENT_BIT)
        shaderSuffix = "ps";
    if (shader_type& VK_SHADER_STAGE_VERTEX_BIT)
        shaderSuffix = "vs";

    std::ofstream out("shaderCode_"+ shaderSuffix +".txt");
    out << shaderCode;
    out.close();

    std::vector<unsigned int> spirv;
    pShader->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pShader->pNext = NULL;
    pShader->pSpecializationInfo = NULL;
    pShader->flags = 0;
    pShader->stage = shader_type;
    pShader->pName = pEntryPoint;

    bool retVal;

    switch (sourceType) {
    case SST_HLSL:
        retVal = HLSLtoSPV(shader_type, shaderCode.c_str(), pEntryPoint, spirv);
        break;
    case SST_GLSL:
        retVal = GLSLtoSPV(shader_type, shaderCode.c_str(), pEntryPoint, spirv);
        break;
    }

    // Continue even if shader build fails!!
    // Check console out for error messages
    if (!retVal) {
        return VK_SUCCESS;
    }

    std::ofstream OutFile;
    OutFile.open("shaderSpirv_" + shaderSuffix + ".bin", std::ios::out | std::ios::binary);
    OutFile.write((char*)spirv.data(), spirv.size() * sizeof(unsigned int));
    OutFile.close();

    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext = NULL;
    moduleCreateInfo.flags = 0;
    moduleCreateInfo.codeSize = spirv.size() * sizeof(unsigned int);
    moduleCreateInfo.pCode = spirv.data();
    res = vkCreateShaderModule(device, &moduleCreateInfo, NULL, &pShader->module);
    assert(res == VK_SUCCESS);
    return res;
}

size_t Hash(const char *ptr, size_t size, size_t result = 2166136261) {
    for (size_t i = 0; i < size; ++i) {
        result = (result * 16777619) ^ ptr[i];
    }

    return result;
}

std::map<size_t, VkPipelineShaderStageCreateInfo> s_shaderCache;

VkResult VKCompileFromFile(VkDevice device, ShaderSourceType sourceType, const VkShaderStageFlagBits shader_type, const char *pFilename, const char *pEntryPoint, std::map<std::string, std::string> defines, VkPipelineShaderStageCreateInfo *pShader) {
    size_t hash = Hash(pFilename, strlen(pFilename));
    hash = Hash(pEntryPoint, strlen(pEntryPoint), hash);
    for (auto it = defines.begin(); it != defines.end(); it++) {
        hash = Hash(it->first.c_str(), it->first.size(), hash);
        hash = Hash(it->second.c_str(), it->second.size(), hash);
    }
    hash = Hash((char*)&shader_type, sizeof(VkShaderStageFlagBits), hash);
    hash = Hash((char*)&sourceType, sizeof(ShaderSourceType), hash);

    VkResult res = VK_SUCCESS;

    auto it = s_shaderCache.find(hash);
    if (it != s_shaderCache.end()) {
        *pShader = it->second;
    } else {
        std::ifstream file(pFilename);
        std::string shader((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        res = VKCompile(device, sourceType, shader_type, shader.c_str(), pEntryPoint, defines, pShader);
        if (res == VK_SUCCESS)
            s_shaderCache[hash] = *pShader;
    }

    return res;
}
