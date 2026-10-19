//=====================================================================
// Copyright 2020-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef CPPROJECTDATA_H
#define CPPROJECTDATA_H

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#if defined(_MSC_VER)
#pragma warning(disable : 4996)  //'sscanf': This function or variable may be unsafe.
#endif

#ifndef USE_MESHOPTIMIZER
#define USE_MESHOPTIMIZER
#endif

// #define ENABLED_USER_GPUVIEW
//#include "qtpropertymanager.h"
//#include "qtvariantproperty.h"
//#include "qtgroupboxpropertybrowser.h"
#include "objectcontroller.h"
#include <QtWidgets>
#include <QQmlListProperty>
#include "pluginmanager.h"
#include "cpimageloader.h"
#include "atiformats.h"
#include "textureio.h"
#include "common.h"
#include "cmp_fileio.h"
// JSon
#include "json/json.hpp"
// Mesh
#include "cmp_mesh.h"
#include "modeldata.h"

#define TREETYPE_Double_Click_here_to_add_files 0x00000001  // [+] Double Click here to add files ...
#define TREETYPE_Add_destination_setting 0x00000002         // [+] Add destination setting ...
#define TREETYPE_Add_Model_destination_settings 0x00000004  // Create a new copy of 3D source data node
#define TREETYPE_IMAGEFILE_DATA 0x00000008                  // items column (1) uses new allocated varient data for C_FileProperties
#define TREETYPE_3DMODEL_DATA 0x00000010                    // items column (1) uses new allocated varient data for C_FileProperties
#define TREETYPE_VIEWIMAGE_ONLY_NODE 0x00000020             // Autogen data  that is part of a 3D  Image that is only viewed
#define TREETYPE_VIEWMESH_ONLY_NODE 0x00000040              // Autogen data  that is part of a 3D  Image that is only viewed
#define TREETYPE_3DSUBMODEL_DATA 0x00000080                 // items column (1) uses new allocated varient data for C_FileProperties
#define TREETYPE_COMPRESSION_DATA 0x00000100                // Texture saved as part of a 3D model
#define TREETYPE_MESH_DATA 0x00000200                       // Mesh data node contains (Vertices, Index, ...)
#define TREETYPE_DIFFVIEW 0x00000400                        // no item data, this id is used along with above for identifying docked widgets

#define TREE_LevelType 0   // Treeview index of data column variant data storage for TREETYPE_...
#define TREE_SourceInfo 1  // Treeview index of data column variant data storage for Source data

// Per user fallback for processed output when the requested destination folder is read
// only, such as the sample images installed under %ProgramFiles%.
// Returns an empty string when no writable location could be created.
inline QString CMP_GetUserResultsFolder()
{
    return QString::fromStdString(CMP_GetUserResultsDir());
}

// =======================================================
// COMPRESSION DATA
// =======================================================
enum eModelType
{
    GLTF,
    OBJ,
    DRC
};

class x_Options_Controller : public QObject
{
    Q_OBJECT
public:
    ObjectController* m_controller = NULL;
};

#ifdef USECOMPSPEED
enum eCompression_Speed
{
    Normal,
    Fast,
    Super_Fast
};

class Compression_Speed : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool No_Alpha MEMBER No_Alpha)
    Q_PROPERTY(eCompression_Speed Speed READ Speed WRITE setPriority NOTIFY priorityChanged)

public:
    Q_ENUMS(eCompression_Speed)

    Compression_Speed()
    {
        No_Alpha = false;
        m_Speed  = Normal;
    }

    void setPriority(eCompression_Speed priority)
    {
        m_Speed = priority;
        emit priorityChanged(priority);
    }

    eCompression_Speed Speed() const
    {
        return m_Speed;
    }

    bool               No_Alpha;
    eCompression_Speed m_Speed;

signals:
    void priorityChanged(eCompression_Speed);
};
#endif
class C_Input_HDR_Image_Properties : public x_Options_Controller
{
    Q_OBJECT
    Q_PROPERTY(double Defog READ getDefog WRITE setDefog NOTIFY defogChanged)
    Q_PROPERTY(double Exposure READ getExposure WRITE setExposure NOTIFY exposureChanged)
    Q_PROPERTY(double KneeLow READ getKneeLow WRITE setKneeLow NOTIFY kneeLowChanged)
    Q_PROPERTY(double KneeHigh READ getKneeHigh WRITE setKneeHigh NOTIFY kneeHighChanged)
    Q_PROPERTY(double Gamma READ getGamma WRITE setGamma NOTIFY gammaChanged)

public:
    C_Input_HDR_Image_Properties()
    {
        m_Defog    = AMD_CODEC_DEFOG_DEFAULT;
        m_Exposure = AMD_CODEC_EXPOSURE_DEFAULT;
        m_KneeLow  = AMD_CODEC_KNEELOW_DEFAULT;
        m_KneeHigh = AMD_CODEC_KNEEHIGH_DEFAULT;
        m_Gamma    = AMD_CODEC_GAMMA_DEFAULT;
    }

    void setDefog(double defog)
    {
        if (m_Defog == defog)
            return;

        if (defog < 0.000)
        {
            m_Defog = 0.0000;
        }
        else if (defog > 0.0100)
        {
            m_Defog = 0.0100;
        }

        if (m_Defog != defog)
        {
            m_Defog                 = defog;
            m_data_has_been_changed = true;
            emit dataChanged();
        }

        emit defogChanged(m_Defog);
    }

    void setExposure(double exposure)
    {
        if (m_Exposure == exposure)
            return;

        if (exposure < -10.0)
        {
            m_Exposure = -10.0;
        }
        else if (exposure > 10.0)
        {
            m_Exposure = 10.0;
        }

        if (m_Exposure != exposure)
        {
            m_Exposure              = exposure;
            m_data_has_been_changed = true;
            emit dataChanged();
        }

        emit exposureChanged(m_Exposure);
    }

    void setKneeLow(double kl)
    {
        if (m_KneeLow == kl)
            return;

        if (kl < -3.0)
        {
            m_KneeLow = -3.00;
        }
        else if (kl > 3.0)
        {
            m_KneeLow = 3.00;
        }

        if (m_KneeLow != kl)
        {
            m_KneeLow               = kl;
            m_data_has_been_changed = true;
            emit dataChanged();
        }

        emit kneeLowChanged(m_KneeLow);
    }

    void setKneeHigh(double kh)
    {
        if (m_KneeHigh == kh)
            return;

        if (kh < 3.50)
        {
            m_KneeHigh = 3.50;
        }
        else if (kh > 7.50)
        {
            m_KneeHigh = 7.50;
        }

        if (m_KneeHigh != kh)
        {
            m_KneeHigh              = kh;
            m_data_has_been_changed = true;
            emit dataChanged();
        }

        emit kneeHighChanged(m_KneeHigh);
    }

    void setGamma(double gamma)
    {
        if (m_Gamma == gamma)
            return;

        if (gamma < 1.0)
        {
            m_Gamma = 1.0;
        }
        else if (gamma > 2.6)
        {
            m_Gamma = 2.6;
        }

        if (m_Gamma != gamma)
        {
            m_Gamma                 = gamma;
            m_data_has_been_changed = true;
            emit dataChanged();
        }

        emit gammaChanged(m_Gamma);
    }

    double getDefog() const
    {
        return m_Defog;
    }

    double getExposure() const
    {
        return m_Exposure;
    }

    double getKneeLow() const
    {
        return m_KneeLow;
    }

    double getKneeHigh() const
    {
        return m_KneeHigh;
    }

    double getGamma() const
    {
        return m_Gamma;
    }

    double m_Defog;
    double m_Exposure;
    double m_KneeLow;
    double m_KneeHigh;
    double m_Gamma;
    bool   m_data_has_been_changed;

signals:
    void defogChanged(double&);
    void exposureChanged(double&);
    void kneeLowChanged(double&);
    void kneeHighChanged(double&);
    void gammaChanged(double&);
    void dataChanged();
};

class C_Codec_Block : public C_Input_HDR_Image_Properties
//public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString Bitrate READ getBitrate WRITE setBitrate NOTIFY bitrateChanged)

