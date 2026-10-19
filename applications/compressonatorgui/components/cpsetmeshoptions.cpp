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

#include "cpsetmeshoptions.h"

#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QIcon>
#include <QMap>
#include "qtpropertymanager.h"
#include "qteditorfactory.h"
#include "qttreepropertybrowser.h"
#include "qtbuttonpropertybrowser.h"
#include "qtgroupboxpropertybrowser.h"

#include "cpmaincomponents.h"
#include "cmp_fileio.h"
#define DROPDOWN_FILEEXT_WIDTH 60

extern void GetSupportedFileFormats(QList<QByteArray>& g_supportedFormats);

#ifdef USE_TREEVIEW_PROP
QTreeWidgetItem* CSetMeshOptions::AddRootItem(int col, QString itemtext, bool checkable)
{
    QTreeWidgetItem* treeItem = new QTreeWidgetItem(m_trewwview);
    treeItem->setText(col, itemtext);
    if (checkable)
    {
        treeItem->setFlags(treeItem->flags() | Qt::ItemIsUserCheckable);
        treeItem->setCheckState(0, Qt::Unchecked);
    }
    treeItem->addChild(treeItem);
    return treeItem;
}

QTreeWidgetItem* CSetMeshOptions::AddChildItem(QTreeWidgetItem* parent, int col, QString itemtext, bool checkable)
{
    QTreeWidgetItem* treeItem = new QTreeWidgetItem(parent);
    treeItem->setText(col, itemtext);
    if (checkable)
    {
        treeItem->setFlags(treeItem->flags() | Qt::ItemIsUserCheckable);
        treeItem->setCheckState(0, Qt::Unchecked);
    }
    treeItem->addChild(treeItem);
    return treeItem;
}
#endif

CSetMeshOptions::CSetMeshOptions(const QString title, QWidget* parent)
    : QDialog(parent)
    ,  //QDockWidget(title, parent),
    m_title(title)
    , m_parent(parent)
{
    changeSelf    = false;
    m_propQuality = NULL;
#ifdef USE_ENABLEHQ
    m_propEnableHQ = NULL;
#endif
    m_propChannelWeightingR = NULL;
    m_propChannelWeightingG = NULL;
    m_propChannelWeightingB = NULL;
    m_propAlphaThreshold    = NULL;
    m_propAdaptiveColor     = NULL;
    m_propDefog             = NULL;
    m_propExposure          = NULL;
    m_propKneeLow           = NULL;
    m_propKneeHigh          = NULL;
    m_propGamma             = NULL;
    m_propBitrate           = NULL;
    isEditing               = false;
    isInit                  = false;
    isNoSetting             = false;
    m_extnum                = 1;

    m_showDestinationEXTSetting = true;  // Show the FileFormat drop down list (DDS, KTX...)
    m_showTheControllerSetting  = true;  // Show the FileFormat drop down list (DDS, KTX...)
    m_showTheInfoTextSetting    = true;  // Show the Info Text

    setWindowTitle(title);
    Qt::WindowFlags flags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);
    setWindowFlags(flags);
    setMinimumWidth(350);

    //--------------------------------
    // Destination Name
    //--------------------------------
    QLabel* LName = new QLabel(tr("Name"), this);
    m_LEName      = new QLineEdit(this);
    m_LEName->setEchoMode(QLineEdit::Normal);
    QObject::connect(m_LEName, SIGNAL(textChanged(QString)), this, SLOT(onNameTextChanged(QString)));
    QObject::connect(m_LEName, SIGNAL(editingFinished()), this, SLOT(onNameEditingFinished()));

    //=========================================================
    // Combo box for type of files supported by plugin manager
    //=========================================================
    m_fileFormats = new QComboBox(this);
    m_fileFormats->setMinimumWidth(DROPDOWN_FILEEXT_WIDTH);
    QList<QByteArray> m_supportedFormats;
    GetSupportedFileFormats(m_supportedFormats);

    m_AllFileTypes.clear();

    QList<QByteArray>::Iterator i;
    for (i = m_supportedFormats.begin(); i != m_supportedFormats.end(); ++i)
    {
        QByteArray fformat = (*i);
        QString    item    = fformat;
#if (OPTION_BUILD_ASTC == 1)
        if (item != "ASTC")
#endif
            m_AllFileTypes.append(item);
    }

    // Default output format
    m_fileFormats->addItem("DDS");
    m_fileFormats->addItem("KTX");

    // =================================================
    // Destination File
    // =================================================
    QLabel* m_lFolder = new QLabel(this);
    m_lFolder->setText("Folder");
    m_DestinationFolder = new QLineEdit(this);
    m_PBDestFileFolder  = new QPushButton("...", this);
    m_PBDestFileFolder->setMaximumWidth(30);
    QObject::connect(m_PBDestFileFolder, SIGNAL(clicked()), this, SLOT(onDestFileFolder()));

    m_DestinationFile = new QLineEdit();
    QObject::connect(m_DestinationFile, SIGNAL(editingFinished()), this, SLOT(onDestinationFileEditingFinished()));

    // =================================================
    // Object Browser
    // =================================================
    m_theController                = new ObjectController(this, true);
    QtTreePropertyBrowser* browser = m_theController->getTreeBrowser();
    if (browser)
    {
        browser->setHeaderVisible(false);
        browser->SetBrowserClick(true);
        browser->setResizeMode(QtTreePropertyBrowser::ResizeToContents);  // follow this comment Note#1
        connect(browser, SIGNAL(currentItemChanged(QtBrowserItem*)), this, SLOT(oncurrentItemChanged(QtBrowserItem*)));
    }

    connect(&m_DestinationData, SIGNAL(compressionChanged(QVariant&)), this, SLOT(compressionValueChanged(QVariant&)));
    //connect(&m_DestinationData, SIGNAL(meshCompressionChanged(QVariant &)), this, SLOT(meshCompressionValueChanged(QVariant &)));
    connect(&m_DestinationData, SIGNAL(qualityChanged(QVariant&)), this, SLOT(qualityValueChanged(QVariant&)));
    connect(&m_DestinationData, SIGNAL(redwChanged(QVariant&)), this, SLOT(redwValueChanged(QVariant&)));
    connect(&m_DestinationData, SIGNAL(greenwChanged(QVariant&)), this, SLOT(greenwValueChanged(QVariant&)));
    connect(&m_DestinationData, SIGNAL(bluewChanged(QVariant&)), this, SLOT(bluewValueChanged(QVariant&)));
    connect(&m_DestinationData, SIGNAL(thresholdChanged(QVariant&)), this, SLOT(thresholdValueChanged(QVariant&)));
    connect(&m_DestinationData, SIGNAL(bitrateChanged(QString&, int&, int&)), this, SLOT(bitrateValueChanged(QString&, int&, int&)));
    connect(&m_DestinationData, SIGNAL(defogChanged(double&)), this, SLOT(defogValueChanged(double&)));
    connect(&m_DestinationData, SIGNAL(exposureChanged(double&)), this, SLOT(exposureValueChanged(double&)));
    connect(&m_DestinationData, SIGNAL(kneeLowChanged(double&)), this, SLOT(kneelowValueChanged(double&)));
    connect(&m_DestinationData, SIGNAL(kneeHighChanged(double&)), this, SLOT(kneehighValueChanged(double&)));
    connect(&m_DestinationData, SIGNAL(gammaChanged(double&)), this, SLOT(gammaValueChanged(double&)));

    m_theController->setObject(&m_DestinationData, true);

    m_DestinationData.m_controller = m_theController;

    // Set Editing Defaults
    m_propQuality = m_theController->getProperty(COMPRESS_OPTIONS_QUALITY);
