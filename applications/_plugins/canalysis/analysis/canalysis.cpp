//=====================================================================
// Copyright 2021-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
//=====================================================================
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

// for XML file processing
#include <cmp_rapidxml.hpp>

// Windows Header Files:
#ifdef _WIN32
#include <windows.h>
#endif

#include "canalysis.h"

#include "plugininterface.h"
#include "pluginmanager.h"
#include "ssim.h"
#include "textureio.h"

#include "cpimageloader.h"

#if (OPTION_CMP_QT == 1)
#include "cvmatandqimage.h"
#include <QtCore/QCoreApplication>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qstring.h>
#include <QtCore/qlist.h>
#include <QtCore/qfileinfo.h>
#include <QtGui/qcolor.h>
#include <QtGui/qimage.h>
#include <QtCore/qmath.h>
#endif

#if (OPTION_CMP_OPENCV == 1)
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>  // Gaussian Blur
#include <opencv2/highgui/highgui.hpp>  // OpenCV window I/O
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <memory>
#include <string>

using namespace std;

#ifdef _DEBUG
#define CMP_EXTERNAL_LibExt "d.lib"
#else
#define CMP_EXTERNAL_LibExt ".lib"
#endif

#if (OPTION_CMP_OPENCV == 1)
#if ((CV_VERSION_EPOCH == 2) || (CV_VERSION_MAJOR < 4))
#if defined(_WIN32)
#define OpenCV_core_Lib "opencv_core" CVAUX_STR(CV_VERSION_EPOCH) CVAUX_STR(CV_VERSION_MAJOR) CVAUX_STR(CV_VERSION_MINOR) CMP_EXTERNAL_LibExt
#define OpenCV_highgui_Lib "opencv_highgui" CVAUX_STR(CV_VERSION_EPOCH) CVAUX_STR(CV_VERSION_MAJOR) CVAUX_STR(CV_VERSION_MINOR) CMP_EXTERNAL_LibExt
#define OpenCV_imgproc_Lib "opencv_imgproc" CVAUX_STR(CV_VERSION_EPOCH) CVAUX_STR(CV_VERSION_MAJOR) CVAUX_STR(CV_VERSION_MINOR) CMP_EXTERNAL_LibExt
#else
#define OpenCV_core_Lib "opencv_core" CVAUX_STR(CV_VERSION_MAJOR) CVAUX_STR(CV_VERSION_MINOR) CVAUX_STR(CV_VERSION_REVISION) CMP_EXTERNAL_LibExt
#define OpenCV_highgui_Lib "opencv_highgui" CVAUX_STR(CV_VERSION_MAJOR) CVAUX_STR(CV_VERSION_MINOR) CVAUX_STR(CV_VERSION_REVISION) CMP_EXTERNAL_LibExt
#define OpenCV_imgproc_Lib "opencv_imgproc" CVAUX_STR(CV_VERSION_MAJOR) CVAUX_STR(CV_VERSION_MINOR) CVAUX_STR(CV_VERSION_REVISION) CMP_EXTERNAL_LibExt
#endif

#pragma comment(lib, OpenCV_core_Lib)
#pragma comment(lib, OpenCV_highgui_Lib)
#pragma comment(lib, OpenCV_imgproc_Lib)
#else
#define OpenCV_world_Lib "opencv_world" CVAUX_STR(CV_VERSION_MAJOR) CVAUX_STR(CV_VERSION_MINOR) CVAUX_STR(CV_VERSION_REVISION) CMP_EXTERNAL_LibExt
#pragma comment(lib, OpenCV_world_Lib)
#endif
#endif

#if (OPTION_CMP_QT == 1)
#define Qt5_core_Lib "Qt5Core" CMP_EXTERNAL_LibExt
#define Qt5_gui_Lib "Qt5Gui" CMP_EXTERNAL_LibExt
#define Qt5_widgets_Lib "Qt5Widgets" CMP_EXTERNAL_LibExt

#pragma comment(lib, Qt5_core_Lib)
#pragma comment(lib, Qt5_gui_Lib)
#pragma comment(lib, Qt5_widgets_Lib)
#endif

#ifdef BUILD_AS_PLUGIN_DLL
DECLARE_PLUGIN(Plugin_Canalysis)
SET_PLUGIN_TYPE("IMAGE")
SET_PLUGIN_NAME("ANALYSIS")
#else
void* make_Plugin_CAnalysis()
{
    return new Plugin_Canalysis;
}
#endif

#define TEST_TOLERANCE 5  //for 4x4 test block omly

Plugin_Canalysis::Plugin_Canalysis()
{
    //default tolerance values
    tolerance_mse   = 1.0100;
    tolerance_psnr  = 0.9900;
    tolerance_psnrb = 0.9900;
    tolerance_psnrg = 0.9900;
    tolerance_psnrr = 0.9900;
    tolerance_ssim  = 0.9995;
    tolerance_ssimb = 0.9995;
    tolerance_ssimg = 0.9995;
    tolerance_ssimr = 0.9995;

#if (OPTION_CMP_QT == 1)
    m_imageloader = NULL;
#endif

    m_MipSrcImages  = NULL;
    m_MipDestImages = NULL;
    m_MipDiffImages = NULL;
    m_RGBAChannels  = 0b1111;
}

Plugin_Canalysis::~Plugin_Canalysis()
{
#if (OPTION_CMP_QT == 1)
    if (m_MipSrcImages)
        m_imageloader->clearMipImages(&m_MipSrcImages);

    if (m_MipDestImages)
        m_imageloader->clearMipImages(&m_MipDestImages);
    if (m_imageloader)
        delete m_imageloader;
#endif
}

int Plugin_Canalysis::TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)
{
    pPluginVersion->dwAPIVersionMajor    = TC_API_VERSION_MAJOR;
    pPluginVersion->dwAPIVersionMinor    = TC_API_VERSION_MINOR;
    pPluginVersion->dwPluginVersionMajor = TC_PLUGIN_VERSION_MAJOR;
    pPluginVersion->dwPluginVersionMinor = TC_PLUGIN_VERSION_MINOR;
    return 0;
}

std::string f2Str(float data, int prec)
{
    std::string s(16, '\0');
    char        pstr[6] = "%.xf";
    pstr[2]             = 48 + (prec % 10);  // prec is limited from 0..9
    std::snprintf(&s[0], s.size(), pstr, data);
    return s;
}