public:
    C_Codec_Block()
    {
        m_Bitrate        = "8.00";
        m_correctBitrate = "8.00";
        xblock           = 4;
        yblock           = 4;
    }

    void setBitrate(QString bitrate)
    {
        if (bitrate == "")
            return;

        if (m_Bitrate != bitrate)
        {
            QRegExp digits("\\d*");  //to check all digits
            //check user input: either bitrate or block numbers
            if (strchr(bitrate.toUtf8().constData(), '.') != NULL || digits.exactMatch(bitrate))
            {
                float bitrateF = bitrate.toFloat();
                if (bitrateF)
                {
                    if (bitrateF > 8.00)
                    {
                        m_Bitrate        = "8.00";
                        m_correctBitrate = "8.00";
                    }
                    else if (bitrateF < 0)
                    {
                        m_Bitrate        = "0.89";
                        m_correctBitrate = "0.89";
                    }
                    else
                    {
                        m_Bitrate        = bitrate;
                        m_correctBitrate = bitrate;
                    }
                    find_closest_blockdim_2d(bitrateF, &xblock, &yblock, 1);
                    m_correctBitrate = bitrateF;
                    emit bitrateChanged(m_Bitrate, xblock, yblock);
                    m_data_has_been_changed = true;
                    emit dataChanged();
                }
            }
            else if ((strchr(bitrate.toUtf8().constData(), 'x') != NULL) || (strchr(bitrate.toUtf8().constData(), 'X') != NULL))
            {  //block number user form input
                int dimensions = sscanf(bitrate.toUtf8().constData(), "%dx%dx", &xblock, &yblock);
                if (dimensions < 2)
                    return;
                find_closest_blockxy_2d(&xblock, &yblock, 0);
                if (xblock < 3 || xblock > 12 || yblock < 3 || yblock > 12)
                {
                    xblock = -1;
                    yblock = -1;
                    emit bitrateChanged(m_Bitrate, xblock, yblock);
                    m_Bitrate        = "8.00";
                    m_correctBitrate = "8.00";
                }
                else
                {
                    float bitrateF         = float(128.0f / (xblock * yblock));
                    m_Bitrate              = bitrate;
                    m_correctBitrate       = QString::number(xblock) + "x" + QString::number(yblock);
                    QString bitrateFString = QString::number(bitrateF, 'f', 2);
                    emit    bitrateChanged(bitrateFString, xblock, yblock);
                    m_data_has_been_changed = true;
                    emit dataChanged();
                }
            }
            else
            {
                xblock = -1;
                yblock = -1;
                emit bitrateChanged(m_Bitrate, xblock, yblock);
                m_Bitrate        = "8.00";
                m_correctBitrate = "8.00";
            }
        }
    }

    QString getBitrate() const
    {
        return m_Bitrate;
    }

    QString m_Bitrate;
    QString m_correctBitrate;
    int     xblock;
    int     yblock;
    bool    m_data_has_been_changed;

signals:
    void bitrateChanged(QString&, int&, int&);
    void dataChanged();
};

class C_Refine : public C_Codec_Block
{
    Q_OBJECT
    //Q_PROPERTY(bool Use_Refine_Steps READ isRefine_Steps WRITE setUseRefine_Steps)
    Q_PROPERTY(int Refine_Steps READ getRefine_Steps WRITE setRefine_Steps NOTIFY Refine_StepsChanged)

public:
    C_Refine()
    {
        init();
    }

    void init()
    {
        Use_Refine_Steps = false;
        Refine_Steps     = 0;
    }

    bool Use_Refine_Steps;
    int  Refine_Steps;

    void setRefine_Steps(int RefineSteps)
    {
        if (Use_Refine_Steps && RefineSteps != Refine_Steps)
        {
            m_data_has_been_changed = true;
            emit dataChanged();
        }
        if (RefineSteps > 8)
            Refine_Steps = 8;
        else if (RefineSteps < 0)
            Refine_Steps = 0;
        else
            Refine_Steps = RefineSteps;

        m_data_has_been_changed = true;
        emit dataChanged();

        emit Refine_StepsChanged((QVariant&)Refine_Steps);
    }

    int getRefine_Steps() const
    {
        return Refine_Steps;
    }

    void setUseRefine_Steps(bool Refine_Steps)
    {
        Use_Refine_Steps        = Refine_Steps;
        m_data_has_been_changed = true;
        emit dataChanged();
    }

    bool isRefine_Steps() const
    {
        return Use_Refine_Steps;
    }

    void setNoRefine_Steps(bool Refine_Steps)
    {
        Use_Refine_Steps        = Refine_Steps;
        m_data_has_been_changed = true;
        emit dataChanged();
    }

    bool isNoRefine_Steps() const
    {
        return !Use_Refine_Steps;
    }

signals:
    void Refine_StepsChanged(QVariant&);
    void dataChanged();
};

class DXT1_Alpha : public C_Refine
//public Compression_Speed
//public C_Codec_Block
{
    Q_OBJECT
    //Q_PROPERTY(bool No_Alpha READ isNoAlpha WRITE setNoAlpha NOTIFY noAlphaChannel)
    //Q_PROPERTY(bool Use_Alpha READ isUseAlpha WRITE setUseAlpha)
    Q_PROPERTY(int Threshold READ getThreshold WRITE setThreshold NOTIFY thresholdChanged)

public:
    DXT1_Alpha()
    {
        init();
    }

    void init()
    {
        Threshold = 0;
    }

    int Threshold;

    void setThreshold(int threshold)
    {
        if (threshold > 255)
            Threshold = 255;

        else if (threshold <= 0)
        {
            Threshold = 0;
        }
        else
        {
            Threshold = threshold;
        }

        m_data_has_been_changed = true;
        emit dataChanged();
        emit thresholdChanged((QVariant&)threshold);
    }

    double getThreshold() const
    {
        return Threshold;
    }

signals:
    void thresholdChanged(QVariant&);
};

class Channel_Weighting : public DXT1_Alpha
{
    Q_OBJECT
    Q_PROPERTY(bool Adaptive MEMBER Adaptive)
    Q_PROPERTY(double X_RED READ getX_Red WRITE setX_Red NOTIFY redwChanged)
    Q_PROPERTY(double Y_GREEN READ getY_Green WRITE setY_Green NOTIFY greenwChanged)
    Q_PROPERTY(double Z_BLUE READ getZ_Blue WRITE setZ_Blue NOTIFY bluewChanged)

public:
    Channel_Weighting()
    {
        Adaptive = false;
        X_RED    = 0.3086;
        Y_GREEN  = 0.6094;
        Z_BLUE   = 0.082;
    }

    bool   Adaptive;
    double X_RED;
    double Y_GREEN;
    double Z_BLUE;

    void setX_Red(double xredw)
    {
        if (X_RED != xredw)
        {
            m_data_has_been_changed = true;
            emit dataChanged();
        }
        if (xredw > 1)
            X_RED = 1;
        else if (xredw < 0)
            X_RED = 0;
        else
            X_RED = xredw;

        emit redwChanged((QVariant&)xredw);
    }

    double getX_Red() const
    {
        return X_RED;
    }

    void setY_Green(double ygreenw)
    {
        if (Y_GREEN != ygreenw)
        {
            m_data_has_been_changed = true;
            emit dataChanged();
        }
        if (ygreenw > 1)
            Y_GREEN = 1;
        else if (ygreenw < 0)
            Y_GREEN = 0;
        else
            Y_GREEN = ygreenw;

        emit greenwChanged((QVariant&)ygreenw);
    }

    double getY_Green() const
    {
        return Y_GREEN;
    }

    void setZ_Blue(double zbluew)
    {
        if (Z_BLUE != zbluew)
        {
            m_data_has_been_changed = true;
            emit dataChanged();
        }
        if (zbluew > 1)
            Z_BLUE = 1;
        else if (zbluew < 0)
            Z_BLUE = 0;
        else
            Z_BLUE = zbluew;

        emit bluewChanged((QVariant&)zbluew);
    }

    double getZ_Blue() const
    {
        return Z_BLUE;
    }

signals:
    void redwChanged(QVariant&);
    void greenwChanged(QVariant&);
    void bluewChanged(QVariant&);
};

#define MESH_SETTINGS_CLASS_NAME "Mesh Settings"
#define MESH_OPTIMIZER_SETTING_CLASS_NAME "Mesh Optimizer Settings"
#define MESH_COMPRESSION_SETTINGS_CLASS_NAME "Mesh Compression Settings"

#define DESTINATION_IMAGE_CLASS_NAME "Destination Image"
#define CHANNEL_WEIGHTING_CLASS_NAME "Channel Weighting"
#define DXT1_ALPHA_CLASS_NAME "DXT1 Alpha"

#if (OPTION_BUILD_ASTC == 1)
#define ASTC_BLOCKRATE_CLASS_NAME "ASTC Block Rate"
#endif

#define CODEC_BLOCK_CLASS_NAME "Codec Block"
#define HDR_PROP_CLASS_NAME "Input HDR Image Properties"
#define REFINE_CLASS_NAME "Refine"

#define DESTINATION_IMAGE_NAME "Name"
#define DESTINATION_IMAGE_FILESIZE "File Size"
#define DESTINATION_IMAGE_NOTPROCESSED "Not Processed"
#define DESTINATION_IMAGE_UNKNOWN "Unknown"