#ifdef USE_ENABLEHQ
    m_propEnableHQ = m_theController->getProperty(COMPRESS_OPTIONS_HIGHQUALITY);
#endif
    m_propFormat = m_theController->getProperty(COMPRESS_OPTIONS_FORMAT);

    // Hide settings not relavent to the current setup

#ifdef USE_MESHOPTIMIZER
    m_propMeshOptimizerSettings = m_theController->getProperty(MESH_OPTIMIZER_SETTING_CLASS_NAME);
    if (m_propMeshOptimizerSettings)
        m_propMeshOptimizerSettings->setHidden(true);

#else
    m_propMeshSettings = m_theController->getProperty(MESH_SETTINGS_CLASS_NAME);
    if (m_propMeshSettings)
        m_propMeshSettings->setHidden(true);
#endif

    m_propMeshCompressionSettings = m_theController->getProperty(MESH_COMPRESSION_SETTINGS_CLASS_NAME);
    if (m_propMeshCompressionSettings)
        m_propMeshCompressionSettings->setHidden(true);

    m_propChannelWeightingR = m_theController->getProperty(COMPRESS_OPTIONS_CHANNEL_WEIGHTING_R);
    m_propChannelWeightingG = m_theController->getProperty(COMPRESS_OPTIONS_CHANNEL_WEIGHTING_G);
    m_propChannelWeightingB = m_theController->getProperty(COMPRESS_OPTIONS_CHANNEL_WEIGHTING_B);
    m_propAlphaThreshold    = m_theController->getProperty(COMPRESS_OPTIONS_ALPHATHRESHOLD);
    m_propAdaptiveColor     = m_theController->getProperty(COMPRESS_OPTIONS_ADAPTIVECOLOR);
    m_propBitrate           = m_theController->getProperty(COMPRESS_OPTIONS_BITRATE);
    m_propDefog             = m_theController->getProperty(COMPRESS_OPTIONS_DEFOG);
    m_propExposure          = m_theController->getProperty(COMPRESS_OPTIONS_EXPOSURE);
    m_propKneeLow           = m_theController->getProperty(COMPRESS_OPTIONS_KNEELOW);
    m_propKneeHigh          = m_theController->getProperty(COMPRESS_OPTIONS_KNEEHIGH);
    m_propGamma             = m_theController->getProperty(COMPRESS_OPTIONS_GAMMA);
    m_propDestImage         = m_theController->getProperty(DESTINATION_IMAGE_CLASS_NAME);
    m_propChannelWeight     = m_theController->getProperty(CHANNEL_WEIGHTING_CLASS_NAME);
    m_propDXT1Alpha         = m_theController->getProperty(DXT1_ALPHA_CLASS_NAME);
#if (OPTION_BUILD_ASTC == 1)
    m_propASTCBlockRate = m_theController->getProperty(ASTC_BLOCKRATE_CLASS_NAME);
#endif
    m_propHDRProperties = m_theController->getProperty(HDR_PROP_CLASS_NAME);

    //=================================
    // Text View for help and Hints
    //=================================

    m_infotext = new QTextBrowser(this);
    // Always show min Two lines of text and Max to 5 lines at font size 16
    m_infotext->setMinimumHeight(40);
    m_infotext->setMaximumHeight(100);
    m_infotext->setReadOnly(true);
    m_infotext->setAcceptRichText(true);

    //================
    // Buttons
    //================
    m_PBSaveSettings = new QPushButton("Save", this);
    m_PBCancel       = new QPushButton("Cancel", this);
    QObject::connect(m_PBSaveSettings, SIGNAL(clicked()), this, SLOT(PBSaveCompressSetting()));
    QObject::connect(m_PBCancel, SIGNAL(clicked()), this, SLOT(onPBCancel()));

#ifdef USE_TREEVIEW_PROP
    m_theControllerOptions         = new ObjectController(this, true);
    QtTreePropertyBrowser* browser = m_theController->getTreeBrowser();
    if (browser)
    {
        browser->setHeaderVisible(false);
        browser->SetBrowserClick(true);
        browser->setResizeMode(QtTreePropertyBrowser::ResizeToContents);  // follow this comment Note#1
    }

    m_trewwview = new QTreeWidget();
    m_trewwview->setHeaderHidden(true);

    // ASTC setup
    QTreeWidgetItem* parentItem;
    AddRootItem(0, "Compression subMode", false);
    AddRootItem(0, "Texture type and block Dimensions", false);
    parentItem = AddRootItem(0, "Additional options", false);
    AddChildItem(parentItem, 0, "Built-in error-weighting options", true);
    AddChildItem(parentItem, 0, "Low-Level error-weighting options", true);
    AddChildItem(parentItem, 0, "Low-Level performace quality tradeoff options", true);
    AddChildItem(parentItem, 0, "Other options", true);
    AddRootItem(0, "Mipmap generation", true);
#endif
    QGroupBox* GBSource = new QGroupBox(tr("Source"));
    m_VlayoutSource     = new QVBoxLayout(this);

    m_HlayoutSourceName = new QHBoxLayout(this);
    m_CBSourceFile      = new QComboBox();
    connect(m_CBSourceFile, SIGNAL(currentIndexChanged(int)), this, SLOT(onSourceNameSelectionChanged(int)));

    m_HlayoutSourceName->addWidget(m_CBSourceFile);

    m_VlayoutSource->addLayout(m_HlayoutSourceName);
    GBSource->setLayout(m_VlayoutSource);

    GBDestinationFile = new QGroupBox(tr("Destination"));

    GBSource->setStyleSheet("QGroupBox { font-weight: bold; } ");
    GBDestinationFile->setStyleSheet("QGroupBox { font-weight: bold; } ");

    m_VlayoutWindow      = new QVBoxLayout(this);
    m_HlayoutName        = new QHBoxLayout(this);
    m_HlayoutDestination = new QHBoxLayout(this);
    m_HlayoutButtons     = new QHBoxLayout(this);
    m_VlayoutDestination = new QVBoxLayout(this);

    m_HlayoutName->addWidget(LName);
    m_HlayoutName->addWidget(m_LEName);
    m_HlayoutName->addWidget(m_fileFormats);

    m_HlayoutDestination->addWidget(m_lFolder);
    m_HlayoutDestination->addWidget(m_DestinationFolder);
    m_HlayoutDestination->addWidget(m_PBDestFileFolder);

    m_VlayoutDestination->addLayout(m_HlayoutName);
    m_VlayoutDestination->addLayout(m_HlayoutDestination);

    GBDestinationFile->setLayout(m_VlayoutDestination);

    m_HlayoutButtons->addStretch();
    m_HlayoutButtons->addWidget(m_PBSaveSettings);
    m_HlayoutButtons->addWidget(m_PBCancel);

    m_VlayoutWindow->addWidget(GBSource);
    m_VlayoutWindow->addWidget(GBDestinationFile);
    m_VlayoutWindow->addWidget(m_theController);
    m_VlayoutWindow->addWidget(m_infotext);
    m_VlayoutWindow->addLayout(m_HlayoutButtons);

    setLayout(m_VlayoutWindow);
}