void Plugin_Canalysis::write(const REPORT_DATA& data, char* resultsFile, char option)
{
    rapidxml::file<>*        xmlResultsFile = nullptr;
    rapidxml::file<>*        xmlGoldenFile  = nullptr;
    rapidxml::xml_document<> xmlDoc;
    rapidxml::xml_document<> xmlGoldDoc;

    std::string diffName     = "";
    std::string diffNodeName = "";
    bool        nodeExist    = false;

    if (m_srcFile.size() > 0 && m_destFile.size() > 0)
    {
        diffName = CMP_GetJustFileName(m_srcFile);
        diffName.append("_");
        diffName.append(CMP_GetJustFileName(m_destFile));
        diffNodeName = diffName;
    }

    auto allocateNewElement = [&](rapidxml::xml_node<>* parent, const std::string name, std::string data) {
        rapidxml::xml_node<>* newElement = nullptr;
        char*                 pName      = parent->document()->allocate_string(name.c_str());
        char*                 pData      = parent->document()->allocate_string(data.c_str());
        newElement                       = xmlDoc.allocate_node(rapidxml::node_type::node_element, pName, pData);
        parent->append_node(newElement);
    };

    auto modifyElement = [&](rapidxml::xml_node<>* parent, const std::string name, std::string data) {
        char* pName = parent->document()->allocate_string(name.c_str());
        char* pData = parent->document()->allocate_string(data.c_str());
        parent->first_node(pName)->value(pData);
    };

    if (CMP_FileExists(resultsFile))
    {
        // Create and parse an xml file (note: we do not want data nodes)
        xmlResultsFile = new rapidxml::file<>(resultsFile);
        xmlDoc.parse<rapidxml::parse_no_data_nodes>(xmlResultsFile->data());

        // traverse pt
        rapidxml::xml_node<>* levelElement = xmlDoc.first_node("ANALYSIS");
        for (rapidxml::xml_node<>* child = levelElement->first_node(); child != nullptr; child = child->next_sibling())
        {
            if (std::string(child->name()) == "DATA")
            {  //gui
                if (option == 's')
                {  //ssim
                    modifyElement(child, "SSIM", f2Str(data.SSIM, 4));
                    modifyElement(child, "SSIM_BLUE", f2Str(data.SSIM_Blue, 4));
                    modifyElement(child, "SSIM_GREEN", f2Str(data.SSIM_Green, 4));
                    modifyElement(child, "SSIM_RED", f2Str(data.SSIM_Red, 4));
                    nodeExist = true;
                }
                else if (option == 'p')
                {  //psnr
                    modifyElement(child, "MSE", f2Str(data.MSE, 4).c_str());
                    modifyElement(child, "PSNR", f2Str(data.PSNR, 1).c_str());
                    modifyElement(child, "PSNR_BLUE", f2Str(data.PSNR_Blue, 1).c_str());
                    modifyElement(child, "PSNR_GREEN", f2Str(data.PSNR_Green, 1).c_str());
                    modifyElement(child, "PSNR_RED", f2Str(data.PSNR_Red, 1).c_str());
                    nodeExist = true;
                }
                else
                {
                    modifyElement(child, "MSE", f2Str(data.MSE, 1).c_str());
                    modifyElement(child, "SSIM", f2Str(data.SSIM, 4).c_str());
                    modifyElement(child, "SSIM_BLUE", f2Str(data.SSIM_Blue, 4).c_str());
                    modifyElement(child, "SSIM_GREEN", f2Str(data.SSIM_Green, 4).c_str());
                    modifyElement(child, "SSIM_RED", f2Str(data.SSIM_Red, 4).c_str());
                    modifyElement(child, "PSNR", f2Str(data.PSNR, 1).c_str());
                    modifyElement(child, "PSNR_BLUE", f2Str(data.PSNR_Blue, 1).c_str());
                    modifyElement(child, "PSNR_GREEN", f2Str(data.PSNR_Green, 1).c_str());
                    modifyElement(child, "PSNR_RED", f2Str(data.PSNR_Red, 1).c_str());
                    nodeExist = true;
                    break;
                }
            }
            else if (std::string(child->name()) == diffName)
            {  //cmdline, node exist
                modifyElement(child, "MSE", f2Str(data.MSE, 1).c_str());
                modifyElement(child, "SSIM", f2Str(data.SSIM, 4).c_str());
                modifyElement(child, "SSIM_BLUE", f2Str(data.SSIM_Blue, 4).c_str());
                modifyElement(child, "SSIM_GREEN", f2Str(data.SSIM_Green, 4).c_str());
                modifyElement(child, "SSIM_RED", f2Str(data.SSIM_Red, 4).c_str());
                modifyElement(child, "PSNR", f2Str(data.PSNR, 1).c_str());
                modifyElement(child, "PSNR_BLUE", f2Str(data.PSNR_Blue, 1).c_str());
                modifyElement(child, "PSNR_GREEN", f2Str(data.PSNR_Green, 1).c_str());
                modifyElement(child, "PSNR_RED", f2Str(data.PSNR_Red, 1).c_str());
                if (data.srcdecodePattern[0] != '\0')
                {
                    modifyElement(child, "FAIL_SRCDECODEPATTERN", data.srcdecodePattern);
                }
                else
                {
                    while (rapidxml::xml_node<>* grandChild = child->first_node("FAIL_SRCDECODEPATTERN"))
                        child->remove_node(grandChild);
                }

                if (data.destdecodePattern[0] != '\0')
                {
                    modifyElement(child, "FAIL_DESTDECODEPATTERN", data.destdecodePattern);
                }
                else
                {
                    while (rapidxml::xml_node<>* grandChild = child->first_node("FAIL_DESTDECODEPATTERN"))
                        child->remove_node(grandChild);
                }

                nodeExist = true;
                break;
            }
        }

        if (!nodeExist)
        {  //cmdline, node not exist
            rapidxml::xml_node<>* newTree = xmlDoc.allocate_node(rapidxml::node_type::node_element, diffNodeName.c_str());
            levelElement->append_node(newTree);

            allocateNewElement(newTree, "MSE", f2Str(data.MSE, 1));
            allocateNewElement(newTree, "SSIM", f2Str(data.SSIM, 4));
            allocateNewElement(newTree, "SSIM_BLUE", f2Str(data.SSIM_Blue, 4));
            allocateNewElement(newTree, "SSIM_GREEN", f2Str(data.SSIM_Green, 4));
            allocateNewElement(newTree, "SSIM_RED", f2Str(data.SSIM_Red, 4));
            allocateNewElement(newTree, "PSNR", f2Str(data.PSNR, 1));
            allocateNewElement(newTree, "PSNR_BLUE", f2Str(data.PSNR_Blue, 1));
            allocateNewElement(newTree, "PSNR_GREEN", f2Str(data.PSNR_Green, 1));
            allocateNewElement(newTree, "PSNR_RED", f2Str(data.PSNR_Red, 1));

            if (data.srcdecodePattern[0] != '\0')
                allocateNewElement(newTree, "FAIL_SRCDECODEPATTERN", data.srcdecodePattern);

            if (data.destdecodePattern[0] != '\0')
                allocateNewElement(newTree, "FAIL_DESTDECODEPATTERN", data.destdecodePattern);
        }
    }
    else
    {  //file not exist
        rapidxml::xml_node<>* decl = xmlDoc.allocate_node(rapidxml::node_declaration);
        decl->append_attribute(xmlDoc.allocate_attribute("version", "1.0"));
        decl->append_attribute(xmlDoc.allocate_attribute("encoding", "UTF-8"));
        xmlDoc.append_node(decl);

        rapidxml::xml_node<>* rootNode = xmlDoc.allocate_node(rapidxml::node_type::node_element, "ANALYSIS");
        xmlDoc.append_node(rootNode);
        if (option == 's' || option == 'p')
        {  //only gui will have option ssim or psnr
            rapidxml::xml_node<>* newTree = xmlDoc.allocate_node(rapidxml::node_type::node_element, "DATA");
            rootNode->append_node(newTree);

            allocateNewElement(newTree, "MSE", f2Str(data.MSE, 1));
            allocateNewElement(newTree, "SSIM", f2Str(data.SSIM, 4));
            allocateNewElement(newTree, "SSIM_BLUE", f2Str(data.SSIM_Blue, 4));
            allocateNewElement(newTree, "SSIM_GREEN", f2Str(data.SSIM_Green, 4));
            allocateNewElement(newTree, "SSIM_RED", f2Str(data.SSIM_Red, 4));
            allocateNewElement(newTree, "PSNR", f2Str(data.PSNR, 1));
            allocateNewElement(newTree, "PSNR_BLUE", f2Str(data.PSNR_Blue, 1));
            allocateNewElement(newTree, "PSNR_GREEN", f2Str(data.PSNR_Green, 1));
            allocateNewElement(newTree, "PSNR_RED", f2Str(data.PSNR_Red, 1));
        }
        else
        {  //cmdline
            rapidxml::xml_node<>* newTree = xmlDoc.allocate_node(rapidxml::node_type::node_element, diffNodeName.c_str());
            rootNode->append_node(newTree);

            allocateNewElement(newTree, "MSE", f2Str(data.MSE, 1));
            allocateNewElement(newTree, "SSIM", f2Str(data.SSIM, 4));
            allocateNewElement(newTree, "SSIM_BLUE", f2Str(data.SSIM_Blue, 4));
            allocateNewElement(newTree, "SSIM_GREEN", f2Str(data.SSIM_Green, 4));
            allocateNewElement(newTree, "SSIM_RED", f2Str(data.SSIM_Red, 4));
            allocateNewElement(newTree, "PSNR", f2Str(data.PSNR, 1));
            allocateNewElement(newTree, "PSNR_BLUE", f2Str(data.PSNR_Blue, 1));
            allocateNewElement(newTree, "PSNR_GREEN", f2Str(data.PSNR_Green, 1));
            allocateNewElement(newTree, "PSNR_RED", f2Str(data.PSNR_Red, 1));
            if (data.srcdecodePattern[0] != '\0')
                allocateNewElement(newTree, "FAIL_SRCDECODEPATTERN", data.srcdecodePattern);

            if (data.destdecodePattern[0] != '\0')
                allocateNewElement(newTree, "FAIL_DESTDECODEPATTERN", data.destdecodePattern);
        }
    }

    // Write the contents to a string and close the document
    std::string xmlAsString;
    rapidxml::print(std::back_inserter(xmlAsString), xmlDoc);
    xmlDoc.clear();
    delete xmlResultsFile;

    // Save to file
    try
    {
        std::ofstream output(resultsFile);
        output << xmlAsString.c_str();
        output.close();
    }
    catch (std::exception const& ex)
    {
        printf("Can't write xml. %s", ex.what());
        return;
    }

    std::string result    = CMP_GetPath(resultsFile);
    int         lastindex = (int)result.find_last_of("/");
    std::string goldFile  = result.substr(0, lastindex + 1);
    goldFile.append("golden.xml");

    std::string toleranceFile = result.substr(0, lastindex + 1);
    toleranceFile.append("analysis_tolerance.xml");

    if ((CMP_FileExists(toleranceFile)))
    {
        rapidxml::file<>         xmlToleranceFile(toleranceFile.c_str());
        rapidxml::xml_document<> xmlToleranceDoc;

        try
        {
            xmlToleranceDoc.parse<0>(xmlToleranceFile.data());
        }
        catch (std::exception exc)
        {
            printf("Can't read golden.xml. %s", exc.what());
            return;
        }

        rapidxml::xml_node<>* levelElement = xmlToleranceDoc.first_node("ANALYSIS");
        for (rapidxml::xml_node<>* child = levelElement->first_node(); child != NULL; child = child->next_sibling())
        {
            if (std::string(child->name()) == "TOLERANCE")
            {
                tolerance_mse   = 1.0000 + std::stod(child->first_node("MSE")->value());
                tolerance_ssim  = 1.0000 - std::stod(child->first_node("SSIM")->value());
                tolerance_ssimb = 1.0000 - std::stod(child->first_node("SSIM_BLUE")->value());
                tolerance_ssimg = 1.0000 - std::stod(child->first_node("SSIM_GREEN")->value());
                tolerance_ssimr = 1.0000 - std::stod(child->first_node("SSIM_RED")->value());
                tolerance_psnr  = 1.0000 - std::stod(child->first_node("PSNR")->value());
                tolerance_psnrb = 1.0000 - std::stod(child->first_node("PSNR_BLUE")->value());
                tolerance_psnrg = 1.0000 - std::stod(child->first_node("PSNR_GREEN")->value());
                tolerance_psnrr = 1.0000 - std::stod(child->first_node("PSNR_RED")->value());
                break;
            }
        }
    }
    //analysis against golden.xml (gold reference) and reporting result
    if ((CMP_FileExists(goldFile)))
    {
        try
        {
            xmlGoldenFile = new rapidxml::file<>(goldFile.c_str());
            xmlGoldDoc.parse<0>(xmlGoldenFile->data());
        }
        catch (std::exception exc)
        {
            delete xmlGoldenFile;
            printf("Can't read golden.xml. %s", exc.what());
            return;
        }

        try
        {
            xmlResultsFile = new rapidxml::file<>(resultsFile);
            xmlDoc.parse<0>(xmlResultsFile->data());
        }
        catch (std::exception exc)
        {
            delete xmlResultsFile;
            printf("Can't read results.xml. %s", exc.what());
            return;
        }

        auto checkFalureGoldAndResults =
            [&](rapidxml::xml_node<>* compare, rapidxml::xml_node<>* parent, const char* refName, const char* name, double value, bool gte = true) {
                allocateNewElement(parent, refName, compare->first_node(name)->value());
                if (gte)
                {
                    if (std::stod(parent->first_node(name)->value()) >= (std::stod(compare->first_node(name)->value()) * value))
                    {
                        allocateNewElement(parent, std::string(name) + "_RESULT", "PASS");
                        return false;
                    }
                }
                else
                {
                    if (std::stod(parent->first_node(name)->value()) <= (std::stod(compare->first_node(name)->value()) * value))
                    {
                        allocateNewElement(parent, refName, "PASS");
                        return false;
                    }
                }

                allocateNewElement(parent, refName, "FAIL");
                return true;
            };

        int  pass    = 0;
        int  fail    = 0;
        int  total   = 0;
        bool hasFail = false;
        // traverse pt for both golden and test files
        rapidxml::xml_node<>* goldLevelElement = xmlGoldDoc.first_node("ANALYSIS");
        for (rapidxml::xml_node<>* goldChild = goldLevelElement->first_node(); goldChild != NULL; goldChild = goldChild->next_sibling())
        {
            rapidxml::xml_node<>* levelElement = xmlDoc.first_node("ANALYSIS");
            for (rapidxml::xml_node<>* child = levelElement->first_node(); child != NULL; child = child->next_sibling())
            {
                if (std::string(goldChild->name()) == std::string(child->name()))
                {  //diff file node exist on both
                    total++;
                    if (checkFalureGoldAndResults(child, goldChild, "MSE_GOLDREF", "MSE", tolerance_mse, false))
                        hasFail = true;
                    if (checkFalureGoldAndResults(child, goldChild, "SSIM_GOLDREF", "SSIM", tolerance_ssim))
                        hasFail = true;
                    if (checkFalureGoldAndResults(child, goldChild, "SSIM_BLUE_GOLDREF", "SSIM_BLUE", tolerance_ssimb))
                        hasFail = true;
                    if (checkFalureGoldAndResults(child, goldChild, "SSIM_GREEN_GOLDREF", "SSIM_GREEN", tolerance_ssimg))
                        hasFail = true;
                    if (checkFalureGoldAndResults(child, goldChild, "SSIM_RED_GOLDREF", "SSIM_RED", tolerance_ssimr))
                        hasFail = true;
                    if (checkFalureGoldAndResults(child, goldChild, "PSNR_GOLDREF", "PSNR", tolerance_psnr))
                        hasFail = true;
                    if (checkFalureGoldAndResults(child, goldChild, "PSNR_BLUE_GOLDREF", "PSNR_BLUE", tolerance_psnrb))
                        hasFail = true;
                    if (checkFalureGoldAndResults(child, goldChild, "PSNR_GREEN_GOLDREF", "PSNR_GREEN", tolerance_psnrg))
                        hasFail = true;
                    if (checkFalureGoldAndResults(child, goldChild, "PSNR_RED_GOLDREF", "PSNR_RED", tolerance_psnrr))
                        hasFail = true;
                    if (checkFalureGoldAndResults(child, goldChild, "MSE_GOLDREF", "MSE", tolerance_mse))
                        hasFail = true;

                    //if one of the above statistic fail, the test case (per image) is fail
                    if (hasFail)
                    {
                        hasFail = false;
                        fail++;
                    }
                    else if (!hasFail)
                    {
                        pass++;
                    }
                }
            }
        }

        // Free the golden xml
        xmlGoldDoc.clear();
        delete xmlGoldenFile;

        if (option != 's' && option != 'p')
        {  //only gui will have this option - s= ssim or p= psnr
            rapidxml::xml_node<>* root = xmlDoc.first_node("ANALYSIS");
            if (!root)
            {
                root = xmlDoc.allocate_node(rapidxml::node_type::node_element, "ANALYSIS");
                xmlDoc.append_node(root);
            }

            rapidxml::xml_node<>* newTree = xmlDoc.allocate_node(rapidxml::node_type::node_element, "TEST_RESULT");
            root->append_node(newTree);

            newTree->first_node("PASS")->value(std::to_string(pass).c_str());
            newTree->first_node("FAIL")->value(std::to_string(fail).c_str());
            newTree->first_node("TOTAL")->value(std::to_string(total).c_str());

            // Write the contents to a string and close the document
            std::string xmlAsString;
            rapidxml::print(std::back_inserter(xmlAsString), xmlDoc);
            xmlDoc.clear();
            delete xmlResultsFile;

            // Save to file
            try
            {
                std::ofstream output(resultsFile);
                output << xmlAsString.c_str();
                output.close();
            }
            catch (std::exception const& ex)
            {
                printf("Can't write xml. %s", ex.what());
                return;
            }
        }
    }
}