class Mesh_Compression_Settings : public Channel_Weighting
{
    Q_OBJECT
    Q_ENUMS(eMeshCompression)
    Q_PROPERTY(eMeshCompression Compression_Format READ getDo_Mesh_Compression WRITE setDo_Mesh_Compression)
    // Reserved
    //Q_PROPERTY(bool x____Force_Input_as_Point_Cloud  READ  getForce_Input_as_Point_Cloud  WRITE setForce_Input_as_Point_Cloud)      // -point_cloud forces the input to be encoded as a point
    //Q_PROPERTY(bool x____Use_Metadata                READ  getUse_Metadata                WRITE setUse_Metadata)                    //  -mata data use metadata to encode extra information in mesh files.
#ifdef USE_MESH_DRACO_SETTING
    Q_PROPERTY(int x____Compression_Level READ getCompression_Level WRITE setCompression_Level)  // -cl compression level [0-10], most=10, least=0, default=7.
    Q_PROPERTY(int x____Position_Bits READ getPosition_Bits WRITE setPosition_Bits)  // -qp quantization bits for the position attribute, default=14 max 30
    Q_PROPERTY(int x____Tex_Coords_Bits READ getTex_Coords_Bits WRITE
                   setTex_Coords_Bits)  // -qt quantization bits for the texture coordinate attribute, default=12 max 30, disabled = -1
    Q_PROPERTY(int x____Normals_Bits READ getNormals_Bits WRITE
                   setNormals_Bits)  // -qn uantization bits for the normal vector attribute, default=10. max 30, disabled = -1
    Q_PROPERTY(
        int x____Generic_Bits READ getGeneric_Bits WRITE setGeneric_Bits)  // -qg quantization bits for any generic attribute, default=8 max 30, disabled = -1
#endif

public:
    enum eMeshCompression
    {
        NoComp
#ifdef USE_MESH_DRACO_SETTING
        ,
        Draco
#endif
    };

    Mesh_Compression_Settings()
    {
        InitCompSettings();
    }

    void disable_mesh_compression_settings(bool value)
    {
        if (m_controller)
        {
            QtProperty* prop = m_controller->getProperty("    Force Input as Point Cloud");
            if (prop)
                prop->setHidden(value);

            prop = m_controller->getProperty("    Use Metadata");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("    Compression Level");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("    Position Bits");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("    Tex Coords Bits");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("    Normals Bits");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("    Generic Bits");
            if (prop)
                prop->setHidden(value);
        }
    }

    void setDo_Mesh_Compression(eMeshCompression value)
    {
        m_Do_Mesh_Compression = value;
        disable_mesh_compression_settings(value == eMeshCompression::NoComp);
        emit onMesh_Compression((QVariant&)value);
    }

    eMeshCompression getDo_Mesh_Compression()
    {
        return m_Do_Mesh_Compression;
    }

    void setForce_Input_as_Point_Cloud(bool value)
    {
        m_DracoOptions.is_point_cloud = value;
    }
    bool getForce_Input_as_Point_Cloud()
    {
        return m_DracoOptions.is_point_cloud;
    }

    void setUse_Metadata(bool value)
    {
        m_DracoOptions.use_metadata = value;
    }
    bool getUse_Metadata()
    {
        return m_DracoOptions.use_metadata;
    }

    void setCompression_Level(int value)
    {
        if (value < 0)
            m_DracoOptions.compression_level = 0;
        else if (value > 10)
            m_DracoOptions.compression_level = 10;
        else
            m_DracoOptions.compression_level = value;
    }

    int getCompression_Level()
    {
        return m_DracoOptions.compression_level;
    }

    void setPosition_Bits(int value)
    {
        if (value < 1)
            m_DracoOptions.pos_quantization_bits = 1;
        else if (value > 30)
            m_DracoOptions.pos_quantization_bits = 30;
        else
            m_DracoOptions.pos_quantization_bits = value;
    }
    int getPosition_Bits()
    {
        return m_DracoOptions.pos_quantization_bits;
    }

    void setTex_Coords_Bits(int value)
    {
        if (value < 1)
            m_DracoOptions.tex_coords_quantization_bits = 1;
        else if (value > 30)
            m_DracoOptions.tex_coords_quantization_bits = 30;
        else
            m_DracoOptions.tex_coords_quantization_bits = value;
    }

    int getTex_Coords_Bits()
    {
        return m_DracoOptions.tex_coords_quantization_bits;
    }

    void setNormals_Bits(int value)
    {
        if (value < 2)
            m_DracoOptions.normals_quantization_bits = 2;
        else if (value > 30)
            m_DracoOptions.normals_quantization_bits = 30;
        else
            m_DracoOptions.normals_quantization_bits = value;
    }
    int getNormals_Bits()
    {
        return m_DracoOptions.normals_quantization_bits;
    }

    void setGeneric_Bits(int value)
    {
        if (value < 1)
            m_DracoOptions.generic_quantization_bits = 1;
        else if (value > 30)
            m_DracoOptions.generic_quantization_bits = 30;
        else
            m_DracoOptions.generic_quantization_bits = value;
    }

    int getGeneric_Bits()
    {
        return m_DracoOptions.generic_quantization_bits;
    }

    void InitCompSettings()
    {
        m_Do_Mesh_Compression = eMeshCompression::NoComp;
    }

    eMeshCompression m_Do_Mesh_Compression;

signals:
    void onMesh_Compression(QVariant& value);

private:
    CMP_DracoOptions m_DracoOptions;
};

#ifdef USE_MESHOPTIMIZER
class Mesh_Optimizer_Settings : public Mesh_Compression_Settings
{
    Q_OBJECT
    Q_ENUMS(eMeshOptimization)
    Q_PROPERTY(eMeshOptimization Optimization_Format READ getDo_Mesh_Optimization WRITE setDo_Mesh_Optimization)
    Q_PROPERTY(bool x____Optimize_Vertex_Cache READ getOptimizeVCacheChecked WRITE setOptimizeVCacheChecked)
    Q_PROPERTY(int x________Cache_Size READ getCacheSize WRITE setCacheSize)
    Q_PROPERTY(bool x____Optimize_Vertex_FIFO READ getOptimizeVCacheFifoChecked WRITE setOptimizeVCacheFifoChecked)
    Q_PROPERTY(int x________FIFO_Cache_Size READ getCacheSizeFifo WRITE setCacheSizeFifo)
    Q_PROPERTY(bool x____Optimize_Overdraw READ getOptimizeOverdrawChecked WRITE setOptimizeOverdrawChecked)
    Q_PROPERTY(double x________ACMR_Threshold READ getACMRThreshold WRITE setACMRThreshold)
    Q_PROPERTY(bool x____Optimize_Vertex_Fetch READ getOptimizeVFetchChecked WRITE setOptimizeVFetchChecked)
    Q_PROPERTY(bool x____Simplify_Mesh READ getMeshSimplifyChecked WRITE setMeshSimplifyChecked)
    Q_PROPERTY(int x________Level_of_Detail READ getLODValue WRITE setLODValue)
    // Resereved for 3.1 release
    //Q_PROPERTY(bool     x____Randomize_Index_Buffer  READ  getRandomIndexBufferChecked     WRITE setRandomIndexBufferChecked)
public:
    enum eMeshOptimization
    {
        NoOpt,
        AutoOpt,
        UserOpt
    };

    Mesh_Optimizer_Settings()
    {
        setDo_Mesh_Optimization(AutoOpt);
        InitOptimizationSettings();
        hide_mesh_compression_settings(false);
    }

    void hide_mesh_compression_settings(bool value)
    {
        if (m_controller)
        {
            QtProperty* prop = m_controller->getProperty("Mesh Compression Settings");
            if (prop)
                prop->setHidden(value);
        }
    }

    void disable_mesh_optimization_setting(bool value)
    {
        if (m_controller)
        {
            QtProperty* prop = m_controller->getProperty("    Optimize Vertex Cache");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("        Cache Size");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("    Optimize Vertex FIFO");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("        FIFO Cache Size");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("    Optimize Overdraw");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("        ACMR Threshold");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("    Optimize Vertex Fetch");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("    Simplify Mesh");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("        Level of Detail");
            if (prop)
                prop->setHidden(value);
            prop = m_controller->getProperty("    Randomize Index Buffer");
            if (prop)
                prop->setHidden(value);
        }
    }

    void setDo_Mesh_Optimization(eMeshOptimization value)
    {
        m_Do_Mesh_Optimization = value;
        disable_mesh_optimization_setting(value != eMeshOptimization::UserOpt);
        emit onMesh_Optimization((QVariant&)value);
    }

    eMeshOptimization getDo_Mesh_Optimization()
    {
        return m_Do_Mesh_Optimization;
    }

    void setRandomIndexBufferChecked(bool value)
    {
        m_runRandomizeIndexBuffer = value;
    }

    bool getRandomIndexBufferChecked()
    {
        return m_runRandomizeIndexBuffer;
    }

    void setOptimizeVCacheChecked(bool value)
    {
        m_runOptimizeVCache = value;
        if (m_controller)
        {
            QtProperty* prop = m_controller->getProperty("        Cache Size");
            if (prop)
                prop->setEnabled(value);
        }
    }