//===================================================================
// update FileType Combobox this event is not called
// when setText is used on a LineEdit
//===================================================================
bool CSetMeshOptions::updateFileFormat(QFileInfo& fileinfo)
{
    QString filetype = fileinfo.suffix();
    filetype         = filetype.toUpper();
    int index        = m_fileFormats->findText(filetype);
    if (index != -1)
    {
        m_fileFormats->setCurrentIndex(index);
        return true;
    }

    return false;
}

void CSetMeshOptions::onSourceNameSelectionChanged(int index)
{
    if (index < 0)
        return;
    switch (m_DestinationData.m_SourceType)
    {
    case TREETYPE_3DSUBMODEL_DATA: {
        bool isImage                    = m_CBSourceFile->itemData(index).toBool();
        m_DestinationData.m_isModelData = !isImage;
        if (isImage)
        {
            // default back to BC7
            m_DestinationData.m_Compression = C_Destination_Options::eCompression_options::BC7;

            QFileInfo fileInfo(m_CBSourceFile->itemText(index));
            m_LEName->clear();
            m_DestinationData.m_compname = fileInfo.baseName();

            QFileInfo srcfileInfo(m_DestinationData.m_modelSource);
            QString   srcPath                      = srcfileInfo.absolutePath();
            m_DestinationData.m_sourceFileNamePath = srcPath + "/" + m_CBSourceFile->itemText(index);
            m_LEName->insert(m_DestinationData.m_compname + "_" + GetFormatString() + "_" + QString::number(m_extnum));
        }
        else
        {  // Processing a Model Mesh Data
            // default NONE
            m_DestinationData.m_Compression = C_Destination_Options::eCompression_options::MESH_DATA;

            QFileInfo fileInfo(m_DestinationData.m_modelDest);
            m_LEName->clear();
            m_DestinationData.m_compname = m_DestinationData.m_modelDest;
            m_LEName->insert(m_DestinationData.m_compname);
        }

        updateDisplayContent();
    }
    break;
    }
}

//==========================
// Destination NAME editing
//==========================

void CSetMeshOptions::onNameEditingFinished()
{
    // m_data.m_compname = m_LEName->displayText();
    // m_data.m_FileInfoDestinationName = m_data.m_compname;
    // m_theController->update();
}

void CSetMeshOptions::onNameTextChanged(QString text)
{
}

//===================================================================
// Check if compression is been specified
//===================================================================