void checkPattern(int* r, int* g, int* b, char* pattern, CMP_FORMAT format)
{
    if (format == CMP_FORMAT_ATI1N)
    {  //BC4
        // only test on red channel output
        if (*r <= (0 + TEST_TOLERANCE))
            *pattern = '2';
        else if (*r > 0)
            *pattern = '1';
        else
            *pattern = '8';
    }
    else
    {
        // 1=Red 2=Green 3=R+G 4=Blue 5=R+B 6=G+B 7=R+G+B
        if (*r <= (0 + TEST_TOLERANCE) && *g <= (0 + TEST_TOLERANCE) && *b <= (0 + TEST_TOLERANCE))
            *pattern = '0';
        else if (*r > 0 && *g <= (0 + TEST_TOLERANCE) && *b <= (0 + TEST_TOLERANCE))
            *pattern = '1';
        else if (*r <= (0 + TEST_TOLERANCE) && *g > 0 && *b <= (0 + TEST_TOLERANCE))
            *pattern = '2';
        else if (*r <= (0 + TEST_TOLERANCE) && *g <= (0 + TEST_TOLERANCE) && *b > 0)
            *pattern = '4';
        else if (*r > 0 && *g > 0 && *b <= (0 + TEST_TOLERANCE))
            *pattern = '3';
        else if (*r > 0 && *g <= (0 + TEST_TOLERANCE) && *b > 0)
            *pattern = '5';
        else if (*r <= (0 + TEST_TOLERANCE) && *g > 0 && *b > 0)
            *pattern = '6';
        else if (*r > 0 && *g > 0 && *b > 0)
            *pattern = '7';
        else
            *pattern = '8';
    }
}