    bool getOptimizeVCacheChecked()
    {
        return m_runOptimizeVCache;
    }

    void setCacheSize(int value)
    {
        if (value < 0)
            m_cacheSize = 1;
        else
            m_cacheSize = value;
    }

    int getCacheSize()
    {
        return m_cacheSize;
    }

    void setOptimizeVCacheFifoChecked(bool value)
    {
        m_runOptimizeVCacheFifo = value;
        if (m_controller)
        {
            QtProperty* prop = m_controller->getProperty("        FIFO Cache Size");
            if (prop)
                prop->setEnabled(value);
        }
    }

    bool getOptimizeVCacheFifoChecked()
    {
        return m_runOptimizeVCacheFifo;
    }

    void setCacheSizeFifo(int value)
    {
        if (value < 0)
            m_cacheSizeFifo = 1;
        else
            m_cacheSizeFifo = value;
    }

    int getCacheSizeFifo()
    {
        return m_cacheSizeFifo;
    }

    void setOptimizeOverdrawChecked(bool value)
    {
        m_runOptimizeOverdraw = value;
        if (m_controller)
        {
            QtProperty* prop = m_controller->getProperty("        ACMR Threshold");
            if (prop)
                prop->setEnabled(value);
        }
    }

    bool getOptimizeOverdrawChecked()
    {
        return m_runOptimizeOverdraw;
    }

    void setACMRThreshold(int value)
    {
        if (value < 1)
            m_acmrThreshold = 1;
        else if (value > 3)
            m_acmrThreshold = 3;
        else
            m_acmrThreshold = value;
    }

    double getACMRThreshold()
    {
        return m_acmrThreshold;
    }

    void setOptimizeVFetchChecked(bool value)
    {
        m_runOptimizeVFetch = value;
    }

    bool getOptimizeVFetchChecked()
    {
        return m_runOptimizeVFetch;
    }

    void setMeshSimplifyChecked(bool value)
    {
        m_runMeshSimplify = value;
        if (m_controller)
        {
            QtProperty* prop = m_controller->getProperty("        Level of Detail");
            if (prop)
                prop->setEnabled(value);
        }
    }

    bool getMeshSimplifyChecked()
    {
        return m_runMeshSimplify;
    }

    void setLODValue(int value)
    {  //LOD = level of details, higher number less triangles drawn
        if (value < 1)
            m_levelofDetails = 1;
        else
            m_levelofDetails = value;
    }

    int getLODValue()
    {
        return m_levelofDetails;
    }

    void setMeshData(CMODEL_DATA& meshData)
    {
        m_ModelData = meshData;
    }

    CMODEL_DATA getMeshData()
    {
        return m_ModelData;
    }

    void InitOptimizationSettings()
    {
        m_runRandomizeIndexBuffer = false;
        m_runOptimizeVCache       = true;
        m_cacheSize               = 16;
        m_runOptimizeVCacheFifo   = false;
        m_cacheSizeFifo           = 16;
        m_runOptimizeOverdraw     = true;
        m_acmrThreshold           = 1.05;
        m_runOptimizeVFetch       = true;
        m_runMeshSimplify         = false;
        m_levelofDetails          = 1;
        m_Do_Mesh_Optimization    = eMeshOptimization::AutoOpt;
    }

    eMeshOptimization m_Do_Mesh_Optimization;

signals:
    void onMesh_Optimization(QVariant& value);

private:
    bool        m_runRandomizeIndexBuffer;
    bool        m_runOptimizeVCache;
    int         m_cacheSize;
    bool        m_runOptimizeVCacheFifo;
    int         m_cacheSizeFifo;
    bool        m_runOptimizeOverdraw;
    double      m_acmrThreshold;
    bool        m_runOptimizeVFetch;
    bool        m_runMeshSimplify;
    int         m_levelofDetails;
    CMODEL_DATA m_ModelData;
};
#else
class Mesh_Settings : public Mesh_Compression_Settings
{
    Q_OBJECT
    Q_ENUMS(eFaceWinding)
    Q_ENUMS(eVCacheOptimizer)
    Q_ENUMS(eOverdrawOptimizer)
    Q_ENUMS(eTootleAlgorithm)
    Q_PROPERTY(QString View_Port READ getViewpointName WRITE setViewpointName)
    Q_PROPERTY(int Clustering READ getClustering WRITE setClustering)
    Q_PROPERTY(int Cache_Size READ getCacheSize WRITE setCacheSize)
    Q_PROPERTY(bool Optimize_VertexMemory READ getOptimizeVertexMemory WRITE setOptimizeVertexMemory)
    Q_PROPERTY(bool Measure_Overdraw READ getMeasureOverdraw WRITE setMeasureOverdraw)
    Q_PROPERTY(eFaceWinding Face_Winding READ getFaceWinding WRITE setFaceWinding)
    Q_PROPERTY(eVCacheOptimizer VCache_Optimizer READ getVCacheOptimizer WRITE setVCacheOptimizer)
    Q_PROPERTY(eOverdrawOptimizer Overdraw_Optimizer READ getOverdrawOptimizer WRITE setOverdrawOptimizer)
    Q_PROPERTY(eTootleAlgorithm Mesh_Optimize_Algorithm READ getAlgorithm WRITE setAlgorithm)
public:
    /// Enumeration for face winding order
    enum eFaceWinding
    {
        CCW = 1,  ///< Face is ordered counter-clockwise
        CW  = 2   ///< Face is ordered clockwise
    };

    /// Enumeration for the algorithm for vertex optimization
    enum eVCacheOptimizer
    {
        VCACHE_AUTO     = 1,  ///< If vertex cache size is less than 7, use TSTRIPS algorithm otherwise TIPSY.
        VCACHE_DIRECT3D = 2,  ///< Use D3DXOptimizeFaces to optimize faces.
        VCACHE_LSTRIPS  = 3,  ///< Build a list like triangle strips to optimize faces.
        VCACHE_TIPSY    = 4   ///< Use TIPSY (the algorithm from SIGGRAPH 2007) to optimize faces.
    };

    /// Enumeration for the algorithm for overdraw optimization.
    enum eOverdrawOptimizer
    {
        OVERDRAW_AUTO     = 1,  ///< Use either Direct3D or raytracing to reorder clusters (depending on the number of clusters).
        OVERDRAW_DIRECT3D = 2,  ///< Use Direct3D rendering to reorder clusters to optimize overdraw (slow O(N^2)).
        OVERDRAW_RAYTRACE = 3,  ///< Use CPU raytracing to reorder clusters to optimize overdraw (slow O(N^2)).
        OVERDRAW_FAST     = 4   ///< Use a fast approximation algorithm (from SIGGRAPH 2007) to reorder clusters.
    };

    //=================================================================================================================================
    /// Enumeration for the choice of test cases for tootle.
    //=================================================================================================================================
    enum eTootleAlgorithm
    {
        TOOTLE_VCACHE_ONLY                 = 1,  // Only perform vertex cache optimization.
        TOOTLE_CLUSTER_VCACHE_OVERDRAW     = 2,  // Call the clustering, optimize vertex cache and overdraw individually.
        TOOTLE_FAST_VCACHECLUSTER_OVERDRAW = 3,  // Call the functions to optimize vertex cache and overdraw individually.  This is using
        //  the algorithm from SIGGRAPH 2007.
        TOOTLE_OPTIMIZE      = 4,  // Call a single function to optimize vertex cache, cluster and overdraw.
        TOOTLE_FAST_OPTIMIZE = 5   // Call a single function to optimize vertex cache, cluster and overdraw using
                                   //  a fast algorithm from SIGGRAPH 2007.
    };

    void setViewpointName(QString vpname)
    {
        m_pViewpointName = vpname;
    }

    QString getViewpointName()
    {
        return m_pViewpointName;
    }

    void setClustering(int value)
    {
        m_Clustering = value;
    }

    int getClustering()
    {
        return m_Clustering;
    }

    void setCacheSize(int value)
    {
        m_CacheSize = value;
    }

    int getCacheSize()
    {
        return m_CacheSize;
    }

    void setFaceWinding(eFaceWinding value)
    {
        m_FaceWinding = value;
    }

    eFaceWinding getFaceWinding()
    {
        return m_FaceWinding;
    }

    void setVCacheOptimizer(eVCacheOptimizer value)
    {
        m_VCacheOptimizer = value;
    }

    eVCacheOptimizer getVCacheOptimizer()
    {
        return m_VCacheOptimizer;
    }

    void setOverdrawOptimizer(eOverdrawOptimizer value)
    {
        m_OverdrawOptimizer = value;
    }

    eOverdrawOptimizer getOverdrawOptimizer()
    {
        return m_OverdrawOptimizer;
    }

    void setAlgorithm(eTootleAlgorithm value)
    {
        m_OptimizeAlgorithm = value;
    }