void CSetMeshOptions::compressionValueChanged(QVariant& value)
{
    // Enable or Diable Some of the Dialogs Displayed Widgets
    // as per callers request
    if (m_showDestinationEXTSetting)
        m_fileFormats->show();
    else
        m_fileFormats->hide();

    if (m_showTheControllerSetting)
        m_theController->show();
    else
        m_theController->hide();

    if (m_showTheInfoTextSetting)
        m_infotext->show();
    else
        m_infotext->hide();

    // Get the source compression data
    C_Destination_Options::eCompression_options comp = (C_Destination_Options::eCompression_options&)value;

    // Backup the original source in case user cancels the dialog or wants to revert settings
    m_DestinationData.m_Compression = comp;

    QString extension           = "DDS";
    bool    compressedOptions   = false;
    bool    colorWeightOptions  = false;
    bool    alphaChannelOptions = false;
    bool    astcbitrateOptions  = false;
    bool    hdrOptions          = false;

    m_fileFormats->clear();

    if (m_propQuality)
        m_propQuality->setEnabled(true);
#ifdef USE_ENABLEHQ
    if (m_propEnableHQ)
        m_propEnableHQ->setEnabled(true);
#endif
    if (m_propChannelWeightingR)
        m_propChannelWeightingR->setEnabled(true);
    if (m_propChannelWeightingG)
        m_propChannelWeightingG->setEnabled(true);
    if (m_propChannelWeightingB)
        m_propChannelWeightingB->setEnabled(true);
    if (m_propAlphaThreshold)
        m_propAlphaThreshold->setEnabled(true);
    if (m_propAdaptiveColor)
        m_propAdaptiveColor->setEnabled(true);
    if (m_propBitrate)
        m_propBitrate->setEnabled(true);
    if (m_propDefog)
        m_propDefog->setEnabled(true);
    if (m_propExposure)
        m_propExposure->setEnabled(true);
    if (m_propKneeLow)
        m_propKneeLow->setEnabled(true);
    if (m_propKneeHigh)
        m_propKneeHigh->setEnabled(true);
    if (m_propGamma)
        m_propGamma->setEnabled(true);

    switch (comp)
    {
    case C_Destination_Options::BC6H:
    case C_Destination_Options::BC6H_SF:
        compressedOptions   = true;
        colorWeightOptions  = false;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
#ifdef _WIN32
        m_fileFormats->addItem("KTX2");
#endif
        m_fileFormats->setCurrentIndex(0);
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append(
            "Block Compression (BC) format designed to support high-dynamic range (floating point) color spaces. (e.g. .exr extension image file).");
        break;
    case C_Destination_Options::BC1:
    case C_Destination_Options::DXT1:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = true;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
#ifdef _WIN32
        m_fileFormats->addItem("KTX2");
#endif
#ifdef USE_CRN
        m_fileFormats->addItem("CRN");
#endif
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append(
            "A four component opaque (or 1-bit alpha) compressed texture format for Microsoft DirectX10. DXT1 identical to BC1.  Four bits per pixel.");
        break;
    case C_Destination_Options::BC3:
    case C_Destination_Options::DXT5:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
#ifdef _WIN32
        m_fileFormats->addItem("KTX2");
#endif
#ifdef USE_CRN
        m_fileFormats->addItem("CRN");
#endif

        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append(
            "A four component compressed texture format with interpolated alpha for Microsoft DirectX10. DXT5 identical to BC3. Eight bits per pixel.");
        break;
    case C_Destination_Options::BC2:
    case C_Destination_Options::DXT3:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
#ifdef _WIN32
        m_fileFormats->addItem("KTX2");
#endif
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append(
            "A four component compressed texture format with explicit alpha for Microsoft DirectX10. DXT3 identical to BC2. Eight bits per pixel.");
        break;
    case C_Destination_Options::ATI1N:
    case C_Destination_Options::BC4:
        compressedOptions   = true;
        colorWeightOptions  = false;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
#ifdef _WIN32
        if (comp != C_Destination_Options::ATI1N)
            m_fileFormats->addItem("KTX2");
#endif
#ifdef USE_CRN
        m_fileFormats->addItem("CRN");
#endif

        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append("A single component compressed texture format for Microsoft DirectX10. Identical to ATI1N. Four bits per pixel.");
        break;
    case C_Destination_Options::BC5:
    case C_Destination_Options::ATI2N:
    case C_Destination_Options::ATI2N_XY:
    case C_Destination_Options::ATI2N_DXT5:
        compressedOptions   = true;
        colorWeightOptions  = false;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
#ifdef _WIN32
        if ((comp == C_Destination_Options::BC5) || (comp == C_Destination_Options::BC5_S))
            m_fileFormats->addItem("KTX2");
#endif
#ifdef USE_CRN
        m_fileFormats->addItem("CRN");
#endif

        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append("A two component compressed texture format for Microsoft DirectX10. BC5 identical to ATI2N. Eight bits per pixel.");
        break;
    case C_Destination_Options::ATC_RGB:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append("A compressed RGB format.");
        break;
    case C_Destination_Options::ATC_RGBA_Explicit:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append("A compressed ARGB format with explicit alpha.");
        break;
    case C_Destination_Options::ATC_RGBA_Interpolated:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append("A compressed ARGB format with interpolated alpha.");
        break;
    case C_Destination_Options::DXT5_xGBR:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append(" DXT5 with the red component swizzled into the alpha channel. Eight bits per pixel.");
        break;
    case C_Destination_Options::DXT5_RxBG:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append("swizzled DXT5 format with the green component swizzled into the alpha channel. Eight bits per pixel.");
        break;
    case C_Destination_Options::DXT5_RBxG:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append(
            " swizzled DXT5 format with the green component swizzled into the alpha channel & the blue component swizzled into the green channel. Eight bits "
            "per pixel.");
        break;
    case C_Destination_Options::DXT5_xRBG:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append(
            "swizzled DXT5 format with the green component swizzled into the alpha channel & the red component swizzled into the green channel. Eight bits per "
            "pixel.");
        break;
    case C_Destination_Options::DXT5_RGxB:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append("swizzled DXT5 format with the blue component swizzled into the alpha channel. Eight bits per pixel.");
        break;
    case C_Destination_Options::DXT5_xGxR:
        compressedOptions   = true;
        colorWeightOptions  = true;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append(
            "two-component swizzled DXT5 format with the red component swizzled into the alpha channel & the green component in the green channel. Eight bits "
            "per pixel.");
        break;
    case C_Destination_Options::BC7:
        compressedOptions   = true;
        colorWeightOptions  = false;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
#ifdef _WIN32
        m_fileFormats->addItem("KTX2");
#endif
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append("The latest block Compression (BC) format designed to support high-quality compression of RGB and RGBA bytes color spaces.");
        break;
    case C_Destination_Options::ETC_RGB:
    case C_Destination_Options::ETC2_RGB:
        compressedOptions   = true;
        colorWeightOptions  = false;
        alphaChannelOptions = false;
        astcbitrateOptions  = false;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        m_fileFormats->addItem("DDS");
        m_fileFormats->addItem("KTX");
#ifdef _WIN32
        m_fileFormats->addItem("KTX2");
#endif
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append("ETC (Ericsson Texture Compression, lossy texture compression developed with Ericsson Research.)");
        break;
#if (OPTION_BUILD_ASTC == 1)
    case C_Destination_Options::ASTC:
        compressedOptions   = true;
        colorWeightOptions  = false;
        alphaChannelOptions = false;
        astcbitrateOptions  = true;
        if (m_DestinationData.m_SourceIsFloatFormat)
        {
            hdrOptions = true;
        }
        extension = "KTX";
        m_fileFormats->addItem("ASTC");
        m_fileFormats->addItem("KTX");
#ifdef _WIN32
        m_fileFormats->addItem("KTX2");
#endif
        m_infotext->clear();
        m_infotext->append("<b>Format Description</b>");
        m_infotext->append("ASTC (Adaptive Scalable Texture Compression),lossy block-based texture compression developed with ARM.");

        break;
#endif
    default:
        m_infotext->clear();
        m_fileFormats->addItems(m_AllFileTypes);

        if (m_propQuality)
            m_propQuality->setEnabled(false);
#ifdef USE_ENABLEHQ
        if (m_propEnableHQ)
            m_propEnableHQ->setEnabled(false);
#endif
        if (m_propChannelWeightingR)
            m_propChannelWeightingR->setEnabled(false);
        if (m_propChannelWeightingG)
            m_propChannelWeightingG->setEnabled(false);
        if (m_propChannelWeightingB)
            m_propChannelWeightingB->setEnabled(false);
        if (m_propAlphaThreshold)
            m_propAlphaThreshold->setEnabled(false);
        if (m_propAdaptiveColor)
            m_propAdaptiveColor->setEnabled(false);
        if (m_propBitrate)
            m_propBitrate->setEnabled(false);
        if (m_propDefog)
            m_propDefog->setEnabled(false);
        if (m_propExposure)
            m_propExposure->setEnabled(false);
        if (m_propKneeLow)
            m_propKneeLow->setEnabled(false);
        if (m_propKneeHigh)
            m_propKneeHigh->setEnabled(false);
        if (m_propGamma)
            m_propGamma->setEnabled(false);

        break;
    }

    if (m_propQuality)
        m_propQuality->setEnabled(compressedOptions);
#ifdef USE_ENABLEHQ
    if (m_propEnableHQ)
        m_propEnableHQ->setEnabled(compressedOptions);
#endif
    if (m_propChannelWeightingR)
        m_propChannelWeightingR->setEnabled(colorWeightOptions);
    if (m_propChannelWeightingG)
        m_propChannelWeightingG->setEnabled(colorWeightOptions);
    if (m_propChannelWeightingB)
        m_propChannelWeightingB->setEnabled(colorWeightOptions);
    if (m_propAlphaThreshold)
        m_propAlphaThreshold->setEnabled(alphaChannelOptions);
    if (m_propAdaptiveColor)
        m_propAdaptiveColor->setEnabled(colorWeightOptions);
#if (OPTION_BUILD_ASTC == 1)
    if (m_propBitrate)
        m_propBitrate->setEnabled(astcbitrateOptions);
#endif
    if (m_propDefog)
        m_propDefog->setEnabled(hdrOptions);
    if (m_propExposure)
        m_propExposure->setEnabled(hdrOptions);
    if (m_propKneeLow)
        m_propKneeLow->setEnabled(hdrOptions);
    if (m_propKneeHigh)
        m_propKneeHigh->setEnabled(hdrOptions);
    if (m_propGamma)
        m_propGamma->setEnabled(hdrOptions);

    if (m_propDXT1Alpha)
    {
        m_propDXT1Alpha->setHidden(!alphaChannelOptions);
    }

#if (OPTION_BUILD_ASTC == 1)
    if (m_propASTCBlockRate)
    {
        m_propASTCBlockRate->setHidden(!astcbitrateOptions);
    }
#endif

    if (m_propHDRProperties)
    {
        m_propHDRProperties->setHidden(!hdrOptions);
    }

    if (m_propChannelWeight)
    {
        m_propChannelWeight->setHidden(!colorWeightOptions);
    }

    int i = m_fileFormats->findText(extension);
    m_fileFormats->setCurrentIndex(i);

    if (compressedOptions)
        m_infotext->append("Destination file will be <b>Compressed</b> when processed");
    else
        m_infotext->append("Destination file will be <b>Transcoded</b> when processed");

    // Update Compression Name
    m_LEName->clear();

    switch (m_DestinationData.m_SourceType)
    {
    case TREETYPE_3DMODEL_DATA: {
        // Restrict destination to DDS files
        m_fileFormats->clear();
        QString ext = getFileExt(m_DestinationData.m_modelDest);
        m_fileFormats->addItem(ext);
        m_LEName->insert(m_DestinationData.m_compname + "_" + QString::number(m_extnum));
    }
    break;
    case TREETYPE_3DSUBMODEL_DATA: {
        m_fileFormats->clear();
        if (m_DestinationData.m_isModelData)
        {
            QString ext = getFileExt(m_DestinationData.m_sourceFileNamePath);
            m_fileFormats->addItem(ext);
            m_LEName->insert(m_DestinationData.m_compname + "_" + QString::number(m_extnum));

            if (m_propFormat)
            {
                m_propFormat->setHidden(true);
            }
            if (m_propQuality)
            {
                m_propQuality->setHidden(true);
            }
#ifdef USE_ENABLEHQ
            if (m_propEnableHQ)
            {
                m_propEnableHQ->setHidden(true);
            }
#endif
        }
        else
        {
            // Restrict destination to DDS files
            m_fileFormats->addItem("DDS");
            m_LEName->insert(m_DestinationData.m_compname + "_" + GetFormatString() + "_" + QString::number(m_extnum));

            if (m_propFormat)
            {
                m_propFormat->setHidden(false);
            }

            if (m_propQuality)
            {
                m_propQuality->setHidden(false);
            }
#ifdef USE_ENABLEHQ
            if (m_propEnableHQ)
            {
                m_propEnableHQ->setHidden(true);  // v4.2 feature set to false when ready
            }
#endif
        }
    }
    break;
    case TREETYPE_IMAGEFILE_DATA:
    default: {
        m_LEName->insert(m_DestinationData.m_compname + "_" + GetFormatString() + "_" + QString::number(m_extnum));

        if (m_propFormat)
        {
            m_propFormat->setHidden(false);
        }
        if (m_propQuality)
        {
            m_propQuality->setHidden(false);
        }
#ifdef USE_ENABLEHQ
        if (m_propEnableHQ)
        {
            m_propEnableHQ->setHidden(true);  // v4.2 feature set to false when ready
        }
#endif
    }
    break;
    }
}