void Plugin_Canalysis::generateBCtestResult(QImage* src, QImage* dest, REPORT_DATA& myReport)
{
#if (OPTION_CMP_QT == 1)
    int srcR = 0, srcG = 0, srcB = 0;
    int destR = 0, destG = 0, destB = 0;

    if (m_MipDestImages != NULL)
        m_Compressformat = m_MipDestImages->mipset->m_format;
    char srcPattern[17];
    char destPattern[17];
    memset(srcPattern, 0, 17);
    memset(destPattern, 0, 17);

    //form decode string for src amd dest image
    int index = 0;
    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            QRgb srcPixel = src->pixel(x, y);
            QRgb dstPixel = dest->pixel(x, y);
            srcR          = qRed(srcPixel);
            srcG          = qGreen(srcPixel);
            srcB          = qBlue(srcPixel);

            destR = qRed(dstPixel);
            destG = qGreen(dstPixel);
            destB = qBlue(dstPixel);

            checkPattern(&srcR, &srcG, &srcB, &srcPattern[index], m_Compressformat);
            checkPattern(&destR, &destG, &destB, &destPattern[index], m_Compressformat);
            index++;
        }
    }

    //compare and if not equal , write to myReport.decodePattern
    if (strcmp(srcPattern, destPattern) != 0)
    {
        strncpy(myReport.srcdecodePattern, srcPattern, 16);
        strncpy(myReport.destdecodePattern, destPattern, 16);
    }