    eTootleAlgorithm getAlgorithm()
    {
        return m_OptimizeAlgorithm;
    }

    void setMeshData(CMODEL_DATA& meshData)
    {
        m_ModelData = meshData;
    }

    CMODEL_DATA getMeshData()
    {
        return m_ModelData;
    }

    void setMeasureOverdraw(bool value)
    {
        m_MeasureOverdraw = value;
    }

    bool getMeasureOverdraw()
    {
        return m_MeasureOverdraw;
    }

    void setOptimizeVertexMemory(bool value)
    {
        m_OptimizeVertexMemory = value;
    }

    bool getOptimizeVertexMemory()
    {
        return m_OptimizeVertexMemory;
    }

private:
    QString          m_pViewpointName       = "";
    unsigned int     m_Clustering           = 0;
    unsigned int     m_CacheSize            = 14;     // Hardware cache size(12 to 24 are good options).
    bool             m_OptimizeVertexMemory = false;  // true if you want to optimize vertex memory location, false to skip
    bool             m_MeasureOverdraw      = false;  // true if you want to measure overdraw, false to skip
    eFaceWinding     m_FaceWinding          = eFaceWinding::CCW;
    eVCacheOptimizer m_VCacheOptimizer      = eVCacheOptimizer::VCACHE_AUTO;  // the choice for vertex cache optimization algorithm, it can be either
    //  TOOTLE_VCACHE_AUTO, TOOTLE_VCACHE_LSTRIPS, TOOTLE_VCACHE_DIRECT3D or
    //  TOOTLE_VCACHE_TIPSY.
    eOverdrawOptimizer m_OverdrawOptimizer = eOverdrawOptimizer::OVERDRAW_FAST;
    eTootleAlgorithm   m_OptimizeAlgorithm = eTootleAlgorithm::TOOTLE_OPTIMIZE;
    CMODEL_DATA        m_ModelData;  //original mesh data buffer
    // todo: destination meshdata
};

#endif

class C_Destination_Image :
#ifdef USE_MESHOPTIMIZER
    public Mesh_Optimizer_Settings
#else
    public Mesh_Settings
#endif
{
    Q_OBJECT
    Q_PROPERTY(QString _Name MEMBER m_FileInfoDestinationName)
    Q_PROPERTY(QString _File_Size MEMBER m_FileSizeStr)
    Q_PROPERTY(QString _Width MEMBER m_WidthStr)
    Q_PROPERTY(QString _Height MEMBER m_HeightStr)
    Q_PROPERTY(QString _Compression_Ratio MEMBER m_CompressionRatio);
    Q_PROPERTY(QString _Compression_Time MEMBER m_CompressionTimeStr);

public:
    C_Destination_Image()
    {
        m_FileSize                = 0;
        m_DstWidth                = 0;
        m_DstHeight               = 0;
        m_FileSizeStr             = "";
        m_WidthStr                = "";
        m_HeightStr               = "";
        m_FileInfoDestinationName = "";
        m_CompressionRatio        = DESTINATION_IMAGE_UNKNOWN;
        m_CompressionTime         = 0;
        m_CompressionTimeStr      = DESTINATION_IMAGE_NOTPROCESSED;
    }

    QString m_FileInfoDestinationName;
    QString m_CompressionRatio;
    int     m_DstWidth;
    int     m_DstHeight;
    int     m_FileSize;
    double  m_CompressionTime;
    QString m_WidthStr;
    QString m_HeightStr;
    QString m_FileSizeStr;
    QString m_CompressionTimeStr;
};

#define COMPRESS_OPTIONS_QUALITY "Quality"
#ifdef USE_ENABLEHQ
#define COMPRESS_OPTIONS_HIGHQUALITY "EnableHQ"
#endif
#define COMPRESS_OPTIONS_FORMAT "Format"
#define COMPRESS_OPTIONS_MESH "Mesh"
#define COMPRESS_OPTIONS_CHANNEL_WEIGHTING_R "X RED"
#define COMPRESS_OPTIONS_CHANNEL_WEIGHTING_G "Y GREEN"
#define COMPRESS_OPTIONS_CHANNEL_WEIGHTING_B "Z BLUE"
#define COMPRESS_OPTIONS_WIDTH "Width"
#define COMPRESS_OPTIONS_HEIGHT "Height"
#define COMPRESS_OPTIONS_COMP_RATIO "Compression Ratio"
#define COMPRESS_OPTIONS_COMP_TIME "Compression Time"
#define COMPRESS_OPTIONS_ALPHATHRESHOLD "Threshold"
#define COMPRESS_OPTIONS_ADAPTIVECOLOR "Adaptive"
#define COMPRESS_OPTIONS_BITRATE "Bitrate"
#define COMPRESS_OPTIONS_REFINE_STEPS "Refine Steps"

//HDR image input properties
#define COMPRESS_OPTIONS_DEFOG "Defog"
#define COMPRESS_OPTIONS_EXPOSURE "Exposure"
#define COMPRESS_OPTIONS_KNEELOW "KneeLow"
#define COMPRESS_OPTIONS_KNEEHIGH "KneeHigh"
#define COMPRESS_OPTIONS_GAMMA "Gamma"

struct Model_Image
{
    QString          m_FilePathName;  // Files Path + name + ext
    bool             m_isImage;       // File is an Image file else File contains Model Mesh Data
    bool             m_srcDelFlag;    // Used only by 3DSubModels
    int              m_Width;         // Width
    int              m_Height;        // Height
    int              m_FileSize;      // FileSize
    QTreeWidgetItem* child = NULL;    // Child node that contains the image data
};

class C_Destination_Options : public C_Destination_Image
{
    Q_OBJECT
    Q_ENUMS(eCompression_options)
    Q_PROPERTY(eCompression_options Format READ getCompression WRITE setCompression NOTIFY compressionChanged)
    Q_PROPERTY(double Quality READ getQuality WRITE setQuality NOTIFY qualityChanged)
#ifdef USE_ENABLEHQ
    Q_PROPERTY(bool EnableHQ READ getEnableHQ WRITE setEnableHQ NOTIFY enableHQChanged)
#endif

public:
    enum eCompression_options
    {
#ifdef USE_GUI_LOSSLESS_COMPRESSION
        // Lossless GPU Based Compression Formats --------------------------------------------------------------------------------
        BRLG,
#endif
#ifdef USE_APC
        APC,
#endif
#ifdef USE_BASIS
        BASIS,
#endif
#ifdef USE_GTC
        GTC,
#endif
        BC1,
        BC2,
        BC3,
        BC4,
        BC4_S,
        BC5,
        BC5_S,
        BC6H,
        BC6H_SF,
        BC7,
        ETC_RGB,
        ETC2_RGB,
        ETC2_RGBA,
        ETC2_RGBA1,
#ifdef ENABLE_USER_ETC2S_FORMATS
        ETC2_SRGB,
        ETC2_SRGBA,
        ETC2_SRGBA1,
#endif
#if (OPTION_BUILD_ASTC == 1)
        ASTC,
#endif
        ATC_RGB,
        ATC_RGBA_Explicit,
        ATC_RGBA_Interpolated,
        ATI1N,
        ATI2N,
        ATI2N_XY,
        ATI2N_DXT5,
        DXT1,
        DXT3,
        DXT5,
        DXT5_xGBR,
        DXT5_RxBG,
        DXT5_RBxG,
        DXT5_xRBG,
        DXT5_RGxB,
        DXT5_xGxR,
        ARGB_8888,
        // ARGB_8888_S,
        ARGB_16F,
        ARGB_32F,

        //ARGB_16,
        //RGB_888,
        //RG_8,
        //R_8,
        //ARGB_2101010,
        //RG_16,
        //R_16,
        //RG_16F,
        //R_16F,
        //RG_32F,
        //R_32F,

        MESH_DATA,

    };

    C_Destination_Options()
    {
        init();
        m_Compression = C_Destination_Options::BC7;
    }

    void init()
    {
        m_MipImages         = NULL;
        m_OriginalMipImages = NULL;
        m_isModelData       = false;
        m_FileSize          = 0;
        m_DstWidth          = 0;
        m_DstHeight         = 0;
        //m_Encoding        = No_Encoding;
        m_Quality = AMD_CODEC_QUALITY_DEFAULT;
#ifdef USE_ENABLEHQ
        m_EnableHQ = false;  // Flag to enable added High Quality encoding
#endif
        m_isselected = false;  // Flag to force Project View to use the datas child item in compression

        // These items are not set by PropertyManager
        // they are done via seperate widgets
        m_compname                 = "";
        m_destFileNamePath         = "";
        m_decompressedFileNamePath = "";
        m_editing                  = false;
        m_iscompressedFormat       = false;  // Flag to indicate the target will be saved as a  compressed file
        m_data_has_been_changed    = false;  // Set if any data has changed value
        m_SourceIscompressedFormat = false;  // Flag indicating source is compressed format
        m_SourceIsFloatFormat      = false;
        m_SourceImageSize          = 0;
        m_SourceType               = 0;
        m_modelSource              = "";
        m_sourceFileNamePath       = "";
        m_Model_Images.clear();
        m_SubModel_Images.clear();
    }