//===================================================================
// Check if compression is been specified
//===================================================================

//===================================================================
// Check if compression quality value changed
//===================================================================
void CSetMeshOptions::qualityValueChanged(QVariant& value)
{
    Q_UNUSED(value);
    m_infotext->clear();
    m_infotext->append("<b>Quality</b> Applies only to compressed formats");
    m_infotext->append("Value range is 0 (Poor Quality) to 1 (High Quality) with default set at 0.05");
}

//===================================================================
// Check if red weight value changed
//===================================================================
void CSetMeshOptions::redwValueChanged(QVariant& value)
{
    Q_UNUSED(value);
    m_infotext->clear();
    m_infotext->append("<b>Red Channel Weighting</b> Applies only to compressed formats");
    m_infotext->append("Value range is 0.01 (Lowest Weight) to 1 (Highest Weight) with default set at 0.3086");
}

//===================================================================
// Check if green weight value changed
//===================================================================
void CSetMeshOptions::greenwValueChanged(QVariant& value)
{
    Q_UNUSED(value);
    m_infotext->clear();
    m_infotext->append("<b>Green Channel Weighting</b> Applies only to compressed formats");
    m_infotext->append("Value range is 0.01 (Lowest Weight) to 1 (Highest Weight) with default set at 0.6094");
}

//===================================================================
// Check if blue weight value changed
//===================================================================
void CSetMeshOptions::bluewValueChanged(QVariant& value)
{
    Q_UNUSED(value);
    m_infotext->clear();
    m_infotext->append("<b>Blue Channel Weighting</b> Applies only to compressed formats");
    m_infotext->append("Value range is 0.01 (Lowest Weight) to 1 (Highest Weight) with default set at 0.0820");
}

//===================================================================
// Check if alpha threshold value changed
//===================================================================
void CSetMeshOptions::thresholdValueChanged(QVariant& value)
{
    Q_UNUSED(value);
    m_infotext->clear();
    m_infotext->append("<b>Alpha Threshold</b> Applies only to compressed formats (with alpha channel on)");
    m_infotext->append("Value range is 1-255");
}

//===================================================================
// Check if compression is been specified
//===================================================================
void CSetMeshOptions::bitrateValueChanged(QString& actualbitrate, int& xblock, int& yblock)
{
    QString msg      = "";
    QString blockmsg = "";
    if (xblock == -1 && yblock == -1)
    {
        msg      = "Invalid input. Not supported.";
        blockmsg = "Value changed to default bit rate 8.00 (4x4).";
    }
    else
    {
        msg      = "The <b>closet bit rate</b> is " + actualbitrate;
        blockmsg = "<b>Block number</b> is (XxY): " + QString::number(xblock) + "x" + QString::number(yblock);
    }

    m_infotext->clear();
    m_infotext->append(msg);
    m_infotext->append(blockmsg);
}

//===================================================================
// Defog value changed
//===================================================================
void CSetMeshOptions::defogValueChanged(double& defog)
{
    QString msg      = "";
    QString blockmsg = "";
    if (defog < 0.00 || defog > 0.01)
    {
        msg      = "Invalid input. Not supported. Clamp to valid range";
        blockmsg = "Defog value should be in range of 0.0000 to 0.0100.";
    }

    m_infotext->clear();
    m_infotext->append(msg);
    m_infotext->append(blockmsg);
}

//===================================================================
//exposure value changed
//===================================================================
void CSetMeshOptions::exposureValueChanged(double& exposure)
{
    QString msg      = "";
    QString blockmsg = "";
    if (exposure < -10.0 || exposure > 10.0)
    {
        msg      = "Invalid input. Not supported. Clamp to valid range";
        blockmsg = "Exposure value supported is in range of -10.0 to 10.0.";
    }

    m_infotext->clear();
    m_infotext->append(msg);
    m_infotext->append(blockmsg);
}

//===================================================================
// kneelow value changed
//===================================================================
void CSetMeshOptions::kneelowValueChanged(double& kl)
{
    QString msg      = "";
    QString blockmsg = "";
    if (kl < -3.00 || kl > 3.00)
    {
        msg      = "Invalid input. Not supported. Clamp to valid range";
        blockmsg = "Knee Low value should be in range of -3.0 to 3.0.";
    }

    m_infotext->clear();
    m_infotext->append(msg);
    m_infotext->append(blockmsg);
}