#endif
}

#if (OPTION_CMP_OPENCV == 1)
/*
bool Plugin_Canalysis::psnr(QImage *src, const cv::Mat& srcimg, QImage *dest, const cv::Mat& destimg, REPORT_DATA &myReport, CMP_Feedback_Proc pFeedbackProc) {
    double bMSE = 0, gMSE = 0, rMSE = 0, MSE = 0;
    double MAX = 255.0; // Maximum possible pixel range. For our BMP's, which have 8 bits, it's 255.
    int w = src->width();
    int h = src->height();
    float fProgress = 0.0;

    if (w == 4 && h == 4)
        generateBCtestResult(src, dest, myReport);

    unsigned char dR,dG,dB;
    unsigned char sR,sG,sB;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            sB = qBlue (src->pixel(x, y));
            sG = qGreen(src->pixel(x, y));
            sR = qRed  (src->pixel(x, y));

            dB = qBlue (dest->pixel(x, y));
            dG = qGreen(dest->pixel(x, y));
            dR = qRed  (dest->pixel(x, y));

            rMSE += pow(sR - dR, 2.0);
            gMSE += pow(sG - dG, 2.0);
            bMSE += pow(sB - dB, 2.0);

        }

        if (pFeedbackProc) {
            fProgress = 100.f * (y * w) / (w * h);
            if (pFeedbackProc(fProgress, NULL, NULL)) {
                printf("Analysis canceled!\n");
                return false; //abort
            }
        }
    }


    //------------------- Code needs validation
    // double mse;
    // double psnr;
    // getMSE_PSNR( srcimg, destimg, mse, psnr);
    //-------------------

    bMSE *= (1.0 / (w*h));
    gMSE *= (1.0 / (w*h));
    rMSE *= (1.0 / (w*h));

    myReport.PSNR_Blue  = -1;
    myReport.PSNR_Green = -1;
    myReport.PSNR_Red   = -1;

    switch (m_RGBAChannels) {
    case 0b0001:
        myReport.MSE        = rMSE;
        if (rMSE != 0)
            myReport.PSNR_Red = 20 * log10(pow(2.0, 8.0) - 1) - 10 * log10(rMSE);
        if (myReport.MSE != 0)
            myReport.PSNR = 20 * log10(pow(2.0, 8.0) - 1) - 10 * log10(myReport.MSE);
        break;
    case 0b0011:
        myReport.MSE        = (gMSE + rMSE) / 2;
        if (gMSE != 0)
            myReport.PSNR_Green = 20 * log10(pow(2.0, 8.0) - 1) - 10 * log10(gMSE);
        if (rMSE != 0)
            myReport.PSNR_Red = 20 * log10(pow(2.0, 8.0) - 1) - 10 * log10(rMSE);
        if (myReport.MSE != 0)
            myReport.PSNR = 20 * log10(pow(2.0, 8.0) - 1) - 10 * log10(myReport.MSE);
        break;
    default:
        myReport.MSE        = (bMSE + gMSE + rMSE) / 3;
        if (bMSE != 0)
            myReport.PSNR_Blue = 20 * log10(pow(2.0, 8.0) - 1) - 10 * log10(bMSE);
        if (gMSE != 0)
            myReport.PSNR_Green = 20 * log10(pow(2.0, 8.0) - 1) - 10 * log10(gMSE);
        if (rMSE != 0)
            myReport.PSNR_Red = 20 * log10(pow(2.0, 8.0) - 1) - 10 * log10(rMSE);
        if (myReport.MSE != 0)
            myReport.PSNR = 20 * log10(pow(2.0, 8.0) - 1) - 10 * log10(myReport.MSE);
        break;
    }

    return (myReport.PSNR != -1);
}
*/
#endif