    // Create a temp unique file name to use for a decompressed image
    // and save it for use in analysis later
    void CreateTempFile()
    {
        QTemporaryFile CTemporaryFile;
        CTemporaryFile.setFileTemplate("Compress_TMP_XXXXXX.bmp");
        if (CTemporaryFile.open())
        {
            m_decompressedFileNamePath = CTemporaryFile.fileName();
            CTemporaryFile.close();
        }
    }

    ~C_Destination_Options()
    {
        if (m_decompressedFileNamePath.length() > 0)
        {
            QFile::remove(m_decompressedFileNamePath);
        }
    }

    void setController(ObjectController* newController)
    {
        m_controller = newController;
    }

    ObjectController* getController()
    {
        return m_controller;
    }

    void setCompression(eCompression_options Compression)
    {
        if (m_Compression != Compression)
        {
            m_data_has_been_changed = true;
            emit dataChanged();
        }
        m_Compression = Compression;
        emit compressionChanged((QVariant&)Compression);
        //}
    }

    eCompression_options getCompression() const
    {
        return m_Compression;
    }

    void setQuality(float quality)
    {
        if (m_Quality != quality)
        {
            m_data_has_been_changed = true;
            emit dataChanged();
        }
        if (quality > 1)
            m_Quality = 1;
        else if (quality < 0)
            m_Quality = 0;
        else
            m_Quality = quality;

        emit qualityChanged((QVariant&)quality);
    }

    double getQuality() const
    {
        if (m_globalSetting_qualityEnabled)
            return m_globalSetting_quality;
        else
            return m_Quality;
    }

#ifdef USE_ENABLEHQ
    bool getEnableHQ() const
    {
        return m_EnableHQ;
    }

    void setEnableHQ(bool enabled)
    {
        if (m_EnableHQ != enabled)
        {
            m_data_has_been_changed = true;
            emit dataChanged();
        }
        m_EnableHQ = enabled;
        emit enableHQChanged((QVariant&)enabled);
    }
#endif

    bool doSwizzle()
    {
        // determin of the swizzle flag needs to be turned on!
        switch (m_Compression)
        {
        case BC4:
        case ATI1N:  // same as BC4
        case BC5:
        case ATI2N:       // same as BC5
        case ATI2N_XY:    // same as BC5 Channels are swizzled
        case ATI2N_DXT5:  // same as BC5
        case BC1:
        case BC2:
        case DXT3:  // same as BC2
        case BC3:
        case DXT5:  // same as BC3
            return true;
            break;
        default:
            break;
        }

        return false;
    }

    QString              m_modelSource;
    QString              m_modelDest;
    QList<Model_Image>   m_Model_Images;
    QList<Model_Image>   m_SubModel_Images;
    QString              m_compname;
    QString              m_sourceFileNamePath;
    QString              m_destFileNamePath;
    QString              m_decompressedFileNamePath;
    eCompression_options m_Compression;
    bool                 m_editing;
    double               m_Quality;
#ifdef USE_ENABLEHQ
    bool m_EnableHQ;
#endif
    bool m_iscompressedFormat;
    bool m_isModelData;  // m_compname is ModelData destination name else its a Texture

    CMipImages* m_MipImages;
    bool        m_isselected;
    bool        m_SourceIscompressedFormat;
    bool        m_SourceIsFloatFormat;
    long        m_SourceImageSize;
    int         m_SourceType;

    // Use this as  Read Only property. It points to the original Image used for this destination setting
    // so dont delete its ref using this class. Parent class will clean it up
    CMipImages* m_OriginalMipImages;

    // Global Setting overrides
    bool  m_globalSetting_qualityEnabled = false;
    float m_globalSetting_quality        = 0.0f;
    int   m_globalSetting_refine_steps   = 0;

signals:

    void compressionChanged(QVariant&);
    void meshCompressionChanged(QVariant&);
    void qualityChanged(QVariant&);
    void enableHQChanged(QVariant&);

private:
    friend C_Destination_Options& operator<<(C_Destination_Options& ds, const C_Destination_Options& obj)
    {
        // Assign Varient data used by the PropertyManager

        QVariant var;
        int      count  = obj.metaObject()->propertyCount();
        int      count2 = ds.metaObject()->propertyCount();
        if (count == count2)
        {
            for (int i = 0; i < count; ++i)
            {
                if (obj.metaObject()->property(i).isStored(&obj))
                {
                    ds.metaObject()->property(i).write(&ds, obj.metaObject()->property(i).read(&obj));
                }
            }
        }

        // Assign none property data used by the class
        ds.m_controller                   = obj.m_controller;
        ds.m_modelSource                  = obj.m_modelSource;
        ds.m_modelDest                    = obj.m_modelDest;
        ds.m_isModelData                  = obj.m_isModelData;
        ds.m_Compression                  = obj.m_Compression;
        ds.m_compname                     = obj.m_compname;
        ds.m_destFileNamePath             = obj.m_destFileNamePath;
        ds.m_editing                      = obj.m_editing;
        ds.m_sourceFileNamePath           = obj.m_sourceFileNamePath;
        ds.m_iscompressedFormat           = obj.m_iscompressedFormat;
        ds.m_SourceIscompressedFormat     = obj.m_SourceIscompressedFormat;
        ds.m_SourceIsFloatFormat          = obj.m_SourceIsFloatFormat;
        ds.m_data_has_been_changed        = obj.m_data_has_been_changed;
        ds.m_globalSetting_qualityEnabled = obj.m_globalSetting_qualityEnabled;
        ds.m_globalSetting_quality        = obj.m_globalSetting_quality;
        ds.m_globalSetting_refine_steps   = obj.m_globalSetting_refine_steps;
        ds.m_SourceImageSize              = obj.m_SourceImageSize;
        ds.X_RED                          = obj.X_RED;
        ds.Y_GREEN                        = obj.Y_GREEN;
        ds.Z_BLUE                         = obj.Z_BLUE;
        ds.Threshold                      = obj.Threshold;

        return ds;
    }
};

// =======================================================
// ORIGINAL IMAGE FILE
// =======================================================

// http://doc.qt.io/qt-5/properties.html
// For the Propert View we use _ in string names to indicate a read only field
// in the editor

class C_Source_Info : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString _Name MEMBER m_Name)
    Q_PROPERTY(QString _Full_Path MEMBER m_Full_Path)
    Q_PROPERTY(QString _File_Size MEMBER m_FileSizeStr)
    Q_PROPERTY(QString _Image_Size MEMBER m_ImageSizeStr)
    Q_PROPERTY(QString _Width MEMBER m_WidthStr)
    Q_PROPERTY(QString _Height MEMBER m_HeightStr)
    Q_PROPERTY(QString _Depth MEMBER m_DepthStr)
    Q_PROPERTY(int _Mip_Levels MEMBER m_Mip_Levels)
    Q_PROPERTY(QString _Format MEMBER m_FormatStr)
    Q_PROPERTY(QString _Texture_Type MEMBER m_TextureTypeStr)

    Q_ENUMS(CMP_FORMAT)

public:
    C_Source_Info()
    {
        m_Name         = "";
        m_Full_Path    = "";
        m_ImageSize    = 0;
        m_FileSize     = 0;
        m_Width        = 0;
        m_Height       = 0;
        m_Depth        = 0;
        m_FileSizeStr  = "";
        m_ImageSizeStr = "";
        m_WidthStr     = "";
        m_HeightStr    = "";
        m_DepthStr     = "";
        m_Mip_Levels   = 0;

        // Used to index new file name
        m_extnum    = 0;
        m_MipImages = NULL;

        m_FormatStr      = GetFormatDesc(CMP_FORMAT::CMP_FORMAT_Unknown);
        m_TextureTypeStr = GetTextureTypeDesc(CMP_TextureType::TT_Unknown);
        m_Format         = CMP_FORMAT::CMP_FORMAT_Unknown;
    }

    QString m_Name;
    QString m_Full_Path;
    QString m_WidthStr;
    QString m_HeightStr;
    QString m_DepthStr;  // depthsupport
    QString m_FileSizeStr;
    QString m_ImageSizeStr;
    QString m_TextureTypeStr;  // depthsupport
    QString m_FormatStr;

    CMP_FORMAT  m_Format;
    TextureType m_TextureType;  // depthsupport
    int         m_Width;
    int         m_Height;
    int         m_Depth;
    int         m_FileSize;
    int         m_Mip_Levels;
    int         m_extnum;
    long        m_ImageSize;
    CMipImages* m_MipImages;
};