//===================================================================
// knee high value changed
//===================================================================
void CSetMeshOptions::kneehighValueChanged(double& kh)
{
    QString msg      = "";
    QString blockmsg = "";
    if (kh < 3.5 || kh > 7.5)
    {
        msg      = "Invalid input. Not supported. Clamp to valid range";
        blockmsg = "Knee High value supported is in range of 3.5 to 7.5.";
    }

    m_infotext->clear();
    m_infotext->append(msg);
    m_infotext->append(blockmsg);
}

//===================================================================
// gamma value changed
//===================================================================
void CSetMeshOptions::gammaValueChanged(double& gamma)
{
    QString msg      = "";
    QString blockmsg = "";
    if (gamma < 1.0 || gamma > 2.6)
    {
        msg      = "Invalid input. Not supported. Clamp to valid range";
        blockmsg = "Gamma value supported is in range of 1.0 to 2.6.";
    }

    m_infotext->clear();
    m_infotext->append(msg);
    m_infotext->append(blockmsg);
}

// -----------------------------------------------------------
// Signaled when items focus has changed on th property view
// ----------------------------------------
void CSetMeshOptions::oncurrentItemChanged(QtBrowserItem* item)
{
    if (!item)
        return;
    m_infotext->clear();

    QtProperty* treeItem = item->property();
    QString     text;
    text = treeItem->propertyName();
    m_infotext->append("<b>" + text + "</b>");

    if (text.compare(COMPRESS_OPTIONS_FORMAT) == 0)
    {
        m_infotext->append("Sets destination image format");
    }
    else if (text.compare(COMPRESS_OPTIONS_QUALITY) == 0)
    {
        m_infotext->append(tr("Sets destinations image quality"));
        m_infotext->append(tr("For low values quality will be poor and the time to process the image will be short."));
        m_infotext->append(tr("Subsequently higher values will increase the quality and processing time"));
    }
    else if (text.compare(COMPRESS_OPTIONS_DEFOG) == 0)
    {
        m_infotext->append(tr("Remove \"fog\" from Input Float type Image (range 0.0000 t0 0.0100)."));
    }
    else if (text.compare(COMPRESS_OPTIONS_EXPOSURE) == 0)
    {
        m_infotext->append(tr("Exposure control for Input Float type Image (-10.0 to 10.0)."));
    }
    else if (text.compare(COMPRESS_OPTIONS_KNEELOW) == 0)
    {
        m_infotext->append(tr("Pixel values between kneeHigh and kneeLow set the white level of the input image. Knee Low should be in range -3.0 to 3.0."));
    }
    else if (text.compare(COMPRESS_OPTIONS_KNEEHIGH) == 0)
    {
        m_infotext->append(tr("Pixel values between kneeHigh and kneeLow set the white level of the input image. Knee High should be in range 3.5-7.5."));
    }
    else if (text.compare(COMPRESS_OPTIONS_GAMMA) == 0)
    {
        m_infotext->append(tr("Gamma correction for Input Float type Image (1.0-2.6)."));
    }
}

CSetMeshOptions::~CSetMeshOptions()
{
}

void CSetMeshOptions::resetData()
{
    m_DestinationData << m_dataOriginal;
}

QString CSetMeshOptions::GetFormatString()
{
    QMetaObject meta                = C_Destination_Options::staticMetaObject;
    int         indexCompression    = meta.indexOfEnumerator("eCompression_options");
    QMetaEnum   metaEnumCompression = meta.enumerator(indexCompression);
    QString     format              = metaEnumCompression.valueToKey(m_DestinationData.m_Compression);
    return format;
}

//===================================================================
// Called just before the display of this editor
// normally called prior to Adding or Editing data
//===================================================================