void Plugin_Canalysis::setActiveChannels()
{
    if (m_MipDestImages)
    {
        if (m_MipDestImages->mipset)
        {
            switch (m_MipDestImages->mipset->m_format)
            {
            case CMP_FORMAT_ATI1N:
            case CMP_FORMAT_BC4:
            case CMP_FORMAT_BC4_S:        // All channels are used and equal, Red is used as active channel
                m_RGBAChannels = 0b0001;  // R
                break;
            case CMP_FORMAT_ATI2N_XY:
            case CMP_FORMAT_BC5_S:
            case CMP_FORMAT_BC5:          // Only Red & Green channels active
                m_RGBAChannels = 0b0011;  // ABGR
                break;
            default:
                m_RGBAChannels = 0b0111;  // BGR , alpha skipped
                break;
            }
        }
    }
}

void Plugin_Canalysis::processSSIMResults()
{
    switch (m_RGBAChannels)
    {
    case 0b0001:
        report.data.SSIM_Red = m_SSIM.val[2];
        report.data.SSIM     = report.data.SSIM_Red;
        break;
    case 0b0011:
        report.data.SSIM_Green = m_SSIM.val[1];
        report.data.SSIM_Red   = m_SSIM.val[2];
        report.data.SSIM       = (report.data.SSIM_Green + report.data.SSIM_Red) / 2;
        break;
    default:
        report.data.SSIM_Blue  = m_SSIM.val[0];
        report.data.SSIM_Green = m_SSIM.val[1];
        report.data.SSIM_Red   = m_SSIM.val[2];
        report.data.SSIM       = (report.data.SSIM_Blue + report.data.SSIM_Green + report.data.SSIM_Red) / 3;
        break;
    }
}