class C_Global_Process_Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool Enabled READ getEnabled WRITE setEnabled)
    Q_PROPERTY(double Set_Quality READ getQuality WRITE setQuality)
    Q_PROPERTY(int Set_Refine_Steps READ getRefineSteps WRITE setRefineSteps)

public:
    C_Global_Process_Settings()
    {
        m_Quality      = 1.00;
        m_Refine_Steps = 0;
    }

    double m_Quality;
    int    m_GlobalSettingEnabled;
    int    m_Refine_Steps;

    void setEnabled(bool setop)
    {
        m_GlobalSettingEnabled = setop ? 1 : 0;
        emit globalPropertyChanged(m_GlobalSettingEnabled);
    }

    int getEnabled() const
    {
        return m_GlobalSettingEnabled;
    }

    void setQuality(double quality)
    {
        if (quality > 1.0)
            quality = 1.0;
        else if (quality <= 0)
        {
            quality = 0.0;
        }
        m_Quality = quality;
    }

    float getQuality() const
    {
        return m_Quality;
    }

    void setRefineSteps(int steps)
    {
        m_GlobalSettingEnabled = 1;
        if (steps > 2)
            steps = 2;
        else if (steps < 0)
        {
            steps = 0;
        }
        m_Refine_Steps = steps;
        emit globalPropertyChanged(m_GlobalSettingEnabled);
    }

    int getRefineSteps() const
    {
        return m_Refine_Steps;
    }

signals:
    void globalPropertyChanged(int&);
};

class C_Mesh_Buffer_Info : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString _Name MEMBER m_Name)
    Q_PROPERTY(QString _Full_Path MEMBER m_Full_Path)
    Q_PROPERTY(QString _File_Size MEMBER m_FileSizeStr)

public:
    C_Mesh_Buffer_Info()
    {
        m_Name          = "";
        m_Full_Path     = "";
        m_FileSize      = 0;
        m_FileSizeStr   = "";
        m_glTF_filePath = "";
    }

    QString m_Name;
    QString m_Full_Path;
    QString m_FileSizeStr;
    QString m_glTF_filePath;
    int     m_FileSize;
};

class C_3DMesh_Statistic : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString _Clusters MEMBER m_Clusters)
    Q_PROPERTY(QString _CacheIn_Out_Ratio MEMBER m_CacheInOutRatio)
    Q_PROPERTY(QString _OverdrawIn_Out_Ratio MEMBER m_OverdrawInOutRatio)
    Q_PROPERTY(QString _OverdrawMaxIn_Out_Ratio MEMBER m_OverdrawMaxInOutRatio)

public:
    C_3DMesh_Statistic()
    {
        m_Clusters              = "";
        m_CacheInOutRatio       = "1x";
        m_OverdrawInOutRatio    = "1x";
        m_OverdrawMaxInOutRatio = "1x";
    }

    QString m_Clusters;
    QString m_CacheInOutRatio;
    QString m_OverdrawInOutRatio;
    QString m_OverdrawMaxInOutRatio;
};

class C_3DSubModel_Info :
#ifdef USE_MESHOPTIMIZER
    public QObject
#else
    public C_3DMesh_Statistic
#endif
{
    Q_OBJECT
    Q_PROPERTY(QString _Name MEMBER m_Name)
    Q_PROPERTY(QString _Full_Path MEMBER m_Full_Path)
    Q_PROPERTY(QString _File_Size MEMBER m_FileSizeStr)
    Q_PROPERTY(QString _Generator MEMBER m_GeneratorStr)
    Q_PROPERTY(QString _Version MEMBER m_VersionStr)

public:
    C_3DSubModel_Info()
    {
        m_Name         = "";
        m_Full_Path    = "";
        m_FileSize     = 0;
        m_GeneratorStr = "Unknown";
        m_VersionStr   = "Unknown";
        m_extnum       = 0;  // Used to index new file name

        m_Model_Images.clear();
        m_SubModel_Images.clear();
    }

    QString m_Name;
    QString m_Full_Path;
    QString m_FileSizeStr;
    QString m_GeneratorStr;
    QString m_VersionStr;
    QString m_ModelSource_gltf;

    QList<Model_Image> m_Model_Images;     // Original 3DModel Images
    QList<Model_Image> m_SubModel_Images;  // New 3DSubModels Images with new or same location and new compressed names

    int m_FileSize;
    int m_extnum;

    CMODEL_DATA m_ModelData;

    eModelType ModelType;

    // copy of the original file in 3D_Source_Info, that has path updated to user selected paths
    nlohmann::json m_original_gltf;

    // Working copy used to add compression file ref and offsets for mesh data
    nlohmann::json m_gltf;
};

class C_3DModel_Info : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString _Name MEMBER m_Name)
    Q_PROPERTY(QString _Full_Path MEMBER m_Full_Path)
    Q_PROPERTY(QString _File_Size MEMBER m_FileSizeStr)
    Q_PROPERTY(QString _Generator MEMBER m_GeneratorStr)
    Q_PROPERTY(QString _Version MEMBER m_VersionStr)

public:
    C_3DModel_Info()
    {
        m_Name         = "";
        m_Full_Path    = "";
        m_FileSize     = 0;
        m_GeneratorStr = "Unknown";
        m_VersionStr   = "Unknown";
        m_extnum       = 0;  // Used to index new file name
        m_Model_Images.clear();
    }

    QString m_Name;
    QString m_Full_Path;
    QString m_FileSizeStr;
    QString m_GeneratorStr;
    QString m_VersionStr;
    QString m_modelSource1;

    QList<Model_Image> m_Model_Images;

    int         m_FileSize;
    int         m_extnum;
    CMODEL_DATA m_ModelData;

    eModelType ModelType;

    // Original glTF File info is stored  for referance
    // and used for copies
    nlohmann::json m_gltf;
};

// =======================================================
// APPLICATION DATA
// =======================================================
#define APP_compress_image_using "Encode with"
#define APP_Render_Models_with "Render Models with"
#define APP_Decompress_image_views_using "Decode with"
#define APP_Reload_image_views_on_selection "Reload image views on selection"
#define APP_Load_recent_project_on_startup "Load recent project on startup"
#define APP_Close_all_image_views_prior_to_process "Close all image views prior to process"
#define APP_Mouse_click_on_icon_to_view_image "Mouse click on icon to view image"
#define APP_Set_Image_Diff_Contrast "Set Image Diff Contrast"
#define APP_Set_Number_of_Threads "Set Number of Threads"
#define APP_Show_MSE_PSNR_SSIM_Results "Show MSE PSNR SSIM Results"
#define APP_Show_Analysis_Results_Table "Show Analysis Results Table"
#define APP_Use_GPU_To_Generate_MipMaps "Use GPU To Generate MipMaps"
#define APP_Use_SRGB_Frames_While_Encoding "Use SRGB Frames While Encoding"
#define APP_Use_Original_File_Names "Use Original File Names"

class C_Application_Options : public QObject
{
    Q_OBJECT
    Q_ENUMS(ImageDecodeWith)
    Q_ENUMS(RenderModelsWith)
    Q_ENUMS(ImageEncodeWith)
    Q_PROPERTY(ImageEncodeWith Encode_with READ getImageEncode WRITE setImageEncode NOTIFY ImageEncodeChanged)
    Q_PROPERTY(bool Use_GPU_To_Generate_MipMaps READ getUseGPUMipMaps WRITE setUseGPUMipMaps)
    Q_PROPERTY(bool Use_SRGB_Frames_While_Encoding READ getUseSRGBFrames WRITE setUseSRGBFrames)
    Q_PROPERTY(ImageDecodeWith Decode_with READ getImageViewDecode WRITE setImageViewDecode NOTIFY ImageViewDecodeChanged)
    Q_PROPERTY(bool Reload_image_views_on_selection READ getUseNewImageViews WRITE setUseNewImageViews)
    Q_PROPERTY(bool Close_all_image_views_prior_to_process READ getCloseAllImageViews WRITE setCloseAllImageViews)
    Q_PROPERTY(bool Mouse_click_on_icon_to_view_image READ getclickIconToViewImage WRITE setclickIconToViewImage)
    Q_PROPERTY(bool Load_recent_project_on_startup READ getLoadRecentFile WRITE setLoadRecentFile)
    Q_PROPERTY(double Set_Image_Diff_Contrast READ getImagediffContrast WRITE setImagediffContrast)
    Q_PROPERTY(int Set_Number_of_Threads READ getThreads WRITE setThreads)
#ifdef USE_ASSIMP
    Q_PROPERTY(bool Use_assimp READ getUseAssimp WRITE setUseAssimp)
#endif
    Q_PROPERTY(RenderModelsWith Render_Models_with READ getGLTFRender WRITE setGLTFRender)
    Q_PROPERTY(bool Show_MSE_PSNR_SSIM_Results READ getLogResults WRITE setLogResults NOTIFY LogResultsChanged)
    Q_PROPERTY(bool Show_Analysis_Results_Table READ getAnalysisResultTable WRITE setAnalysisResultTable)
    Q_PROPERTY(bool Use_Original_File_Names READ getUseOriginalFileNames WRITE setUseOriginalFileNames)
public:
    // Keep order of list as its ref is saved in CompressSettings.ini
    // we should change the save to use string name instead of indexes to the enum
    enum class ImageEncodeWith
    {
        CPU,
        HPC,
        GPU_DirectX,
        GPU_OpenCL,
        GPU_HW,
#ifdef USE_GPU_PIPELINE_VULKAN
        GPU_Vulkan,
#endif
    };