bool CSetMeshOptions::updateDisplayContent()
{
    isEditing = true;

    m_dataOriginal << m_DestinationData;

    // Check source file extension for special cases
    //m_data.m_settoUseOnlyBC6 = false;
    QFileInfo fi(m_DestinationData.m_sourceFileNamePath);
    m_srcext = fi.suffix().toUpper();

    // Compression Name
    m_LEName->clear();
    switch (m_DestinationData.m_SourceType)
    {
    case TREETYPE_3DMODEL_DATA: {
        m_DestinationData.m_FileInfoDestinationName = m_DestinationData.m_compname;
        m_LEName->insert(m_DestinationData.m_compname + "_" + QString::number(m_extnum));
    }
    break;
    case TREETYPE_3DSUBMODEL_DATA: {
        if (m_DestinationData.m_isModelData)
        {
            m_DestinationData.m_FileInfoDestinationName = m_DestinationData.m_compname;
            m_LEName->insert(m_DestinationData.m_compname + "_" + QString::number(m_extnum));

            if (m_propQuality)
            {
                m_propQuality->setHidden(true);
            }
#ifdef USE_ENABLEHQ
            if (m_propEnableHQ)
            {
                m_propEnableHQ->setHidden(true);
            }
#endif
            if (m_propFormat)
            {
                m_propFormat->setHidden(true);
            }

#ifdef USE_MESHOPTIMIZER
            m_propMeshOptimizerSettings = m_theController->getProperty(MESH_OPTIMIZER_SETTING_CLASS_NAME);

            if (m_propMeshOptimizerSettings)
                m_propMeshOptimizerSettings->setHidden(false);
#else
            m_propMeshSettings = m_theController->getProperty(MESH_SETTINGS_CLASS_NAME);
            if (m_propMeshSettings)
                m_propMeshSettings->setHidden(false);
#endif
            m_propMeshCompressionSettings = m_theController->getProperty(MESH_COMPRESSION_SETTINGS_CLASS_NAME);

            if (m_propMeshCompressionSettings)
                m_propMeshCompressionSettings->setHidden(false);
        }
        else
        {
            m_DestinationData.m_compname                = m_DestinationData.m_compname + "_" + m_srcext;
            m_DestinationData.m_FileInfoDestinationName = m_DestinationData.m_compname;
            m_LEName->insert(m_DestinationData.m_compname + "_" + GetFormatString() + "_" + QString::number(m_extnum));

            if (m_propQuality)
            {
                m_propQuality->setHidden(false);
            }
#ifdef USE_ENABLEHQ
            if (m_propEnableHQ)
            {
                m_propEnableHQ->setHidden(true);  // v4.2 feature set to false when ready
            }
#endif
            if (m_propFormat)
            {
                m_propFormat->setHidden(false);
            }
#ifdef USE_MESHOPTIMIZER
            m_propMeshOptimizerSettings = m_theController->getProperty(MESH_OPTIMIZER_SETTING_CLASS_NAME);

            if (m_propMeshOptimizerSettings)
                m_propMeshOptimizerSettings->setHidden(true);

#else
            m_propMeshSettings = m_theController->getProperty(MESH_SETTINGS_CLASS_NAME);
            if (m_propMeshSettings)
                m_propMeshSettings->setHidden(true);
#endif
            m_propMeshCompressionSettings = m_theController->getProperty(MESH_COMPRESSION_SETTINGS_CLASS_NAME);

            if (m_propMeshCompressionSettings)
                m_propMeshCompressionSettings->setHidden(true);
        }
    }
    break;
    case TREETYPE_IMAGEFILE_DATA:
    default: {
        m_DestinationData.m_compname                = m_DestinationData.m_compname + "_" + m_srcext;
        m_DestinationData.m_FileInfoDestinationName = m_DestinationData.m_compname;
        m_LEName->insert(m_DestinationData.m_compname + "_" + GetFormatString() + "_" + QString::number(m_extnum));
    }
    break;
    }

    // Update Property Managed Settings and content view
    m_theController->setObject(&m_DestinationData, true, true);
    m_theController->update();

    // Destination FileName
    QString   FileName;
    QFileInfo fileinfo;
    //if (isInit)
    //{
    //    fileinfo.setFile(m_data.m_destFileNamePath);
    //    m_DestinationFolder->setText(m_destFilePath);
    //    // Destination File Name
    //    FileName = fileinfo.baseName();
    //    if (FileName.length() <= 0) FileName = m_data.m_compname;
    //}
    //else
    {
        fileinfo.setFile(m_DestinationData.m_sourceFileNamePath);
        FileName = m_DestinationData.m_compname;
        // Destination Folder
        QDir    dir(fileinfo.absoluteDir());
        QString DestFolder = dir.absolutePath();
        if ((DestFolder.length() <= 1) && DestFolder.contains("."))
        {
            DestFolder = "./Results";
        }
        m_DestinationFolder->setText(DestFolder);
        isInit = true;
    }

    //m_DestinationFile->setText(FileName);

    m_PBSaveSettings->setEnabled(true);

    if (m_propQuality)
    {
        m_propQuality->setToolTip(STR_QUALITY_SETTING_HINT);
        m_propQuality->setEnabled(true);
        // Set  Properties for editing
        QtVariantPropertyManager* Manager = (QtVariantPropertyManager*)m_propQuality->propertyManager();
        setMinMaxStep(Manager, m_propQuality, 0.0, 1.0, 0.05, 2);
    }
#ifdef USE_ENABLEHQ
    if (m_propEnableHQ)
    {
        m_propEnableHQ->setToolTip(STR_ENABLEHQ_SETTING_HINT);
        m_propEnableHQ->setEnabled(true);
    }
#endif
    if (m_propFormat)
    {
        m_propFormat->setToolTip(STR_FORMAT_SETTING_HINT);
    }

    if (m_propChannelWeightingR)
    {
        m_propChannelWeightingR->setToolTip(STR_CHANNELWEIGHTR_SETTING_HINT);
        m_propChannelWeightingR->setEnabled(true);
        // Set  Properties for editing
        QtVariantPropertyManager* Manager = (QtVariantPropertyManager*)m_propChannelWeightingR->propertyManager();
        setMinMaxStep(Manager, m_propChannelWeightingR, 0.01, 1.0, 0.01, 2);
    }

    if (m_propChannelWeightingG)
    {
        m_propChannelWeightingG->setToolTip(STR_CHANNELWEIGHTG_SETTING_HINT);
        m_propChannelWeightingG->setEnabled(true);
        // Set  Properties for editing
        QtVariantPropertyManager* Manager = (QtVariantPropertyManager*)m_propChannelWeightingG->propertyManager();
        setMinMaxStep(Manager, m_propChannelWeightingG, 0.01, 1.0, 0.01, 2);
    }

    if (m_propChannelWeightingB)
    {
        m_propChannelWeightingB->setToolTip(STR_CHANNELWEIGHTB_SETTING_HINT);
        m_propChannelWeightingB->setEnabled(true);
        // Set  Properties for editing
        QtVariantPropertyManager* Manager = (QtVariantPropertyManager*)m_propChannelWeightingB->propertyManager();
        setMinMaxStep(Manager, m_propChannelWeightingB, 0.01, 1.0, 0.01, 2);
    }

    if (m_propBitrate)
    {
        m_propBitrate->setToolTip(STR_BITRATE_SETTING_HINT);
        m_propBitrate->setEnabled(true);
        // Set  Properties for editing
        QtVariantPropertyManager* Manager = (QtVariantPropertyManager*)m_propBitrate->propertyManager();
        setMinMaxStep(Manager, m_propBitrate, 0.00, 8.00, 0.01, 2);
    }

    if (m_propDefog)
    {
        m_propDefog->setToolTip(STR_DEFOG_SETTING_HINT);
        m_propDefog->setEnabled(true);
        // Set  Properties for editing
        QtVariantPropertyManager* Manager = (QtVariantPropertyManager*)m_propDefog->propertyManager();
        setMinMaxStep(Manager, m_propDefog, 0.000, 0.010, 0.001, 3);
    }

    if (m_propExposure)
    {
        m_propExposure->setToolTip(STR_EXPOSURE_SETTING_HINT);
        m_propExposure->setEnabled(true);
        // Set  Properties for editing
        QtVariantPropertyManager* Manager = (QtVariantPropertyManager*)m_propExposure->propertyManager();
        setMinMaxStep(Manager, m_propExposure, -10.000, 10.000, 0.125, 3);
    }

    if (m_propKneeLow)
    {
        m_propKneeLow->setToolTip(STR_KNEELOW_SETTING_HINT);
        m_propKneeLow->setEnabled(true);
        // Set  Properties for editing
        QtVariantPropertyManager* Manager = (QtVariantPropertyManager*)m_propKneeLow->propertyManager();
        setMinMaxStep(Manager, m_propKneeLow, -3.00, 3.00, 0.125, 3);
    }

    if (m_propKneeHigh)
    {
        m_propKneeHigh->setToolTip(STR_KNEEHIGH_SETTING_HINT);
        m_propKneeHigh->setEnabled(true);
        // Set  Properties for editing
        QtVariantPropertyManager* Manager = (QtVariantPropertyManager*)m_propKneeHigh->propertyManager();
        setMinMaxStep(Manager, m_propKneeHigh, 3.50, 7.50, 0.125, 3);
    }

    if (m_propGamma)
    {
        m_propGamma->setToolTip(STR_GAMMA_SETTING_HINT);
        m_propGamma->setEnabled(true);
        // Set  Properties for editing
        QtVariantPropertyManager* Manager = (QtVariantPropertyManager*)m_propGamma->propertyManager();
        setMinMaxStep(Manager, m_propGamma, 1.0, 2.6, 0.2, 1);
    }

    if (m_propAlphaThreshold)
    {
        m_propAlphaThreshold->setToolTip(STR_ALPHATHRESHOLD_HINT);
        m_propAlphaThreshold->setEnabled(true);
        // Set  Properties for editing
        QtVariantPropertyManager* Manager = (QtVariantPropertyManager*)m_propAlphaThreshold->propertyManager();
        setMinMaxStep(Manager, m_propAlphaThreshold, 0, 255, 1, 0);
    }

    if (m_propDestImage)
    {
        m_propDestImage->setHidden(true);
    }

    return true;
}