int Plugin_Canalysis::TC_ImageDiff(const char*       in1,
                                   const char*       in2,
                                   const char*       out,
                                   char*             resultsFile,
                                   void*             usrAnalysisData,
                                   void*             pluginManager,
                                   void**            cmipImages,
                                   CMP_Feedback_Proc pFeedbackProc)
{
    if (pluginManager == NULL)
        return -1;

#if (OPTION_CMP_QT == 1)
    CMP_ANALYSIS_DATA* analysisData = (CMP_ANALYSIS_DATA*)usrAnalysisData;

    if (m_imageloader == NULL)
        m_imageloader = new CImageLoader(pluginManager);

    QImage*                 srcImage  = NULL;
    QImage*                 destImage = NULL;
    std::unique_ptr<QImage> diffImage;

    if (m_imageloader)
    {
        if (m_MipSrcImages)
            m_imageloader->clearMipImages(&m_MipSrcImages);
        if (m_MipDestImages)
            m_imageloader->clearMipImages(&m_MipDestImages);

        m_MipSrcImages  = m_imageloader->LoadPluginImage(QString::fromUtf8(in1).toStdString());
        m_MipDestImages = m_imageloader->LoadPluginImage(QString::fromUtf8(in2).toStdString());
    }

    if (m_MipSrcImages != NULL && m_MipDestImages != NULL)
    {
        // analyize the destination image type and set active channels to compare the source with
        setActiveChannels();

        // Analysis is only on top MipLevel and first cubemap face!
        // Need to update the code to handle all faces of cubemaps
        if (m_MipSrcImages->QImage_list[0].size() > 0)
        {
            srcImage = m_MipSrcImages->QImage_list[0][0];
        }
        else
        {
            printf("Error: Source Image cannot be loaded\n");
            return -1;
        }

        if (m_MipDestImages->QImage_list[0].size() > 0)
        {
            destImage = m_MipDestImages->QImage_list[0][0];
        }
        else
        {
            printf("Error: Dest Image cannot be loaded\n");
            return -1;
        }
    }
    else
    {
        printf("Error: Image(s) cannot be loaded\n");
        return -1;
    }

    MipSet* mipset_src = (m_MipSrcImages->mipset != NULL) ? m_MipSrcImages->mipset : m_MipSrcImages->decompressedMipSet;
    MipSet* mipset_dst = (m_MipDestImages->decompressedMipSet != NULL) ? m_MipDestImages->decompressedMipSet : m_MipDestImages->mipset;

    if ((mipset_src == NULL) || (mipset_dst == NULL))
    {
        printf("Error: Unable to read mipset data\n");
        return -1;
    }

    // Calculate MSE & PSNR
    CMP_AnalysisData pAnalysisData = {0};
    pAnalysisData.channelBitMap    = m_RGBAChannels;

    if (CMP_MipSetAnlaysis(mipset_src, mipset_dst, 0, 0, &pAnalysisData) != CMP_OK)
    {
        printf("Error: Unable to calculate MSE and PSNR\n");
        return -1;
    }

    report.data.PSNR       = pAnalysisData.psnr;
    report.data.PSNR_Red   = pAnalysisData.psnrR;
    report.data.PSNR_Green = pAnalysisData.psnrG;
    report.data.PSNR_Blue  = pAnalysisData.psnrB;
    report.data.MSE        = pAnalysisData.mse;

    if (analysisData)
    {
        analysisData->PSNR       = pAnalysisData.psnr;
        analysisData->PSNR_Red   = pAnalysisData.psnrR;
        analysisData->PSNR_Green = pAnalysisData.psnrG;
        analysisData->PSNR_Blue  = pAnalysisData.psnrB;
        analysisData->MSE        = pAnalysisData.mse;
    }

    // Test images
    if (srcImage->width() == 4 && srcImage->height() == 4)
    {
        generateBCtestResult(srcImage, destImage, report.data);
        bool testpassed = report.data.PSNR > 0;
        if (!testpassed)
        {
            printf("Error: Images analysis fail\n");
            return -1;
        }
    }

    // Do SSIM using OpenCV
    if (srcImage != NULL && destImage != NULL)
    {
        m_srcFile  = in1;
        m_destFile = in2;

        int w = srcImage->width() == destImage->width() ? srcImage->width() : -1;
        int h = srcImage->height() == destImage->height() ? srcImage->height() : -1;

        if (w == -1 || h == -1)
        {
            printf("Error: Both images must be same size\n");
            return -1;
        }

        if (cmipImages == NULL)
        {  //cmdline enable both ssim and psnr

#if (OPTION_CMP_OPENCV == 1)
            cv::Mat srcimg  = QtOcv::image2Mat(*srcImage);
            cv::Mat destimg = QtOcv::image2Mat(*destImage);
            if (srcimg.empty() || destimg.empty())
            {
                printf("Error: Images fail to allocate for ssim analysis\n");
                return -1;
            }

            m_SSIM = getSSIM(srcimg, destimg, pFeedbackProc);
            processSSIMResults();

            srcimg.release();
            destimg.release();

#endif

            // If we have a report file write to it
            if ((strcmp(resultsFile, "") != 0))
                write(report.data, resultsFile, 'a');

            if (analysisData)
            {
                analysisData->SSIM       = report.data.SSIM;
                analysisData->SSIM_Red   = report.data.SSIM_Red;
                analysisData->SSIM_Green = report.data.SSIM_Green;
                analysisData->SSIM_Blue  = report.data.SSIM_Blue;
            }
        }  //

        if ((strcmp(out, "") != 0))
        {
            diffImage.reset(new QImage(w, h, QImage::Format_ARGB32));  // C++11-compatible ownership
            QColor src;
            QColor dest;
            QColor diff;
            int    r, g, b, a;
            float  fProgress = 0.0;

            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    src  = QColor(srcImage->pixel(x, y));
                    dest = QColor(destImage->pixel(x, y));
                    r = g = b = a = 0;
                    switch (m_RGBAChannels)
                    {
                    case 0b0001:
                        r = qAbs(src.red() - dest.red());
                        break;
                    case 0b0011:
                        r = qAbs(src.red() - dest.red());
                        g = qAbs(src.green() - dest.green());
                        break;
                    default:
                        r = qAbs(src.red() - dest.red());
                        g = qAbs(src.green() - dest.green());
                        b = qAbs(src.blue() - dest.blue());
                        a = qAbs(src.alpha() - dest.alpha());
                        break;
                    }

                    diff.setRed(qMin(r, 255));
                    diff.setGreen(qMin(g, 255));
                    diff.setBlue(qMin(b, 255));
                    diff.setAlpha(qMin(a, 255));
                    diffImage->setPixel(x, y, diff.rgba());
                }

                if (pFeedbackProc)
                {
                    fProgress = 100.f * (y * w) / (w * h);
                    if (pFeedbackProc(fProgress, NULL, NULL))
                    {
                        printf("Analysis canceled!\n");
                        return -1;  //abort
                    }
                }
            }
        }
    }
    else
    {
        printf("Error: Image(s) cannot be loaded\n");
        return -1;
    }

    //-------------------
    // Process Image Diff
    //-------------------
    if (diffImage)
    {
        if ((CMP_FileExists(out)))
        {
            QFile::remove(out);
        }

        bool saved = diffImage->save(out);

        if (saved && cmipImages != NULL)
        {  //gui
            if (m_MipDiffImages)
                m_imageloader->clearMipImages(&m_MipDiffImages);

            m_MipDiffImages = m_imageloader->LoadPluginImage(out);
            if (m_MipDiffImages)
            {
                QFile::remove(out);
                *cmipImages = m_MipDiffImages;
                return 0;
            }
            else
            {
                return -1;
            }
        }
        else if (saved && cmipImages == NULL)
        {  //cmdline version pass in null
            if ((strcmp(out, "") == 0))
            {
                QFile::remove(out);
            }
            return 0;
        }
        else if (!saved && cmipImages != NULL)
        {  //gui- saved fail due to admin right
            if ((strcmp(out, "") == 0))
            {
                return 0;
            }
            else
            {
                // CMP_GetUserResultsDir() returns "" when no writable per-user fallback exists;
                QString appLocalPath = QString::fromStdString(CMP_GetUserResultsDir());
                if (appLocalPath.isEmpty())
                {
                    printf("Error: Image(s) saved failed. No writable fallback location is available.\n");
                    return -1;
                }

                QString redirectOut = appLocalPath + "/diff.bmp";
                saved               = diffImage->save(redirectOut);
                if (saved)
                {
                    if (m_MipDiffImages)
                        m_imageloader->clearMipImages(&m_MipDiffImages);

                    printf("User does not have admin right to the saved path. Diff image has been redirect saved to %s\n", redirectOut.toStdString().c_str());
                    m_MipDiffImages = m_imageloader->LoadPluginImage(redirectOut.toStdString());
                    if (m_MipDiffImages)
                    {
                        QFile::remove(redirectOut);
                        *cmipImages = m_MipDiffImages;
                        return 0;
                    }
                    return -1;
                }
                else
                {
                    printf("Error: Image(s) saved failed. Please run the app as admin.\n");
                    return -1;
                }
            }
        }
        else if (!saved && cmipImages == NULL)
        {  //cmdline version pass in null- saved fail due to admin right
            if ((strcmp(out, "") == 0))
            {
                return 0;
            }
            else
            {
                // CMP_GetUserResultsDir() returns "" when no writable per-user fallback exists;
                QString appLocalPath = QString::fromStdString(CMP_GetUserResultsDir());
                if (appLocalPath.isEmpty())
                {
                    printf("Error: Image(s) saved failed. No writable fallback location is available.\n");
                    return -1;
                }

                QString redirectOut = appLocalPath + "/diff.bmp";
                saved               = diffImage->save(redirectOut);
                if (saved)
                {
                    printf("User does not have admin right to the saved path. Diff image has been redirect saved to %s\n", redirectOut.toStdString().c_str());
                    return 0;
                }
                else
                {
                    printf("Error: Image(s) saved failed. Please run the app as admin.\n");
                    return -1;
                }
            }
        }
        else
        {  //failed to save image diff
            printf("Error: Image(s) diff saved/write failed. Please try run the app as admin.\n");
            return -1;
        }
    }
    else if ((strcmp(out, "") == 0))
        return 0;
    else
        return -1;
#else
    printf("Error: Image Diff is not supported requires Qt\n");
    return -1;
#endif
}