    enum class ImageDecodeWith
    {
        CPU,
        GPU_OpenGL,
        GPU_DirectX,
        GPU_Vulkan
    };

    enum class RenderModelsWith
    {
        glTF_DX12_EX = 0,
        glTF_Vulkan  = 1,
        glTF_OpenGL  = 2
    };

    // Flags how image views are used, True deletes the old view and creates a new one
    // every time user clicks on an image item on the Project View, else it will
    // load the image once and cashe the image views, Default is True

    bool m_useNewImageViews;

    C_Application_Options()
    {
        m_ImageViewDecode      = ImageDecodeWith::CPU;
        m_ImageEncode          = ImageEncodeWith::CPU;
        m_useGPUMipMaps        = false;
        m_useSRGBFrames        = false;
        m_loadRecentFile       = false;
        m_useNewImageViews     = false;
        m_refreshCurrentView   = false;
        m_closeAllDocuments    = true;
        m_clickIconToViewImage = true;
        m_useAssimp            = false;
        m_logresults           = false;
        m_analysisResultTable  = false;
        m_imagediff_contrast   = 20.0;
        m_threads              = CMP_NumberOfProcessors();
        if (m_threads < 2)
            m_threads = 8;

        //#ifdef USE_GLTF_OPENGL
        //        m_GLTFRenderWith       = RenderModelsWith::glTF_OpenGL;
        //#else
        m_GLTFRenderWith = RenderModelsWith::glTF_Vulkan;
        //#endif

        m_useOriginalFileNames = true;
    }

    void setImageViewDecode(ImageDecodeWith decodewith)
    {
        m_ImageViewDecode = decodewith;
        emit ImageViewDecodeChanged((QVariant&)decodewith);
    }

    ImageDecodeWith getImageViewDecode() const
    {
        return m_ImageViewDecode;
    }

    void setImageEncode(ImageEncodeWith encodewith)
    {
        m_ImageEncode = encodewith;
        emit ImageEncodeChanged((QVariant&)encodewith);
    }

    ImageEncodeWith getImageEncode() const
    {
        return m_ImageEncode;
    }

    bool isGPUEncode()
    {
        return ((m_ImageEncode == ImageEncodeWith::GPU_DirectX) || (m_ImageEncode == ImageEncodeWith::GPU_OpenCL) ||
                (m_ImageEncode == ImageEncodeWith::GPU_HW));
    }

    void setImagediffContrast(double contrast)
    {
        if (contrast > 200)
            contrast = 200;
        else if (contrast < 1)
            contrast = 1;
        m_imagediff_contrast = contrast;
    }

    void setThreads(int threads)
    {
        if (threads > 128)
            threads = 128;
        else if (threads < 0)
            threads = 0;
        m_threads = threads;
    }

    double getImagediffContrast() const
    {
        return m_imagediff_contrast;
    }

    int getThreads() const
    {
        return m_threads;
    }

    void setUseAssimp(bool recent)
    {
        m_useAssimp = recent;
    }

    double getUseAssimp() const
    {
        return m_useAssimp;
    }

    void setCloseAllImageViews(bool recent)
    {
        m_closeAllDocuments = recent;
    }

    double getCloseAllImageViews() const
    {
        return m_closeAllDocuments;
    }

    void setLoadRecentFile(bool recent)
    {
        m_loadRecentFile = recent;
    }

    bool getLoadRecentFile() const
    {
        return m_loadRecentFile;
    }

    void setUseGPUMipMaps(bool recent)
    {
        m_useGPUMipMaps = recent;
    }

    bool getUseGPUMipMaps() const
    {
        return m_useGPUMipMaps;
    }

    void setUseSRGBFrames(bool recent)
    {
        m_useSRGBFrames = recent;
    }

    bool getUseSRGBFrames() const
    {
        return m_useSRGBFrames;
    }

    void setLogResults(bool recent)
    {
        m_logresults = recent;
        emit LogResultsChanged((QVariant&)recent);
    }

    bool getLogResults() const
    {
        return m_logresults;
    }

    double getAnalysisResultTable() const
    {
        return m_analysisResultTable;
    }

    void setAnalysisResultTable(bool recent)
    {
        m_analysisResultTable = recent;
    }

    void setUseNewImageViews(bool recent)
    {
        m_useNewImageViews = recent;
    }

    double getUseNewImageViews() const
    {
        return m_useNewImageViews;
    }

    void setclickIconToViewImage(bool recent)
    {
        m_clickIconToViewImage = recent;
    }

    double getclickIconToViewImage() const
    {
        return m_clickIconToViewImage;
    }

    void setGLTFRender(RenderModelsWith renderwith)
    {
#ifndef NDEBUG
#if !OPTION_CMP_DIRECTX
        assert(renderwith != RenderModelsWith::glTF_DX12_EX);
#endif
#if !OPTION_CMP_OPENGL
        assert(renderwith != RenderModelsWith::glTF_OpenGL);
#endif
#if !OPTION_CMP_VULKAN
        assert(renderwith != RenderModelsWith::glTF_Vulkan);
#endif
#endif
        m_GLTFRenderWith = renderwith;
    }

    RenderModelsWith getGLTFRender() const
    {
        return m_GLTFRenderWith;
    }

    bool getUseOriginalFileNames() const
    {
        return m_useOriginalFileNames;
    }

    void setUseOriginalFileNames(bool newValue)
    {
        m_useOriginalFileNames = newValue;
    }

    ImageDecodeWith  m_ImageViewDecode;
    ImageEncodeWith  m_ImageEncode;
    bool             m_clickIconToViewImage;
    bool             m_closeAllDocuments;
    bool             m_useGPUMipMaps;
    bool             m_useSRGBFrames;
    bool             m_loadRecentFile;
    bool             m_refreshCurrentView;
    bool             m_useAssimp;
    double           m_imagediff_contrast;
    int              m_threads;
    bool             m_logresults;
    bool             m_analysisResultTable;
    RenderModelsWith m_GLTFRenderWith;
    bool             m_useOriginalFileNames;

signals:
    void ImageViewDecodeChanged(QVariant&);
    void LogResultsChanged(QVariant&);
    void ImageEncodeChanged(QVariant&);
};

#define STR_QUALITY_SETTING_HINT "Quality Setting Range 0 (Poor)to 1 (High)Default is 0.05"
#ifdef USE_ENABLEHQ
#define STR_ENABLEHQ_SETTING_HINT "EnableHQ sets additional tests to improve image quality"
#endif
#define STR_FORMAT_SETTING_HINT "Please refer to format description below for more info on compression format"
#define STR_SETTING_MINIMUM "minimum"
#define STR_SETTING_MAXIMUM "maximum"
#define STR_SETTING_SINGLESTEP "singleStep"
#define STR_SETTING_DECIMALS "decimals"

#define STR_CHANNELWEIGHTR_SETTING_HINT "Channel Weight Setting Range 0 (Poor)to 1 (High)Default R Weightiing is 0.3086"
#define STR_CHANNELWEIGHTG_SETTING_HINT "Channel Weight Setting Range 0 (Poor)to 1 (High)Default G Weightiing is 0.6094"
#define STR_CHANNELWEIGHTB_SETTING_HINT "Channel Weight Setting Range 0 (Poor)to 1 (High)Default B Weightiing is 0.0820"

#define STR_ALPHATHRESHOLD_HINT "Alpha Threshold Range 1 to 255"

#define STR_BITRATE_SETTING_HINT "The maximum bitrate allowed is 8.00. The closest bitrate will be determined. Default is 8.00(4x4)"

#define STR_DEFOG_SETTING_HINT "The defog range supported is 0.000  to 0.100. Default is 0."
#define STR_EXPOSURE_SETTING_HINT "The exposure range supported is -10 to 10. Default is 0."
#define STR_KNEELOW_SETTING_HINT "The kneelow range supported is -3.0 to 3.0. Default is 0."
#define STR_KNEEHIGH_SETTING_HINT "The kneehigh range supported is 3.5 to 7.5. Default is 5."
#define STR_GAMMA_SETTING_HINT "The gamma range supported is 1 to 2.6. Default is 2.2."

#define UNKNOWN_IMAGE " Unknown"
#define IMAGE_TYPE_PLUGIN "IMAGE"

#endif