void CSetMeshOptions::setMinMaxStep(QtVariantPropertyManager* manager, QtProperty* m_prop, double min, double max, double step, int decimals)
{
    if (manager)
    {
        QtVariantProperty* prop = manager->variantProperty(m_prop);
        if (prop)
        {
            prop->setAttribute(STR_SETTING_MINIMUM, min);
            prop->setAttribute(STR_SETTING_MAXIMUM, max);
            prop->setAttribute(STR_SETTING_SINGLESTEP, step);
            prop->setAttribute(STR_SETTING_DECIMALS, decimals);
        }
    }
}

void CSetMeshOptions::onPBCancel()
{
    isNoSetting = false;
    resetData();
    hide();
    isEditing = false;

    // Obtain the Parent and its data
    // and reset the num of extension counts for that image
    QTreeWidgetItem* parent = m_item->parent();
    if (parent)
    {
        // Varify its root
        QVariant v               = parent->data(TREE_LevelType, Qt::UserRole);
        int      ParentlevelType = v.toInt();
        if (ParentlevelType == TREETYPE_IMAGEFILE_DATA)
        {
            QVariant       v           = parent->data(TREE_SourceInfo, Qt::UserRole);
            C_Source_Info* m_imagefile = v.value<C_Source_Info*>();
            if (m_imagefile)
                m_imagefile->m_extnum--;
        }
        if (ParentlevelType == TREETYPE_3DMODEL_DATA)
        {
            QVariant        v            = parent->data(TREE_SourceInfo, Qt::UserRole);
            C_3DModel_Info* m_sourcefile = v.value<C_3DModel_Info*>();
            if (m_sourcefile)
                m_sourcefile->m_extnum -= 2;
        }
    }
}

void CSetMeshOptions::SaveCompressedInfo()
{
    if (!isNoSetting)
    {
        m_DestinationData.m_compname = m_LEName->displayText();
    }
    else
    {
        QString temp = m_LEName->displayText();
        int     ind  = temp.indexOf('_');
        temp         = temp.mid(ind);
        m_DestinationData.m_compname.append(temp);
    }

    if (m_DestinationData.m_compname == "")
    {
        QMessageBox msgBox;
        msgBox.setText("Please enter a valid filename. Thanks");
        msgBox.exec();
        return;
    }

    QString ImageExt = m_fileFormats->currentText();

    // FileName+Ext
    m_DestinationData.m_FileInfoDestinationName = m_DestinationData.m_compname + "." + ImageExt;

    // Path
    QString Path = m_DestinationFolder->text();
    QDir    dir(Path);
    QString finalPath = dir.absolutePath();

    // CMP_CreateDir() only creates a single level, so a destination whose parent folders are also
    // missing needs the recursive variant or the write probe below would redirect unnecessarily.
    if (!CMP_DirExists(finalPath.toStdString().c_str()))
        CMP_CreateDirTree(finalPath.toStdString());

    // Probe the folder rather than the target file: opening the file to test for write
    // access would truncate a result that has not been regenerated yet.
    if (!CMP_IsDirWritable(finalPath.toStdString()))
    {
        QString userResultsPath = CMP_GetUserResultsFolder();
        if (!userResultsPath.isEmpty())
        {
            finalPath = userResultsPath;

            cpMainComponents* temp = (cpMainComponents*)(m_parent);
            if (temp)
            {
                if (temp->m_CompressStatusDialog)
                {
                    temp->m_CompressStatusDialog->onClearText();
                    temp->m_CompressStatusDialog->showOutput();
                }
            }
            PrintInfo("Warning: Destination path is not writable, the path has been changed to %s\n", finalPath.toStdString().c_str());
        }
        else
            PrintInfo("Warning: Destination path %s is not writable\n", finalPath.toStdString().c_str());
    }

    // Path+FileName
    finalPath.append("/");
    finalPath.append(m_DestinationData.m_compname);

    if (m_DestinationData.m_SourceType == TREETYPE_3DMODEL_DATA)
    {
        QString fileExt(getFileExt(m_DestinationData.m_modelSource));
        fileExt.prepend(".");
        finalPath.append(fileExt);
        m_DestinationData.m_modelDest = finalPath;
    }
    else
    {
        if (m_fileFormats->currentIndex() >= 0 && !(ImageExt.isEmpty()))
        {
            finalPath.append(".");
            finalPath.append(ImageExt);

            // obj file "add destination setting" has destination filename same as 3dsubmodule
            if (ImageExt == "obj" || ImageExt == "OBJ")
            {
                finalPath = m_DestinationData.m_modelDest;
            }
        }
        else
            finalPath.append(".DDS");
    }

    m_DestinationData.m_destFileNamePath = finalPath;

    // check if user change the file name or path
    if (m_DestinationData.m_destFileNamePath.compare(m_dataOriginal.m_destFileNamePath) != 0)
        m_DestinationData.m_data_has_been_changed = true;

    emit SaveCompressSettings(m_item, m_DestinationData);
    hide();
    isEditing = false;
}

void CSetMeshOptions::PBSaveCompressSetting()
{
    if (!isNoSetting)
    {
        SaveCompressedInfo();
    }
    else
        hide();
    //    else
    //    {
    //        QTreeWidgetItem *itr = m_items.back();
    //        if (!m_items.empty())
    //            m_items.pop_back();
    //        else return;
    //
    //        while (itr)
    //        {
    //            QVariant v = itr->data(TREE_SourceInfo, Qt::UserRole);
    //            C_Source_Info *m_imagefile = v.value<C_Source_Image *>();
    //            QFileInfo fileinfo(m_imagefile->m_Name);
    //            m_data.m_sourceFileNamePath = m_imagefile->m_Full_Path;
    //            m_data.m_SourceImageSize = m_imagefile->m_ImageSize;
    //            m_data.m_SourceIscompressedFormat = CMP_IsCompressedFormat(m_imagefile->m_Format);
    //
    //            int count = itr->childCount();
    //
    //            if (m_imagefile->m_extnum <= count)
    //                m_imagefile->m_extnum = count;
    //
    //            m_extnum = m_imagefile->m_extnum++;
    //            m_data.m_Width = m_imagefile->m_Width;
    //            m_data.m_Height = m_imagefile->m_Height;
    //
    //            m_data.m_compname = fileinfo.baseName();
    //
    //            m_data.m_editing = false;
    //            m_item = itr;
    //
    //            SaveCompressedInfo();
    //
    //            if (!m_items.empty())
    //            {
    //                itr = m_items.back();
    //                m_items.pop_back();
    //            }
    //            else
    //                break;
    //        }
    //
    //        isNoSetting = false; //reset value
    //        m_LEName->setEnabled(true); //re-enable file naming
    //    }
}

// update FileType Combobox this event is not called
// when setText is used on a LineEdit
void CSetMeshOptions::onDestinationFileEditingFinished()
{
    QFileInfo fileinfo(m_DestinationFile->text());

    // Update the FileType combobox
    updateFileFormat(fileinfo);

    // Strip extension from filename
    m_DestinationFile->setText(fileinfo.baseName());
}

void CSetMeshOptions::onDestFileFolder()
{
    QString fileFolder = QFileDialog::getExistingDirectory(this, tr("Destination Folder"), m_DestinationFolder->text());
    if (fileFolder.length() > 0)
    {
        m_DestinationFolder->setText(fileFolder);
    }
}