int Plugin_Canalysis::TC_PSNR_MSE(const char* in1, const char* in2, char* resultsFile, void* pluginManager, CMP_Feedback_Proc pFeedbackProc)
{
    if (pluginManager == NULL)
        return -1;

#if (OPTION_CMP_QT == 1)
    if (m_imageloader == NULL)
        m_imageloader = new CImageLoader(pluginManager);

    QImage* srcImage  = NULL;
    QImage* destImage = NULL;

    if (m_imageloader)
    {
        m_MipSrcImages  = m_imageloader->LoadPluginImage(QString::fromUtf8(in1).toStdString());
        m_MipDestImages = m_imageloader->LoadPluginImage(QString::fromUtf8(in2).toStdString());
    }

    if (m_MipSrcImages != NULL && m_MipDestImages != NULL)
    {
        setActiveChannels();

        if (m_MipSrcImages->QImage_list[0].size() > 0)
            srcImage = m_MipSrcImages->QImage_list[0][0];
        else
        {
            printf("Error: Source Image cannot be loaded\n");
            return -1;
        }

        if (m_MipDestImages->QImage_list[0].size() > 0)
            destImage = m_MipDestImages->QImage_list[0][0];
        else
        {
            printf("Error: Dest Image cannot be loaded\n");
            return -1;
        }
    }
    else
    {
        printf("Error: Image(s) cannot be loaded\n");
        return -1;
    }

    if (srcImage != NULL && destImage != NULL)
    {
        int w = srcImage->width() == destImage->width() ? srcImage->width() : -1;
        int h = srcImage->height() == destImage->height() ? srcImage->height() : -1;

        if (w == -1 || h == -1)
        {
            printf("Error: Both images must be same size\n");
            return -1;
        }

        report.data.PSNR_Blue  = -1;
        report.data.PSNR_Green = -1;
        report.data.PSNR_Red   = -1;

        CMP_AnalysisData pAnalysisData = {0};
        pAnalysisData.channelBitMap    = m_RGBAChannels;

        MipSet* mipset_src = (m_MipSrcImages->mipset != NULL) ? m_MipSrcImages->mipset : m_MipSrcImages->decompressedMipSet;
        MipSet* mipset_dst = (m_MipDestImages->decompressedMipSet != NULL) ? m_MipDestImages->decompressedMipSet : m_MipDestImages->mipset;

        if ((mipset_src == NULL) || (mipset_dst == NULL))
        {
            printf("Error: Unable to read mipset data\n");
            return -1;
        }

        if (CMP_MipSetAnlaysis(mipset_src, mipset_dst, 0, 0, &pAnalysisData) != CMP_OK)
        {
            printf("Error: Unable to calculate MSE and PSNR\n");
            return -1;
        }

        report.data.PSNR       = pAnalysisData.psnr;
        report.data.PSNR_Red   = pAnalysisData.psnrR;
        report.data.PSNR_Green = pAnalysisData.psnrG;
        report.data.PSNR_Blue  = pAnalysisData.psnrB;
        report.data.MSE        = pAnalysisData.mse;

        // cv::Mat srcimg  = QtOcv::image2Mat(*srcImage);
        // cv::Mat destimg = QtOcv::image2Mat(*destImage);
        // if (!&srcimg || !&destimg) {
        //     printf("Error: Images fail to allocate for ssim analysis\n");
        //     return -1;
        // }
        //
        // bool testpassed = psnr(srcImage, srcimg, destImage, destimg, report.data);
        // if (!testpassed) {
        //     printf("Error: Images analysis fail\n");
        //     return -1;
        // }

        // Test images
        if (srcImage->width() == 4 && srcImage->height() == 4)
        {
            generateBCtestResult(srcImage, destImage, report.data);
            bool testpassed = report.data.PSNR > 0;
            if (!testpassed)
            {
                printf("Error: Images analysis fail\n");
                return -1;
            }
        }

        write(report.data, resultsFile, 'p');
    }
    else
    {
        printf("Error: Image(s) cannot be loaded\n");
        return -1;
    }
    return 0;
#else
    printf("Error: Image PSNT MSE is not supported requires Qt\n");
    return -1;
#endif
}

int Plugin_Canalysis::TC_SSIM(const char* in1, const char* in2, char* resultsFile, void* pluginManager, CMP_Feedback_Proc pFeedbackProc)
{
    if (pluginManager == NULL)
        return -1;

#if (OPTION_CMP_QT == 1)
    if (m_imageloader == NULL)
        m_imageloader = new CImageLoader(pluginManager);
    QImage* srcImage  = NULL;
    QImage* destImage = NULL;

    if (m_imageloader)
    {
        if (m_MipSrcImages)
            m_imageloader->clearMipImages(&m_MipSrcImages);

        if (m_MipDestImages)
            m_imageloader->clearMipImages(&m_MipDestImages);

        m_MipSrcImages  = m_imageloader->LoadPluginImage(QString::fromUtf8(in1).toStdString());
        m_MipDestImages = m_imageloader->LoadPluginImage(QString::fromUtf8(in2).toStdString());
    }

    if (m_MipSrcImages != NULL && m_MipDestImages != NULL)
    {
        setActiveChannels();
        if (m_MipSrcImages->QImage_list[0].size() > 0)
            srcImage = m_MipSrcImages->QImage_list[0][0];
        else
        {
            printf("Error: Source Image cannot be loaded\n");
            return -1;
        }

        if (m_MipDestImages->QImage_list[0].size() > 0)
            destImage = m_MipDestImages->QImage_list[0][0];
        else
        {
            printf("Error: Dest Image cannot be loaded\n");
            return -1;
        }
    }
    else
    {
        printf("Error: Image(s) cannot be loaded\n");
        return -1;
    }

    if (srcImage != NULL && destImage != NULL)
    {
        int w = srcImage->width() == destImage->width() ? srcImage->width() : -1;
        int h = srcImage->height() == destImage->height() ? srcImage->height() : -1;

        if (w == -1 || h == -1)
        {
            printf("Error: Both images must be same size\n");
            return -1;
        }

        report.data.SSIM_Blue  = 0;
        report.data.SSIM_Green = 0;
        report.data.SSIM_Red   = 0;
        report.data.SSIM       = 0;

#if (OPTION_CMP_OPENCV == 1)
        cv::Mat srcimg  = QtOcv::image2Mat(*srcImage);
        cv::Mat destimg = QtOcv::image2Mat(*destImage);

        if (!&srcimg || !&destimg)
        {
            printf("Error: Images fail to allocate for ssim analysis\n");
            return -1;
        }

        m_SSIM = getSSIM(srcimg, destimg, pFeedbackProc);
        processSSIMResults();

        srcimg.release();
        destimg.release();

#endif
        write(report.data, resultsFile, 's');
    }
    else
    {
        printf("Error: Image(s) cannot be loaded\n");
        return -1;
    }
    return 0;
#else
    printf("Error: Image SSIM is not supported requires Qt\n");
    return -1;
#endif
}
