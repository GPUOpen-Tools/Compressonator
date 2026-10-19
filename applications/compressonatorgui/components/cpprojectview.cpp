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

#include "cpprojectview.h"

#include "cmdline.h"

#ifdef USE_MESHOPTIMIZER
#include "../../_plugins/cmesh/mesh_optimizer/mesh_optimizer.h"
#else
#include "../../_plugins/cmesh/tootle/mesh_tootle.h"
#endif
#include "cpmaincomponents.h"

#ifdef USE_ASSIMP
#include <assimp/Exporter.hpp>
#endif

#include <gltf/cmp_gltfcommon.h>
#include <gltf/tiny_gltf2.h>
#include <gltf/tiny_gltf2_utils.h>

#include <fstream>
#include <chrono>
#include <thread>

#ifndef _WIN32
#include <unistd.h>  // for usleep()
#endif

#define STATUS_SUCCESS (0x00000000)
// ToDo(s)
// Clean up allocated memory for each tree nodes m_data

#define STR_AddDestinationSetting "Add destination settings..."
#define STR_AddModelDestinationSetting "Add model destination settings..."

//extern C_Application_Options::ImageEncodeWith encodewith;
bool                           g_useCPUEncode = true;
static signalProcessMsgHandler static_processmsghandler;
extern void                    GetSupportedFileFormats(QList<QByteArray>& g_supportedFormats);
extern PluginManager           g_pluginManager;
extern int                     g_MipLevel;
extern float                   g_fProgress;
extern C_Application_Options   g_Application_Options;
extern CMIPS*                  g_GUI_CMIPS;
extern double                  timeStampsec();

// eventually place this in a separate common utils file or something
static int GetItemType(const QTreeWidgetItem* item)
{
    if (!item)
        return 0;

    QVariant data = item->data(TREE_LevelType, Qt::UserRole);
    return data.toInt();
}

void UpdateDestglTFWithFile(QString modelSource, QString modelDest, QString file, QString newfile, bool userdeleted)
{
    std::ifstream fstreamsrc(modelSource.toStdString());
    if (!fstreamsrc)
    {
        QString error = "Error:Reading " + modelSource + " failed.\n";
        PrintInfo(error.toStdString().c_str());
        if (!(QFile::exists(modelSource)))
        {
            error = "Error: " + modelSource + " does not exist.\n";
            PrintInfo(error.toStdString().c_str());
        }
        return;
    }

    // Load the glTF json text file
    nlohmann::json gltfsrc;
    fstreamsrc >> gltfsrc;
    fstreamsrc.close();

    std::ifstream fstreamdest(modelDest.toStdString());
    if (!fstreamdest)
    {
        QString error = "Error:Reading " + modelDest + " failed. glTF failed to be updated.\n";
        PrintInfo(error.toStdString().c_str());
        if (!(QFile::exists(modelDest)))
        {
            error = "Error: " + modelDest + " does not exist.\n";
            PrintInfo(error.toStdString().c_str());
        }
        return;
    }

    nlohmann::json gltfdest;
    fstreamdest >> gltfdest;
    fstreamdest.close();

    // Did the OS really close the file in time for us to write too again!
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    //image section of gltf file
    auto         srcimages = gltfsrc["images"];
    unsigned int i         = 0;
    bool         updated   = false;

    while (i < srcimages.size())
    {
        std::string srcname = srcimages[i]["uri"].get<std::string>();

        // extract source texture filename + path
        QString   qsrcname = QString::fromStdString(srcname);
        QFileInfo fileInfosrc(qsrcname);
        QString   srcfilename(fileInfosrc.fileName());

        QFileInfo fileInfo(file);
        QString   filename(fileInfo.fileName());
        if (srcfilename == filename)
        {
            updated = true;
            if (newfile == "")
                gltfdest["images"][i]["uri"] = qsrcname.toStdString();
            else
            {
                // texture filename with path
                QString path                 = fileInfosrc.path();
                gltfdest["images"][i]["uri"] = (path + "/" + newfile).toStdString();
            }
            break;
        }
        i++;
    }

    if (!updated)
    {
        auto srcmesh = gltfsrc["buffers"];
        for (unsigned int i = 0; i < srcmesh.size(); i++)
        {
            std::string srcname = srcmesh[i]["uri"].get<std::string>();

            // extract source mesh filename + path
            QString   qsrcname = QString::fromStdString(srcname);
            QFileInfo fileInfosrc(qsrcname);
            QString   srcfilename(fileInfosrc.fileName());

            QFileInfo fileInfo(file);
            QString   filename(fileInfo.fileName());
            if (srcfilename == filename)
            {
                updated = true;
                if (newfile == "")
                    gltfdest["buffers"][i]["uri"] = qsrcname.toStdString();
                else
                {
                    // mesh filename with path
                    QString path                  = fileInfosrc.path();
                    gltfdest["buffers"][i]["uri"] = (path + "/" + newfile).toStdString();
                }
                break;
            }
        }
    }

    // Remove the old file first then stream in a new one
    std::ofstream ofstreamdest(modelDest.toStdString(), std::ios_base::out);
    if (!ofstreamdest)
    {
        QString error = "<b>Error:<b> opening gltf compressed file for update: " + modelDest + "[" + strerror(errno) + "]";
        if (!userdeleted)
        {
            PrintInfo(error.toStdString().c_str());
        }
        else
        {
            QMessageBox msgBox;
            msgBox.setText(error);
            msgBox.exec();
        }
        return;
    }
    ofstreamdest << gltfdest;
    ofstreamdest.close();
}

// Checks the nodes parent, if its a 3DModel the items destination FileName path
// is replaced in the gltf file using the items source FileName Path as referance
void UpdateDestglTFAfterProcess(QTreeWidgetItem* item)
{
    if (item)
    {
        QTreeWidgetItem* parent = item->parent();
        if (parent)
        {
            // Verify its root
            int parentLevelType = GetItemType(parent);
            if (parentLevelType == TREETYPE_3DSUBMODEL_DATA || parentLevelType == TREETYPE_3DMODEL_DATA)
            {
                QVariant               data   = item->data(TREE_SourceInfo, Qt::UserRole);
                C_Destination_Options* m_data = data.value<C_Destination_Options*>();
                if (m_data)
                {
                    QFileInfo destfileInfo(m_data->m_destFileNamePath);
                    QString   destfilename(destfileInfo.fileName());
                    UpdateDestglTFWithFile(m_data->m_modelSource, m_data->m_modelDest, m_data->m_sourceFileNamePath, destfilename, false);
                }
            }
        }
    }
}

ProjectView::ProjectView(const QString title, CompressStatusDialog* StatusDialog, QWidget* parent)
{
    setWindowTitle(title);
    m_parent               = parent;
    m_CompressStatusDialog = StatusDialog;

    m_processBusy = false;

    // Tracks number of items added to the Project View
    // Includes (+) Add ... items

    m_NumItems = 0;

    m_CurrentItem = NULL;

    // True when any changes were made to the project

    m_saveProjectChanges  = false;
    m_AllItemsSelected    = false;
    ContextMenu_ImageItem = NULL;

    // Tracks what Compressed Image Tree Item has been clicked on
    m_CurrentCompressedImageItem = NULL;
    g_bCompressing               = false;

    // Enables diplay of checked box next to items
    m_EnableCheckedItemsView                      = false;
    m_processFromContext                          = false;
    m_globalProcessSetting.m_Quality              = 1.0f;
    m_globalProcessSetting.m_Refine_Steps         = 0;
    m_globalProcessSetting.m_GlobalSettingEnabled = false;

    m_newWidget = new QWidget(parent);
    m_layout    = new QGridLayout(m_newWidget);
    SetupTreeView();
    m_layout->addWidget(m_projectTreeView);
    m_newWidget->setLayout(m_layout);
    setWidget(m_newWidget);

    m_curProjectFilePathName = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir(m_curProjectFilePathName).exists())
        QDir().mkdir(m_curProjectFilePathName);

    m_curProjectFilePathName.append("/");
    m_curProjectFilePathName.append("NewProject");
    m_curProjectName = "NewProject";

    // For view image diff from selected child file
    m_curDiffSourceFile = "";
    m_curDiffDestFile   = "";

    // Get Qt image read formats
    GetSupportedFileFormats(m_supportedFormats);
    // add list to Image Dialog Filter variable
    m_ImageFileFilter = "Images (";  //  "Image Files (*.*);;";

    QString imageList = "";

    // QList<QByteArray>::Iterator i;
    // for (i = m_supportedFormats.begin(); i != m_supportedFormats.end(); ++i) {
    //     QByteArray fformat = (*i);
    //     m_ImageFileFilter.append("*.");
    //     m_ImageFileFilter.append(fformat);
    //     m_ImageFileFilter.append(" ");
    //
    //     imageList.append("*.");
    //     imageList.append(fformat);
    //     imageList.append(";;");
    // }

    m_ImageFileFilter.append(");;");
    m_ImageFileFilter.append(imageList);

    connect(&static_processmsghandler, SIGNAL(signalProcessMessage()), this, SLOT(onSignalProcessMessage()));

    // Diff any 2 image file dialog
    m_diffImageDialog = new acDiffImage(this);
    m_diffImageDialog->hide();

    // Run Analysis on Mesh data dialog
    m_3DMeshAnalysisDlg = new ac3DMeshAnalysis(this);
    m_3DMeshAnalysisDlg->hide();

    // Image loader
    m_imageloader      = new CImageLoader();
    m_newProjectwindow = new cpNewProject();
    connect(m_newProjectwindow, SIGNAL(OnSetNewProject(QString&)), this, SLOT(onSetNewProject(QString&)));
}

// Pick the first free "<baseName>.cprj", "<baseName> (2).cprj", ... in dir.
//
// Redirected projects are opened WriteOnly, which truncates, so two different read only
// projects that share a base name would otherwise overwrite each other's redirected copy.
// Returns an empty string when no free candidate was found.
static QString freeProjectFilePath(const QDir& dir, const QString& baseName)
{
    QString candidate = dir.absoluteFilePath(baseName + ".cprj");
    for (int suffix = 2; QFileInfo::exists(candidate) && suffix < 1000; ++suffix)
        candidate = dir.absoluteFilePath(QString("%1 (%2).cprj").arg(baseName).arg(suffix));

    if (QFileInfo::exists(candidate))
        return QString();

    return candidate;
}

// Expands $(CMP_SAMPLES) in sample .cprj paths to CMP_GetSampleDataDir().
static QString resolveProjectPath(const QString& path)
{
    if (!path.contains(CMP_PROJECT_SAMPLES_TOKEN))
        return path;

    QString resolved = path;
    resolved.replace(CMP_PROJECT_SAMPLES_TOKEN, QString::fromStdString(CMP_GetSampleDataDir()));
    return QDir::toNativeSeparators(QDir::cleanPath(resolved));
}

void ProjectView::setCurrentProjectName(QString filePathName)
{
    QFileInfo fileInfo(filePathName);
    QString   filename;
    if (filePathName.contains(".cprj"))
        filename = fileInfo.completeBaseName();
    else
    {
        QString   fitempo = QString(filePathName + ".cprj");
        QFileInfo fileInfo2(fitempo);
        filename = fileInfo2.completeBaseName();
    }

    // Probe write access without ever creating or deleting the requested path: an earlier
    // existence check cannot be trusted, so a project another process creates in between must
    // not be opened and then removed. Qt 5.9 (Linux) has no exclusive-create open mode, so the
    // no-file case probes the parent folder instead.
    bool isWritable;
    if (QFileInfo::exists(filePathName))
    {
        // ReadWrite does not truncate, and the file is left alone either way.
        QFile file(filePathName);
        isWritable = file.open(QIODevice::ReadWrite);
        file.close();
    }
    else
    {
        isWritable = CMP_IsDirWritable(fileInfo.absolutePath().toStdString());
    }

    if (!isWritable)
    {
        // Redirect relative to the REQUESTED path, not the previously loaded project: using
        // the old project's directory silently sent a new project to an unrelated folder.
        // If the requested folder is itself unwritable (e.g. a sample project under
        // %ProgramFiles%), fall back to the per-user application data folder.
        //
        // The redirect target is writable, so saveProjectFile() opens (and truncates) it
        // without reaching its own collision handling; the free candidate has to be chosen
        // here, otherwise a second read only project with the same base name overwrites the
        // first one's redirected copy.
        QString appDataPath = QString::fromStdString(CMP_GetUserDataDir("projects"));
        QString redirected;
        if (!appDataPath.isEmpty())
            redirected = freeProjectFilePath(QDir(appDataPath), filename);

        if (!redirected.isEmpty())
            m_curProjectFilePathName = redirected;
        else
            m_curProjectFilePathName = QDir(fileInfo.absolutePath()).absoluteFilePath(filename + ".cprj");
#ifdef _WIN32
        m_curProjectFilePathName.replace("/", "\\");
#endif
    }
    else
    {
        m_curProjectFilePathName = filePathName;
    }

    m_curProjectName = filename;

    if (m_parent)
        m_parent->setWindowTitle(filename);
}

void ProjectView::SignalUpdateData(QTreeWidgetItem* item, int levelType)
{
    if (!item)
        return;

    QVariant v = item->data(TREE_SourceInfo, Qt::UserRole);
    switch (levelType)
    {
    case TREETYPE_Double_Click_here_to_add_files:
        emit UpdateData(&m_globalProcessSetting);
        break;
    case TREETYPE_MESH_DATA:
    case TREETYPE_COMPRESSION_DATA: {
        C_Destination_Options* m_data = v.value<C_Destination_Options*>();
        if (m_data)
        {
            if ((m_globalProcessSetting.m_GlobalSettingEnabled) && (g_Application_Options.m_ImageEncode != C_Application_Options::ImageEncodeWith::GPU_HW))
            {
                m_data->m_globalSetting_quality        = m_globalProcessSetting.m_Quality;
                m_data->m_globalSetting_refine_steps   = m_globalProcessSetting.m_Refine_Steps;
                m_data->m_globalSetting_qualityEnabled = true;
            }
            else
                m_data->m_globalSetting_qualityEnabled = false;

            emit UpdateData(m_data);
        }
    }
    break;
    case TREETYPE_3DMODEL_DATA: {
        C_3DModel_Info* m_data = v.value<C_3DModel_Info*>();
        if (m_data)
            emit UpdateData(m_data);
    }
    break;
    case TREETYPE_3DSUBMODEL_DATA: {
        C_3DSubModel_Info* m_data = v.value<C_3DSubModel_Info*>();
        if (m_data)
            emit UpdateData(m_data);
    }
    break;
    case TREETYPE_VIEWMESH_ONLY_NODE: {
        C_Mesh_Buffer_Info* data = v.value<C_Mesh_Buffer_Info*>();
        if (data)
        {
            emit UpdateData(data);
        }
    }
    case TREETYPE_VIEWIMAGE_ONLY_NODE:
    case TREETYPE_IMAGEFILE_DATA:
    default: {
        C_Source_Info* imagedata = v.value<C_Source_Info*>();
        if (imagedata)
        {
            // Need a better way to do this via Objects DATA Properties
            if (imagedata->m_MipImages)
            {
                if (imagedata->m_MipImages->mipset)
                {
                    if (imagedata->m_MipImages->mipset->m_nMipLevels >= 1)
                        imagedata->m_Mip_Levels = imagedata->m_MipImages->mipset->m_nMipLevels;
                    imagedata->m_Depth       = imagedata->m_MipImages->mipset->m_nDepth;       // depthsupport
                    imagedata->m_TextureType = imagedata->m_MipImages->mipset->m_TextureType;  // depthsupport
                }
            }
            emit UpdateData(imagedata);
        }
    }
    break;
    }
}

bool ProjectView::AnySelectedItems()
{
    bool userSelected = false;

    int numSelected = GetNumSelectedDestItems();

    if (numSelected > 0)
    {
        userSelected = true;
    }

    if (m_CompressStatusDialog)
    {
        m_CompressStatusDialog->onClearText();
        m_CompressStatusDialog->showOutput();
    }

    if (!userSelected)
    {
        // We did not find any selected compress images
        // Try the current selected item
        QTreeWidgetItem* item = m_projectTreeView->currentItem();

        // Try project view items
        if (!item)
        {
            // Parse the Project view tree
            QTreeWidgetItemIterator it(m_projectTreeView);
            while (*it)
            {
                QString name       = (*it)->text(0);
                int     itemType   = GetItemType(*it);
                int     childcount = (*it)->childCount();

                if (itemType == TREETYPE_IMAGEFILE_DATA)
                {
                    // "Source File" = FilePathName);
                    if (childcount >= 1)
                    {
                        item = (*it);
                        break;
                    }
                }

                // last item should be a vaild one unless we ended in a
                // child loop that was the last element in the tree if so break out were done..
                if (*it)
                    ++it;
                else
                    break;
            }
        }

        if (item)
        {
            Tree_setAllItemsSelected();
            m_AllItemsSelected = true;
            return false;
        }
    }

    if (GetNumDestItems() == 0)
    {
        if (m_CompressStatusDialog)
        {
            m_CompressStatusDialog->appendText("Please open or create a project file and add/select destination images to process!");
        }
        return false;
    }

    return true;
}

bool ProjectView::saveImageAs()
{
    QTreeWidgetItem* item = m_projectTreeView->currentItem();
    if (!item)
    {
        if (m_CompressStatusDialog)
        {
            m_CompressStatusDialog->appendText("Please load or click to select the image that you would like to save.");
        }
        return false;
    }
    else
    {
        QVariant  v        = item->data(TREE_SourceInfo, Qt::UserRole);
        QString   tempName = item->text(0);
        QFileInfo fileInfo(tempName);
        QString   imgFileName = fileInfo.completeBaseName();
        imgFileName.append("_saved");
        C_Source_Info* data = v.value<C_Source_Info*>();
        if (data == nullptr)
        {
            if (m_CompressStatusDialog)
            {
                m_CompressStatusDialog->appendText("Please load or click to select a valid image (only 2D texture) that you would like to save.");
                m_CompressStatusDialog->showOutput();
            }
            return false;
        }
        if (data->m_MipImages)
            if (data->m_MipImages->mipset)
            {
                QString filePathName = QFileDialog::getSaveFileName(this, tr("Save image as"), imgFileName, tr("Image files (*.dds)"));
                if (filePathName.length() == 0)
                    return false;

                if (m_parent)
                    m_parent->setWindowTitle(imgFileName);

                PluginInterface_Image* plugin_Image;
                plugin_Image = reinterpret_cast<PluginInterface_Image*>(g_pluginManager.GetPlugin("IMAGE", "DDS"));
                imgFileName.append(".dds");

                if (AMDSaveMIPSTextureImage(filePathName.toStdString().c_str(), data->m_MipImages->mipset, false, g_CmdPrams.CompressOptions) != 0)
                {
                    if (m_CompressStatusDialog)
                    {
                        m_CompressStatusDialog->onClearText();
                        m_CompressStatusDialog->showOutput();
                    }
                    PrintInfo("Error: saving image fail.\n");
                    return false;
                }
                if (m_CompressStatusDialog)
                {
                    m_CompressStatusDialog->onClearText();
                    m_CompressStatusDialog->showOutput();
                }
                PrintInfo("Image file: %s saved successfully.\n", imgFileName.toStdString().c_str());
            }
            else
            {
                if (m_CompressStatusDialog)
                {
                    m_CompressStatusDialog->onClearText();
                    m_CompressStatusDialog->showOutput();
                }
                PrintInfo("Error: Image mipset not found. Saving failed.\n");
                return false;
            }
    }
    return true;
}

void ProjectView::onMenuBarCompression()
{
    int numSelectedItems = GetNumSelectedItems();

    if (numSelectedItems <= 0)
    {
        AddSettingtoEmptyTree();
        if (g_bAbortCompression)
            return;
        Tree_setAllItemsSelected();
        m_AllItemsSelected = true;
    }
    else
    {
        // for any selected images that don't already have destination files, we create default destinations

        std::vector<QTreeWidgetItem*> emptyItems;

        auto images = GetAllSelectedImages();
        for (auto& image : images)
        {
            if (image->childCount() == 1)
                emptyItems.push_back(image);
        }

        AddDefaultDestToItems(emptyItems);

        if (g_bAbortCompression)
            return;

        for (auto& item : emptyItems)
            Tree_selectAllChildItems(item);
    }

    CompressSelectedItems();
}

ProjectView::~ProjectView()
{
    g_bAbortCompression = true;

    if (m_imageloader)
    {
        delete m_imageloader;
        m_imageloader = NULL;
    }

    if (m_newProjectwindow)
    {
        delete m_newProjectwindow;
        m_newProjectwindow = NULL;
    }

    if (m_diffImageDialog)
    {
        delete m_diffImageDialog;
        m_diffImageDialog = NULL;
    }

    if (m_3DMeshAnalysisDlg)
    {
        delete m_3DMeshAnalysisDlg;
        m_3DMeshAnalysisDlg = NULL;
    }
}

void ProjectView::run3DMeshAnalysis(CMODEL_DATA* meshData, CMODEL_DATA* meshDataOri)
{
    if (!meshData)
        return;
    if (m_3DMeshAnalysisDlg)
    {
        m_3DMeshAnalysisDlg->m_meshData = meshData->m_meshData;

        if (meshDataOri)
        {
            m_3DMeshAnalysisDlg->m_meshDataCompare = meshDataOri->m_meshData;
        }
        else
        {
            m_3DMeshAnalysisDlg->m_fileNameCompare = "";
        }
        m_3DMeshAnalysisDlg->cleanText();
        m_3DMeshAnalysisDlg->show();
    }
}

void ProjectView::diffImageFiles()
{
    if (m_diffImageDialog)
    {
        m_diffImageDialog->m_RecentImageDir       = m_RecentImageDirOpen;
        m_diffImageDialog->m_SupportedImageFormat = m_ImageFileFilter;

        m_diffImageDialog->m_file1Name->clear();
        m_diffImageDialog->m_file1Name->addItem("");
        m_diffImageDialog->m_file1Name->addItems(m_ImagesinProjectTrees);

        // find the index of the selected diff file from the drop down list
        int index1 = m_diffImageDialog->m_file1Name->findText(m_curDiffSourceFile);
        if (index1 != -1)
        {
            m_diffImageDialog->m_file1Name->setCurrentIndex(index1);
        }
        else
        {
            m_diffImageDialog->m_file1Name->setCurrentText(m_curDiffSourceFile);
        }
        m_diffImageDialog->m_file2Name->clear();
        m_diffImageDialog->m_file2Name->addItem("");
        m_diffImageDialog->m_file2Name->addItems(m_ImagesinProjectTrees);
        int index2 = m_diffImageDialog->m_file1Name->findText(m_curDiffDestFile);
        if (index2 != -1)
        {
            m_diffImageDialog->m_file2Name->setCurrentIndex(index2);
        }
        else
        {
            m_diffImageDialog->m_file2Name->setCurrentText(m_curDiffDestFile);
        }

        m_diffImageDialog->show();

        //reset the diff image file name
        m_curDiffSourceFile = "";
        m_curDiffDestFile   = "";
    }
}

bool ProjectView::OpenImageFile()
{
    // Add new file
    QStringList ls = QFileDialog::getOpenFileNames(this, tr("Open image file(s)"), m_RecentImageDirOpen, m_ImageFileFilter);
    if (!ls.isEmpty())
    {
        for (int i = 0; i < ls.size(); i++)
        {
            C_Source_Info* m_dataout = NULL;
            if (Tree_AddImageFile(ls[i], 0, &m_dataout))
            {
                QFileInfo FileInfo(ls[i]);
                emit      AddedImageFile(ls[i]);
                m_RecentImageDirOpen = FileInfo.path();
            }
        }
        m_saveProjectChanges = true;
        return true;
    }
    return false;
}

//=======================================================

int ProjectView::PromptSaveChanges()
{
    QMessageBox msgBox;
    QString     msg;
    msg.append("The project \"");
    msg.append(m_curProjectName);
    msg.append("\" has been modified.");
    msgBox.setText(msg);
    msgBox.setInformativeText("Do you want to save your changes?");
    msgBox.addButton("SaveAs", QMessageBox::AcceptRole);
    msgBox.setStandardButtons(QMessageBox::Discard | QMessageBox::Cancel);
    return msgBox.exec();
}

void ProjectView::SelectImageItem(QString filePathName)
{
    static QString lastfilePathName = "";

    // Already Selected on Treeview then exit
    if (lastfilePathName.compare(filePathName) == 0)
        return;

    QTreeWidgetItem* it = Tree_FindImageItem(filePathName, true);

    if (it)
    {
        m_projectTreeView->setCurrentItem(it);
        // Update the property view for the item selected
        int levelType = GetItemType(it);

        if ((levelType == TREETYPE_IMAGEFILE_DATA) || (levelType == TREETYPE_VIEWIMAGE_ONLY_NODE))
            emit OnSourceImage(it->childCount());
        else if (levelType == TREETYPE_COMPRESSION_DATA)
            emit OnDecompressImage();

        lastfilePathName = GetDestinationFileNamePath(it);
        if (lastfilePathName.length() == 0)
            lastfilePathName = GetSourceFileNamePath(it);

        SignalUpdateData(it, levelType);
    }
}

void ProjectView::clearProjectTreeView()
{
    // Find the item and set it as selected
    QTreeWidgetItemIterator it(m_treeRootItem);
    while (*it)
    {
        DeleteItemData(*it, false);
        ++it;
    }

    // Clean the entire tree
    m_projectTreeView->clear();

    // add a new root node
    Tree_AddRootNode();
}

void ProjectView::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Delete) && (!g_bCompressing))
    {
        // handle the key press, perhaps giving the item text a default value
        UserDeleteItems();
        event->accept();
    }
}

QString ProjectView::GetSourceFileNamePath(QTreeWidgetItem* item)
{
    QString filePathName = "";
    if (!item)
        return filePathName;

    int itemType = GetItemType(item);

    switch (itemType)
    {
    case TREETYPE_3DMODEL_DATA: {
        QVariant        v    = item->data(TREE_SourceInfo, Qt::UserRole);
        C_3DModel_Info* data = v.value<C_3DModel_Info*>();
        if (data)
            filePathName = data->m_Full_Path;
    }
    break;
    case TREETYPE_3DSUBMODEL_DATA: {
        QVariant           v    = item->data(TREE_SourceInfo, Qt::UserRole);
        C_3DSubModel_Info* data = v.value<C_3DSubModel_Info*>();
        if (data)
            filePathName = data->m_Full_Path;
    }
    break;
    case TREETYPE_IMAGEFILE_DATA:
    case TREETYPE_VIEWIMAGE_ONLY_NODE: {
        QVariant v = item->data(TREE_SourceInfo, Qt::UserRole);
        if (!v.Invalid)
        {
            C_Source_Info* data = v.value<C_Source_Info*>();
            if (data)
                filePathName = data->m_Full_Path;
        }
    }
    break;
    case TREETYPE_VIEWMESH_ONLY_NODE: {
        QVariant            v    = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Mesh_Buffer_Info* data = v.value<C_Mesh_Buffer_Info*>();
        if (data)
            filePathName = data->m_Full_Path;
    }
    break;
    case TREETYPE_MESH_DATA:
    case TREETYPE_COMPRESSION_DATA: {
        QVariant               v    = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Destination_Options* data = v.value<C_Destination_Options*>();
        if (data)
            filePathName = data->m_sourceFileNamePath;
    }
    break;
    default:
        filePathName = "";
    }

    return filePathName;
}

QString ProjectView::GetDestinationFileNamePath(QTreeWidgetItem* item)
{
    QString filePathName = "";
    if (!item)
        return filePathName;

    int itemType = GetItemType(item);

    switch (itemType)
    {
    case TREETYPE_MESH_DATA:
    case TREETYPE_COMPRESSION_DATA: {
        QVariant               v    = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Destination_Options* data = v.value<C_Destination_Options*>();
        if (data)
            filePathName = data->m_destFileNamePath;
    }
    break;
    default:
        filePathName = "";
    }
    return filePathName;
}

// Item delete can be from two cases:
// 1: From app where projects files are loaded or removed
// 2: User manually removes a node and its content from the project
//    In this case we will also add addtional procesing for cases where
//    Image nodes attached to Models may need further processing
void ProjectView::DeleteItemData(QTreeWidgetItem* item, bool userdeleted)
{
    if (!item)
        return;

    QVariant v         = item->data(TREE_LevelType, Qt::UserRole);
    int      levelType = v.toInt();

    switch (levelType)
    {
    case TREETYPE_3DMODEL_DATA: {  // Project Explorer Root tree item
        // Get the Image Data linked to this node
        v                    = item->data(TREE_SourceInfo, Qt::UserRole);
        C_3DModel_Info* data = v.value<C_3DModel_Info*>();

        // Delete the Data Class
        if (data)
        {
            QString filePathName = data->m_Full_Path;
            // Remove any views of the file
            if (filePathName.length() > 0)
            {
                // Remove the item from the diff image drop down list
                int temp = m_ImagesinProjectTrees.indexOf(filePathName);
                if (temp != -1)
                    m_ImagesinProjectTrees.removeAt(temp);

                emit DeleteImageView(filePathName);
            }

            delete data;
            data = nullptr;
        }
        break;
    }
    case TREETYPE_3DSUBMODEL_DATA: {  // Project Explorer sub tree item
        // Get the Image Data linked to this node
        v                       = item->data(TREE_SourceInfo, Qt::UserRole);
        C_3DSubModel_Info* data = v.value<C_3DSubModel_Info*>();

        // Delete the Data Class
        if (data)
        {
            QString filePathName = data->m_Full_Path;
            // Remove any views of the file
            if (filePathName.length() > 0)
            {
                // Remove the item from the diff image drop down list
                int temp = m_ImagesinProjectTrees.indexOf(filePathName);
                if (temp != -1)
                    m_ImagesinProjectTrees.removeAt(temp);

                emit DeleteImageView(filePathName);
            }

            delete data;
            data = nullptr;
        }
        break;
    }
    case TREETYPE_VIEWIMAGE_ONLY_NODE:
    case TREETYPE_IMAGEFILE_DATA: {  // Project Explorer Root Item
        // Get the Image Data linked to this node
        QString filePathName = {};
        v                    = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Source_Info* data  = v.value<C_Source_Info*>();
        if (data)
        {
            filePathName = data->m_Full_Path;
        }

        // Remove the MIP Image data
        if (m_imageloader)
        {
            if (data)
                m_imageloader->clearMipImages(&data->m_MipImages);
        }

        // Remove any docked views of the file
        if (filePathName.length() > 0)
        {
            // Remove the item from the diff image drop down list
            int temp = m_ImagesinProjectTrees.indexOf(filePathName);
            if (temp != -1)
                m_ImagesinProjectTrees.removeAt(temp);

            emit DeleteImageView(filePathName);
        }

        // Delete the Data Class
        if (data)
        {
            delete data;
            data = nullptr;
        }
    }
    break;
    case TREETYPE_VIEWMESH_ONLY_NODE: {
        // Get the Image Data linked to this node
        QString filePathName     = {};
        v                        = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Mesh_Buffer_Info* data = v.value<C_Mesh_Buffer_Info*>();
        if (data)
        {
            filePathName = data->m_Full_Path;
        }

        // Remove any docked views of the file
        if (filePathName.length() > 0)
        {
            // Remove the item from the diff image drop down list
            int temp = m_ImagesinProjectTrees.indexOf(filePathName);
            if (temp != -1)
                m_ImagesinProjectTrees.removeAt(temp);

            emit DeleteImageView(filePathName);
        }

        // Delete the Data Class
        if (data)
        {
            delete data;
            data = nullptr;
        }
    }
    break;
    case TREETYPE_MESH_DATA: {  // Project Explorer sub tree item
        v                           = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Destination_Options* data = v.value<C_Destination_Options*>();
        if (data == NULL)
            return;
        QString filePathName = data->m_destFileNamePath;

        if (data)
        {
            // Check if image belongs to a Model parent
            QTreeWidgetItem* parent = item->parent();
            if (parent && userdeleted)
            {
                // Verify its root
                QVariant parentv         = parent->data(TREE_LevelType, Qt::UserRole);
                int      ParentlevelType = parentv.toInt();
                if (ParentlevelType == TREETYPE_3DSUBMODEL_DATA)
                {
                    QFileInfo replacementfileInfo(data->m_sourceFileNamePath);
                    QString   replacementfilename(replacementfileInfo.fileName());

                    // Find the sourceFileNamePath file in modelSource
                    // then replaces it with replacement file that has the same index pos in modelDest
                    parentv                       = parent->data(TREE_SourceInfo, Qt::UserRole);
                    C_3DSubModel_Info* parentdata = parentv.value<C_3DSubModel_Info*>();
                    if (parentdata->ModelType == eModelType::GLTF)
                        UpdateDestglTFWithFile(data->m_modelSource, data->m_modelDest, data->m_sourceFileNamePath, replacementfilename, userdeleted);
                }
            }

            delete data;
            data = nullptr;
        }

        // Remove any docked views of the file
        if (filePathName.length() > 0)
        {
            // Remove the item from the diff image drop down list
            int temp = m_ImagesinProjectTrees.indexOf(filePathName);
            if (temp != -1)
                m_ImagesinProjectTrees.removeAt(temp);

            emit DeleteImageView(filePathName);
        }
    }
    break;
    case TREETYPE_COMPRESSION_DATA: {  // Project Explorer sub tree item
        v                           = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Destination_Options* data = v.value<C_Destination_Options*>();
        if (data == NULL)
            return;
        QString filePathName = data->m_destFileNamePath;

        // Remove the MIP Image data
        if (m_imageloader)
            m_imageloader->clearMipImages(&data->m_MipImages);

        if (data)
        {
            // Check if image belongs to a Model parent
            QTreeWidgetItem* parent = item->parent();
            if (parent && userdeleted)
            {
                // Verify its root
                QVariant parentv         = parent->data(TREE_LevelType, Qt::UserRole);
                int      ParentlevelType = parentv.toInt();
                if (ParentlevelType == TREETYPE_3DSUBMODEL_DATA || ParentlevelType == TREETYPE_3DMODEL_DATA)
                {
                    QFileInfo replacementfileInfo(data->m_sourceFileNamePath);
                    QString   replacementfilename(replacementfileInfo.fileName());

                    // Find the sourceFileNamePath file in modelSource
                    // then replaces it with replacement file that has the same index pos in modelDest
                    parentv                       = parent->data(TREE_SourceInfo, Qt::UserRole);
                    C_3DSubModel_Info* parentdata = parentv.value<C_3DSubModel_Info*>();
                    if (parentdata->ModelType == eModelType::GLTF)
                        UpdateDestglTFWithFile(data->m_modelSource, data->m_modelDest, data->m_sourceFileNamePath, replacementfilename, userdeleted);
                }
            }

            delete data;
            data = nullptr;
        }

        // Remove any docked views of the file
        if (filePathName.length() > 0)
        {
            // Remove the item from the diff image drop down list
            int temp = m_ImagesinProjectTrees.indexOf(filePathName);
            if (temp != -1)
                m_ImagesinProjectTrees.removeAt(temp);

            emit DeleteImageView(filePathName);
        }
    }
    break;
    }

    m_saveProjectChanges = true;
}

// Return a valid image item

QTreeWidgetItem* ProjectView::GetCurrentItem(int matchType)
{
    // check if new items exit to process
    int topCount = m_projectTreeView->topLevelItemCount();
    if (topCount == 1)
        return NULL;

    //use tree
    QTreeWidgetItem* item = m_projectTreeView->currentItem();
    if (!item)
        return NULL;

    int itemType = GetItemType(item);

    if ((itemType != TREETYPE_Double_Click_here_to_add_files) && (itemType != TREETYPE_Add_destination_setting) &&
        (itemType != TREETYPE_Add_Model_destination_settings) && (itemType == matchType))
    {
        return (item);
    }

    if (m_CurrentItem)
    {
        int itemType = GetItemType(m_CurrentItem);

        if ((itemType != TREETYPE_Double_Click_here_to_add_files) && (itemType != TREETYPE_Add_destination_setting) &&
            (itemType != TREETYPE_Add_Model_destination_settings) && (itemType == matchType))
            return m_CurrentItem;
    }

    return (NULL);
}

QTreeWidgetItem* ProjectView::GetCurrentItem()
{
    // use known current
    if (m_CurrentItem)
    {
        return m_CurrentItem;
    }

    // else use tree
    QTreeWidgetItem* item = m_projectTreeView->currentItem();
    if (!item)
        return NULL;

    int itemType = GetItemType(item);

    if ((itemType != TREETYPE_Double_Click_here_to_add_files) && (itemType != TREETYPE_Add_destination_setting) &&
        (itemType != TREETYPE_Add_Model_destination_settings))
    {
        return (item);
    }
    return (NULL);
}

// Delete an Image Item from the project view
// User is prompted to confirm the delete

void ProjectView::UserDeleteItems()
{
    QMessageBox msgBox;
    msgBox.setText(trUtf8("Delete Selected Images"));
    msgBox.setInformativeText(
        "<br>Choose <b>Remove</b> to remove from project<br><br>Choose <b>Delete</b> to delete the image and its compressed images (if have any) "
        "from disk permanently <br>");
    QPushButton* myRemoveButton = msgBox.addButton(trUtf8("Remove"), QMessageBox::YesRole);
    QPushButton* myDeleteButton = msgBox.addButton(trUtf8("Delete"), QMessageBox::AcceptRole);
    QPushButton* myCancelButton = msgBox.addButton(trUtf8("Cancel"), QMessageBox::NoRole);
    msgBox.setDefaultButton(myCancelButton);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.exec();
    if (msgBox.clickedButton() == myRemoveButton)
    {
        DeleteAllSeletedItems(false);
    }
    else if (msgBox.clickedButton() == myDeleteButton)
    {
        DeleteAllSeletedItems(true);
    }
    emit UpdateData(NULL);
}

QTreeWidgetItem* ProjectView::DeleteSelectedItemData(QTreeWidgetItem* item, bool RemoveFromDisk)
{
    if (!item)
        return (NULL);

    QVariant v         = item->data(TREE_LevelType, Qt::UserRole);
    int      levelType = v.toInt();

    QString itemName;
    itemName = item->text(0);
    //printf("Delete [%d]: %s \n", levelType,itemName.toStdString().c_str());

    if (levelType == TREETYPE_IMAGEFILE_DATA || levelType == TREETYPE_3DSUBMODEL_DATA)
    {
        // Does this item have childern
        int childcount = item->childCount();
        if (childcount > 0)
        {
            // index starts from 0 to childcount-1
            childcount--;

            // Parse the child items that are of compress data types
            // in reverse order (start from botton of the three and work upwards
            // to clean the variant data items
            QTreeWidgetItem* child;
            int              ChildlevelType;

            // child 0 = Root node (+) all other indexes will be Compression settings
            while (childcount >= 0)
            {
                child = item->child(childcount);
                if (child)
                {
                    v              = child->data(TREE_LevelType, Qt::UserRole);
                    ChildlevelType = v.toInt();
                    // This should always be true: We are not flagging errors if false
                    if ((ChildlevelType == TREETYPE_COMPRESSION_DATA) || ((ChildlevelType == TREETYPE_MESH_DATA)))
                    {
                        v                           = child->data(TREE_SourceInfo, Qt::UserRole);
                        C_Destination_Options* data = v.value<C_Destination_Options*>();
                        if (data == NULL)
                        {
                            --childcount;
                            --m_NumItems;
                            continue;
                        }
                        // Remove the file
                        if (RemoveFromDisk)
                        {
                            bool isRemoved = QFile::remove(data->m_destFileNamePath);
                            if (!isRemoved)
                            {
                                if (QFile::exists(data->m_destFileNamePath))
                                {
                                    QString error = "Error: Delete " + data->m_destFileNamePath + " from disk failed. \n";
                                    PrintInfo(error.toStdString().c_str());
                                }
                            }
                        }

                        // Remove the item from the treeview
                        DeleteItemData(child, true);
                        m_NumItems--;
                    }
                    else
                    {
                        // Remove the (+) item node
                        DeleteItemData(child, true);
                        m_NumItems--;
                    }
                }
                childcount--;
            }
            m_NumItems--;
        }

        if (levelType == TREETYPE_3DSUBMODEL_DATA)
        {
            // Remove the gltf compressed file
            v                       = item->data(TREE_SourceInfo, Qt::UserRole);
            C_3DSubModel_Info* data = v.value<C_3DSubModel_Info*>();
            if (data)
            {
                if (RemoveFromDisk)
                {
                    bool isRemoved = QFile::remove(data->m_Full_Path);
                    if (!isRemoved)
                    {
                        if (QFile::exists(data->m_Full_Path))
                        {
                            QString error = "Error: Delete " + data->m_Full_Path + "from disk failed. \n";
                            PrintInfo(error.toStdString().c_str());
                        }
                    }
                }
            }
            m_NumItems--;
        }
    }
    else if (levelType == TREETYPE_3DMODEL_DATA)
    {
        // Does this item have childern
        int childcount = item->childCount();
        if (childcount > 0)
        {
            // index starts from 0 to childcound-1
            childcount--;

            // Parse the child items that are of compress data types
            // in reverse order (start from botton of the three and work upwards
            // to clean the variant data items
            QTreeWidgetItem* child;
            int              ChildlevelType;

            // child 0 = Root node (+) all other indexes will be Compression settings
            while (childcount >= 0)
            {
                child = item->child(childcount);
                if (child)
                {
                    v              = child->data(TREE_LevelType, Qt::UserRole);
                    ChildlevelType = v.toInt();
                    //printf("childlevel[%d]\n", ChildlevelType);
                    // This should always be true: We are not flagging errors if false
                    if (ChildlevelType == TREETYPE_3DSUBMODEL_DATA)
                    {
                        // Does this child item have childern
                        int grandchildcount = child->childCount();
                        if (grandchildcount > 0)
                        {
                            //printf("grandchild[%d]\n", grandchildcount);
                            // index starts from 0 to childcound-1
                            grandchildcount--;

                            // Parse the grandchild items that are of compress data types
                            // in reverse order (start from botton of the three and work upwards
                            // to clean the variant data items
                            QTreeWidgetItem* grandchild;
                            int              grandChildlevelType;

                            // child 0 = Root node (+) all other indexes will be Compression settings
                            while (grandchildcount >= 0)
                            {
                                grandchild = child->child(grandchildcount);
                                if (grandchild)
                                {
                                    v                   = grandchild->data(TREE_LevelType, Qt::UserRole);
                                    grandChildlevelType = v.toInt();
                                    // This should always be true: We are not flagging errors if false
                                    if ((grandChildlevelType == TREETYPE_COMPRESSION_DATA) || (grandChildlevelType == TREETYPE_MESH_DATA))
                                    {
                                        v                           = grandchild->data(TREE_SourceInfo, Qt::UserRole);
                                        C_Destination_Options* data = v.value<C_Destination_Options*>();
                                        // Remove the file
                                        if (data)
                                        {
                                            // remove subitem from treeview
                                            //printf("grandchilditem [%s]", data->m_destFileNamePath.toStdString().c_str());
                                            if (RemoveFromDisk)
                                            {
                                                bool isRemoved = QFile::remove(data->m_destFileNamePath);
                                                if (!isRemoved)
                                                {
                                                    if (QFile::exists(data->m_destFileNamePath))
                                                    {
                                                        QString error = "Error: Delete " + data->m_destFileNamePath + "from disk failed. \n";
                                                        PrintInfo(error.toStdString().c_str());
                                                    }
                                                }
                                            }
                                        }

                                        // Remove the item from the treeview
                                        DeleteItemData(grandchild, true);
                                        m_NumItems--;
                                    }
                                    else
                                    {
                                        // Remove the (+) item node
                                        DeleteItemData(grandchild, true);
                                        m_NumItems--;
                                    }
                                }
                                grandchildcount--;
                            }
                            m_NumItems--;
                        }

                        // Remove the gltf compressed (subtree item) file
                        v                       = child->data(TREE_SourceInfo, Qt::UserRole);
                        C_3DSubModel_Info* data = v.value<C_3DSubModel_Info*>();
                        if (data)
                        {
                            // remove subitem from treeview
                            //printf("Subitem [%s]\n", data->m_Full_Path.toStdString().c_str());
                            if (RemoveFromDisk)
                            {
                                bool isRemoved = QFile::remove(data->m_Full_Path);
                                if (!isRemoved)
                                {
                                    if (QFile::exists(data->m_Full_Path))
                                    {
                                        QString error = "Error: Delete " + data->m_Full_Path + "from disk failed. \n";
                                        PrintInfo(error.toStdString().c_str());
                                    }
                                }
                            }
                        }
                        DeleteItemData(child, true);
                        m_NumItems--;
                    }
                    else
                    {
                        // Remove the (+) item node
                        DeleteItemData(child, true);
                        m_NumItems--;
                    }
                }
                childcount--;
            }
            m_NumItems--;
        }
    }
    else if ((levelType == TREETYPE_COMPRESSION_DATA) || (levelType == TREETYPE_MESH_DATA))
    {
        // Remove the file
        v                           = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Destination_Options* data = v.value<C_Destination_Options*>();
        if (data)
        {
            if (RemoveFromDisk)
            {
                bool isRemoved = QFile::remove(data->m_destFileNamePath);
                if (!isRemoved)
                {
                    if (QFile::exists(data->m_destFileNamePath))
                    {
                        QString error = "Error: Delete " + data->m_destFileNamePath + "from disk failed. \n";
                        PrintInfo(error.toStdString().c_str());
                    }
                }
            }
        }

        // Find for parent which is 3D sub model or image level type to update the src file combo list
        QTreeWidgetItem* parent = item->parent();
        if (parent)
        {
            QVariant parentv         = parent->data(TREE_LevelType, Qt::UserRole);
            int      parentlevelType = parentv.toInt();
            if (parentlevelType == TREETYPE_IMAGEFILE_DATA)
            {
                parentv                       = parent->data(TREE_SourceInfo, Qt::UserRole);
                C_3DSubModel_Info* parentdata = parentv.value<C_3DSubModel_Info*>();
                if (data)
                {
                    for (int i = 0; i < data->m_Model_Images.size(); i++)
                    {
                        if (parentdata)
                        {
                            if (data->m_sourceFileNamePath == parentdata->m_Model_Images[i].m_FilePathName)
                            {
                                parentdata->m_SubModel_Images[i].m_srcDelFlag = false;
                            }
                        }
                    }
                }
            }
            else if (parentlevelType == TREETYPE_3DSUBMODEL_DATA)
            {
                parentv                       = parent->data(TREE_SourceInfo, Qt::UserRole);
                C_3DSubModel_Info* parentdata = parentv.value<C_3DSubModel_Info*>();
                if (data)
                {
                    for (int i = 0; i < data->m_Model_Images.size(); i++)
                    {
                        if (parentdata)
                        {
                            if (data->m_sourceFileNamePath == parentdata->m_Model_Images[i].m_FilePathName)
                            {
                                parentdata->m_SubModel_Images[i].m_srcDelFlag = false;
                                if (parentdata->ModelType == eModelType::GLTF)
                                    UpdateDestglTFWithFile(
                                        parentdata->m_ModelSource_gltf, data->m_modelDest, parentdata->m_Model_Images[i].m_FilePathName, "", false);
                            }
                        }
                    }
                }
            }
        }

        m_NumItems--;
    }
    else
    {
        item->setSelected(false);
    }
    // remove the node and its data for which the user selected the delete on
    int              i            = m_projectTreeView->indexOfTopLevelItem(item);
    QTreeWidgetItem* TopLevelitem = m_projectTreeView->takeTopLevelItem(i);
    if ((levelType != TREETYPE_Double_Click_here_to_add_files) && (levelType != TREETYPE_Add_destination_setting) &&
        (levelType != TREETYPE_Add_Model_destination_settings))
    {
        DeleteItemData(item, true);
        if (item)
        {
            delete item;
            item = NULL;
        }
    }

    m_saveProjectChanges = true;
    return TopLevelitem;
}

void ProjectView::DeleteAllSeletedItems(bool RemoveFromDisk)
{
    // Find the item and set it as selected
    QTreeWidgetItemIterator it(m_treeRootItem);
    QVariant                v;
    int                     levelType;
    int                     TotalItems = m_NumItems;

    // Use TotalItems as a bug check to prevent infinate loops!
    // The itterator should loop only once through all current items in the TreeView

    while ((*it) && (TotalItems > 0))
    {
        v         = (*it)->data(TREE_LevelType, Qt::UserRole);
        levelType = v.toInt();

        QString itemName;
        itemName = (*it)->text(0);
        //qDebug() << "Delete: " << itemName;

        if ((*it)->isSelected() && (levelType != TREETYPE_Double_Click_here_to_add_files) && (levelType != TREETYPE_VIEWIMAGE_ONLY_NODE))
        {
            DeleteSelectedItemData(*it, RemoveFromDisk);
        }
        else
            ++it;

        TotalItems--;
    }
}

QTreeWidgetItem* ProjectView::Tree_SetCurrentItem(QString FilePathName)
{
    QTreeWidgetItem* item = Tree_FindImageItem(FilePathName, true);
    if (item)
    {
        m_CurrentItem = item;
        m_projectTreeView->setCurrentItem(item);
        item->setSelected(true);
        if (m_EnableCheckedItemsView)
        {
            item->setCheckState(0, Qt::Checked);
        }
        return item;
    }

    m_CurrentItem = NULL;
    return NULL;
}

void ProjectView::saveToBatchFile()
{
    if (!AnySelectedItems())
        return;

    QFileInfo fileInfo(m_curProjectFilePathName);
    QString   suggestedName = fileInfo.completeBaseName();
    QString filePathName = QFileDialog::getSaveFileName(this, tr("Save selected images to batch file"), suggestedName, tr("Command Line Batch Files (*.bat)"));
    if (filePathName.length() > 0)
    {
        saveProjectFile();

        // Writing to a file
        QFile file(filePathName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            //qDebug() << "Open file for writing failed";
            return;
        }

        if (m_CompressStatusDialog)
        {
            m_CompressStatusDialog->onClearText();
        }

        compressProjectFiles(&file);
        file.close();

        if (m_AllItemsSelected)
        {
            // Reset the list of selections
            Tree_clearAllItemsSelected();
            m_AllItemsSelected = false;
        }
    }
}

void ProjectView::openContainingFolder()
{
    if (ContextMenu_ImageItem)
    {
        QVariant v         = ContextMenu_ImageItem->data(TREE_LevelType, Qt::UserRole);
        int      levelType = v.toInt();
        switch (levelType)
        {
        case TREETYPE_3DMODEL_DATA: {
            QVariant        fv     = ContextMenu_ImageItem->data(TREE_SourceInfo, Qt::UserRole);
            C_3DModel_Info* m_data = fv.value<C_3DModel_Info*>();
            if (m_data)
            {
                QDir      dir(QCoreApplication::applicationDirPath());
                QString   absolute_file_pathName = dir.absoluteFilePath(m_data->m_Full_Path);
                QFileInfo FilePath(absolute_file_pathName);
                QDesktopServices::openUrl(QUrl::fromLocalFile(FilePath.absolutePath()));
            }
            break;
        }
        case TREETYPE_3DSUBMODEL_DATA: {
            QVariant           fv     = ContextMenu_ImageItem->data(TREE_SourceInfo, Qt::UserRole);
            C_3DSubModel_Info* m_data = fv.value<C_3DSubModel_Info*>();
            if (m_data)
            {
                QDir      dir(QCoreApplication::applicationDirPath());
                QString   absolute_file_pathName = dir.absoluteFilePath(m_data->m_Full_Path);
                QFileInfo FilePath(absolute_file_pathName);
                QDesktopServices::openUrl(QUrl::fromLocalFile(FilePath.absolutePath()));
            }
            break;
        }
        case TREETYPE_IMAGEFILE_DATA: {  // Original Image item
            QVariant       fv     = ContextMenu_ImageItem->data(TREE_SourceInfo, Qt::UserRole);
            C_Source_Info* m_data = fv.value<C_Source_Info*>();
            if (m_data)
            {
                QDir      dir(QCoreApplication::applicationDirPath());
                QString   absolute_file_pathName = dir.absoluteFilePath(m_data->m_Full_Path);
                QFileInfo FilePath(absolute_file_pathName);
                QDesktopServices::openUrl(QUrl::fromLocalFile(FilePath.absolutePath()));
            }
            break;
        }
        case TREETYPE_MESH_DATA:
        case TREETYPE_COMPRESSION_DATA: {  // Compressed Image item
            QVariant               fv     = ContextMenu_ImageItem->data(TREE_SourceInfo, Qt::UserRole);
            C_Destination_Options* m_data = fv.value<C_Destination_Options*>();
            if (m_data)
            {
                QDir      dir(QCoreApplication::applicationDirPath());
                QString   absolute_file_pathName = dir.absoluteFilePath(m_data->m_destFileNamePath);
                QFileInfo FilePath(absolute_file_pathName);
                QDesktopServices::openUrl(QUrl::fromLocalFile(FilePath.absolutePath()));
            }
            break;
        }

        default:
            break;
        }
    }
}

void ProjectView::copyFullPath()
{
    if (ContextMenu_ImageItem)
    {
        QVariant v         = ContextMenu_ImageItem->data(0, Qt::UserRole);
        int      levelType = v.toInt();
        switch (levelType)
        {
        case TREETYPE_3DMODEL_DATA: {
            QVariant        fv     = ContextMenu_ImageItem->data(TREE_SourceInfo, Qt::UserRole);
            C_3DModel_Info* m_data = fv.value<C_3DModel_Info*>();
            if (m_data)
            {
                QClipboard* clipboard = QApplication::clipboard();
                if (clipboard)
                {
                    QDir    dir(QCoreApplication::applicationDirPath());
                    QString absolute_file_pathName = dir.absoluteFilePath(m_data->m_Full_Path);
                    // Use this if Path Only
                    QFileInfo FilePath(absolute_file_pathName);
                    QString   filePath = FilePath.absolutePath();
                    filePath.replace("/", "\\");
                    clipboard->setText(filePath);
                    // else use the following that contains the full path and File Name with extension
                    //clipboard->setText(absolute_file_pathName);
                }
            }
            break;
        }
        case TREETYPE_3DSUBMODEL_DATA: {
            QVariant           fv     = ContextMenu_ImageItem->data(TREE_SourceInfo, Qt::UserRole);
            C_3DSubModel_Info* m_data = fv.value<C_3DSubModel_Info*>();
            if (m_data)
            {
                QClipboard* clipboard = QApplication::clipboard();
                if (clipboard)
                {
                    QDir    dir(QCoreApplication::applicationDirPath());
                    QString absolute_file_pathName = dir.absoluteFilePath(m_data->m_Full_Path);
                    // Use this if Path Only
                    QFileInfo FilePath(absolute_file_pathName);
                    QString   filePath = FilePath.absolutePath();
                    filePath.replace("/", "\\");
                    clipboard->setText(filePath);
                    // else use the floowing that contains the full path and File Name with extension
                    //clipboard->setText(absolute_file_pathName);
                }
            }
            break;
        }
        case TREETYPE_IMAGEFILE_DATA: {  // Original Image item
            QVariant       fv     = ContextMenu_ImageItem->data(TREE_SourceInfo, Qt::UserRole);
            C_Source_Info* m_data = fv.value<C_Source_Info*>();
            if (m_data)
            {
                QClipboard* clipboard = QApplication::clipboard();
                if (clipboard)
                {
                    QDir    dir(QCoreApplication::applicationDirPath());
                    QString absolute_file_pathName = dir.absoluteFilePath(m_data->m_Full_Path);
                    // Use this if Path Only
                    QFileInfo FilePath(absolute_file_pathName);
                    QString   filePath = FilePath.absolutePath();
                    filePath.replace("/", "\\");
                    clipboard->setText(filePath);
                    // else use the floowing that contains the full path and File Name with extension
                    //clipboard->setText(absolute_file_pathName);
                }
            }
            break;
        }
        case TREETYPE_MESH_DATA:
        case TREETYPE_COMPRESSION_DATA: {  // Compressed Image item
            QVariant               fv     = ContextMenu_ImageItem->data(TREE_SourceInfo, Qt::UserRole);
            C_Destination_Options* m_data = fv.value<C_Destination_Options*>();
            if (m_data)
            {
                QClipboard* clipboard = QApplication::clipboard();
                if (clipboard)
                {
                    QDir    dir(QCoreApplication::applicationDirPath());
                    QString absolute_file_pathName = dir.absoluteFilePath(m_data->m_destFileNamePath);
                    // Use this if Path Only
                    QFileInfo FilePath(absolute_file_pathName);
                    QString   filePath = FilePath.absolutePath();
                    filePath.replace("/", "\\");
                    clipboard->setText(filePath);
                    // else use the floowing that contains the full path and File Name with extension
                    //clipboard->setText(absolute_file_pathName);
                }
            }
            break;
        }

        default:
            break;
        }
    }
}

void makeFormatExtCompatible(C_Destination_Options* C_Destination_Options)
{
#if (OPTION_BUILD_ASTC == 1)
    C_Destination_Options::eCompression_options format       = C_Destination_Options->m_Compression;
    QString                                     destfilePath = C_Destination_Options->m_destFileNamePath;
    QFileInfo                                   info(destfilePath);
    QString                                     newDestPath;
    if (format == C_Destination_Options::eCompression_options::ASTC && info.suffix() == "DDS")
    {
        newDestPath                                      = info.path() + "/" + info.completeBaseName() + ".KTX";
        C_Destination_Options->m_destFileNamePath        = newDestPath;
        C_Destination_Options->m_FileInfoDestinationName = C_Destination_Options->m_compname + ".KTX";
    }
    else if (format != C_Destination_Options::eCompression_options::ASTC && info.suffix() == "ASTC")
    {
        newDestPath                                      = info.path() + "/" + info.completeBaseName() + ".DDS";
        C_Destination_Options->m_destFileNamePath        = newDestPath;
        C_Destination_Options->m_FileInfoDestinationName = C_Destination_Options->m_compname + ".DDS";
    }
#endif
}

void ProjectView::saveProjectFile()
{
    // Writing to a file
    if (!(m_curProjectFilePathName.contains(".cprj", Qt::CaseInsensitive)))
        m_curProjectFilePathName.append(QString(".cprj"));

    QFile file(m_curProjectFilePathName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        // Projects opened from a read only location, such as the samples installed under
        // %ProgramFiles%, are redirected to the per user projects folder.
        QString appDataPath = QString::fromStdString(CMP_GetUserDataDir("projects"));
        QString redirected;
        if (!appDataPath.isEmpty())
        {
            // The redirect target is opened WriteOnly, which truncates. Two different read only
            // sample projects that share a base name would otherwise overwrite each other's
            // redirected copy, so pick the first free "<name>.cprj", "<name> (2).cprj", ... .
            redirected = freeProjectFilePath(QDir(appDataPath), QFileInfo(m_curProjectFilePathName).completeBaseName());
        }

        if (redirected.isEmpty() || redirected == m_curProjectFilePathName)
        {
            QMessageBox::warning(this, "Save Project", "Unable to save the project to " + m_curProjectFilePathName);
            return;
        }

        file.setFileName(redirected);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, "Save Project", "Unable to save the project to " + redirected);
            return;
        }

        m_curProjectFilePathName = redirected;
        m_curProjectName         = QFileInfo(redirected).completeBaseName();
        if (m_parent)
            m_parent->setWindowTitle(m_curProjectName);
    }

    QXmlStreamWriter xmlWriter(&file);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument();

    // <PROJECT>
    xmlWriter.writeStartElement("PROJECT");

    // Parse the Project view tree
    QTreeWidgetItemIterator it(m_projectTreeView);
    while (*it)
    {
        //=======
        // Node
        //=======
        QVariant v         = (*it)->data(TREE_LevelType, Qt::UserRole);
        int      levelType = v.toInt();

        if (levelType == TREETYPE_IMAGEFILE_DATA)
        {
            int            childcount = (*it)->childCount();
            QVariant       v          = (*it)->data(TREE_SourceInfo, Qt::UserRole);
            C_Source_Info* m_data     = v.value<C_Source_Info*>();
            if (m_data == NULL)
            {
                ++it;
                continue;
            }

            QString FilePathName;
            FilePathName = m_data->m_Full_Path;

            // <IMAGE FILE="">
            xmlWriter.writeStartElement("Image");
            xmlWriter.writeAttribute("File", FilePathName);
            if (childcount >= 1)
            {
                // now save the child elements
                for (int i = 0; i < childcount; i++)
                {
                    //=======
                    // Node
                    //=======
                    ++it;
                    if (*it)
                    {
                        QString  Setting      = (*it)->text(0);
                        QVariant v            = (*it)->data(TREE_LevelType, Qt::UserRole);
                        int      sublevelType = v.toInt();

                        // save the settings item
                        if ((sublevelType == TREETYPE_COMPRESSION_DATA) || (sublevelType == TREETYPE_MESH_DATA))
                        {
                            v                           = (*it)->data(TREE_SourceInfo, Qt::UserRole);
                            C_Destination_Options* data = v.value<C_Destination_Options*>();

                            // <Compression setting="">
                            if (sublevelType == TREETYPE_COMPRESSION_DATA)
                                xmlWriter.writeStartElement("Compression");
                            else
                                xmlWriter.writeStartElement("Compression_mesh");  // This should not be called check!!

                            xmlWriter.writeAttribute("Setting", Setting);
                            xmlWriter.writeAttribute("Enabled", (*it)->checkState(0) == Qt::Checked ? "True" : "False");

                            if (data)
                            {
                                makeFormatExtCompatible(data);

                                xmlWriter.writeTextElement("Source", data->m_sourceFileNamePath);
                                xmlWriter.writeTextElement("Destination", data->m_destFileNamePath);

                                if (sublevelType == TREETYPE_COMPRESSION_DATA)
                                {
                                    QMetaObject meta                = C_Destination_Options::staticMetaObject;
                                    int         indexCompression    = meta.indexOfEnumerator("eCompression_options");
                                    QMetaEnum   metaEnumCompression = meta.enumerator(indexCompression);
                                    const char* key                 = metaEnumCompression.valueToKey(data->m_Compression);
                                    xmlWriter.writeTextElement("fd", key);

                                    xmlWriter.writeTextElement("Quality", QString::number(data->m_Quality, 'g', 4));

                                    xmlWriter.writeTextElement("WeightR", QString::number(data->X_RED, 'g', 4));
                                    xmlWriter.writeTextElement("WeightG", QString::number(data->Y_GREEN, 'g', 4));
                                    xmlWriter.writeTextElement("WeightB", QString::number(data->Z_BLUE, 'g', 4));

                                    xmlWriter.writeTextElement("AlphaThreshold", QString::number(data->Threshold));
                                    xmlWriter.writeTextElement("RefineSteps", QString::number(data->Refine_Steps));
                                    xmlWriter.writeTextElement("BlockRate", data->m_Bitrate);
                                }
                                else
                                {
                                    // This should not be called check!!
                                    QMetaObject meta                = C_Destination_Options::staticMetaObject;
                                    int         indexCompression    = meta.indexOfEnumerator("eCompression_options");
                                    QMetaEnum   metaEnumCompression = meta.enumerator(indexCompression);
                                    const char* key                 = metaEnumCompression.valueToKey(data->m_Compression);
                                    xmlWriter.writeTextElement("fd", key);
                                }
                            }
                            // </Compression>
                            xmlWriter.writeEndElement();
                        }
                    }
                }
            }

            // </IMAGE>
            xmlWriter.writeEndElement();
        }
        else if (levelType == TREETYPE_3DMODEL_DATA)
        {
            QVariant        v      = (*it)->data(TREE_SourceInfo, Qt::UserRole);
            C_3DModel_Info* m_data = v.value<C_3DModel_Info*>();
            if (m_data == NULL)
            {
                ++it;
                continue;
            }

            QString FilePathName;
            FilePathName = m_data->m_Full_Path;

            // <Image>
            xmlWriter.writeStartElement("Image");
            xmlWriter.writeAttribute("File", FilePathName);

            int childcount = (*it)->childCount();
            if (childcount >= 1)
            {
                // now save the child elements
                for (int i = 0; i < childcount; i++)
                {
                    //=======
                    // Node
                    //=======

                    ++it;
                    if (*it)
                    {
                        QString  Setting      = (*it)->text(0);
                        QVariant v            = (*it)->data(TREE_LevelType, Qt::UserRole);
                        int      sublevelType = v.toInt();

                        // save the glTF detination settings ... node
                        if (sublevelType == TREETYPE_3DSUBMODEL_DATA)
                        {
                            v                       = (*it)->data(TREE_SourceInfo, Qt::UserRole);
                            C_3DSubModel_Info* data = v.value<C_3DSubModel_Info*>();

                            // <SubModel>
                            xmlWriter.writeStartElement("SubModel");
                            xmlWriter.writeAttribute("Setting", Setting);
                            xmlWriter.writeAttribute("Enabled", (*it)->checkState(0) == Qt::Checked ? "True" : "False");
                            if (data)
                            {
                                xmlWriter.writeTextElement("FileName", data->m_Full_Path);
                                for (int i = 0; i < data->m_Model_Images.size(); i++)
                                {
                                    xmlWriter.writeStartElement("DelFlags");
                                    xmlWriter.writeAttribute("TexName", data->m_Model_Images[i].m_FilePathName);
                                    if (data->m_SubModel_Images[i].m_srcDelFlag)
                                    {
                                        xmlWriter.writeAttribute("Deleted", "true");
                                    }
                                    else
                                        xmlWriter.writeAttribute("Deleted", "false");
                                    xmlWriter.writeEndElement();
                                }

                                int subchildcount = (*it)->childCount();
                                if (subchildcount >= 1)
                                {
                                    for (int i = 0; i < subchildcount; i++)
                                    {
                                        //=======
                                        // Node
                                        //=======
                                        ++it;
                                        if (*it)
                                        {
                                            QString  Setting      = (*it)->text(0);
                                            QVariant v            = (*it)->data(TREE_LevelType, Qt::UserRole);
                                            int      sublevelType = v.toInt();
                                            if ((sublevelType == TREETYPE_COMPRESSION_DATA) || (sublevelType == TREETYPE_MESH_DATA))
                                            {
                                                v                           = (*it)->data(TREE_SourceInfo, Qt::UserRole);
                                                C_Destination_Options* data = v.value<C_Destination_Options*>();

                                                // <Compression>
                                                if (sublevelType == TREETYPE_COMPRESSION_DATA)
                                                    xmlWriter.writeStartElement("Compression");
                                                else
                                                    xmlWriter.writeStartElement("Compression_mesh");

                                                xmlWriter.writeAttribute("Setting", Setting);
                                                xmlWriter.writeAttribute("Enabled", (*it)->checkState(0) == Qt::Checked ? "True" : "False");

                                                if (data)
                                                {
                                                    makeFormatExtCompatible(data);

                                                    xmlWriter.writeTextElement("Source", data->m_sourceFileNamePath);
                                                    xmlWriter.writeTextElement("Destination", data->m_destFileNamePath);

                                                    xmlWriter.writeTextElement("ThreeDSource", data->m_modelSource);
                                                    xmlWriter.writeTextElement("ThreeDDestination", data->m_modelDest);

                                                    if (sublevelType == TREETYPE_COMPRESSION_DATA)
                                                    {
                                                        QMetaObject meta                = C_Destination_Options::staticMetaObject;
                                                        int         indexCompression    = meta.indexOfEnumerator("eCompression_options");
                                                        QMetaEnum   metaEnumCompression = meta.enumerator(indexCompression);
                                                        const char* key                 = metaEnumCompression.valueToKey(data->m_Compression);
                                                        xmlWriter.writeTextElement("fd", key);

                                                        xmlWriter.writeTextElement("Quality", QString::number(data->m_Quality, 'g', 4));

                                                        xmlWriter.writeTextElement("WeightR", QString::number(data->X_RED, 'g', 4));
                                                        xmlWriter.writeTextElement("WeightG", QString::number(data->Y_GREEN, 'g', 4));
                                                        xmlWriter.writeTextElement("WeightB", QString::number(data->Z_BLUE, 'g', 4));

                                                        xmlWriter.writeTextElement("AlphaThreshold", QString::number(data->Threshold));
                                                        xmlWriter.writeTextElement("RefineSteps", QString::number(data->Refine_Steps));
                                                        xmlWriter.writeTextElement("BlockRate", data->m_Bitrate);
                                                    }
                                                    else
                                                    {
                                                        QMetaObject meta                = C_Destination_Options::staticMetaObject;
                                                        int         indexCompression    = meta.indexOfEnumerator("eCompression_options");
                                                        QMetaEnum   metaEnumCompression = meta.enumerator(indexCompression);
                                                        const char* key                 = metaEnumCompression.valueToKey(data->m_Compression);
                                                        xmlWriter.writeTextElement("fd", key);
                                                    }
                                                }
                                                // </Compression>
                                                xmlWriter.writeEndElement();
                                            }
                                        }
                                    }
                                }
                            }
                            // </SubModel>
                            xmlWriter.writeEndElement();
                        }
                    }
                }
            }

            // </Image>
            xmlWriter.writeEndElement();
        }

        // last item should be a vaild one unless we ended in a
        // child loop that was the last element in the tree if so break out were done..
        if (*it)
            ++it;
        else
            break;
    }

    // </PROJECT>
    xmlWriter.writeEndElement();
    file.close();

    m_saveProjectChanges = false;
}

void ProjectView::saveAsProjectFile()
{
    QString filePathName = QFileDialog::getSaveFileName(this, tr("Save Project"), m_curProjectFilePathName, tr("Compress Project files (*.cprj)"));
    if (filePathName.length() == 0)
        return;
    setCurrentProjectName(filePathName);
    saveProjectFile();
}

bool ProjectView::loadProjectFile(QString fileToLoad)
{
    // don't attempt to load a new project if there is currently some processing going on
    if (g_pProgressDlg)
    {
        if (!g_pProgressDlg->isHidden())
        {
            QMessageBox msgBox;
            msgBox.setText("Processing in progress, can't open new project.");
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.exec();

            return false;
        }
    }

    int numCompressedItems = 0;

    // Create a document to write XML
    QDomDocument document;
    // Open a file for reading
    QFile file(fileToLoad);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        //qDebug() << "Failed to open the file for reading.";
        return false;
    }
    else
    {
        // loading
        if (!document.setContent(&file))
        {
            //qDebug() << "Failed to load the file for reading.";
            return false;
        }
        file.close();
    }

    // Remove the old view
    clearProjectTreeView();

    // Getting root element
    QDomElement root = document.firstChildElement();

    // Loop Through Project Images
    QDomNodeList domImages = root.elementsByTagName("Image");

    // The number of Image nodes in the file <Image ...>
    int countImages = domImages.count();
    for (int i = 0; i < countImages; i++)
    {
        QDomNode domImage = domImages.at(i);
        if (domImage.isElement())
        {
            //======
            // Node
            //======
            // Get the File element  <Image File=".."/>
            QDomElement eleImage     = domImage.toElement();
            QString     FilePathName = resolveProjectPath(eleImage.attribute("File"));

            // Check if we have a file specified else igonore the node
            if (FilePathName.length() > 0)
            {
                // Add the file to our Project View
                C_Source_Info*   m_dataout = NULL;
                QTreeWidgetItem* Imageitem = Tree_AddImageFile(FilePathName, 0, &m_dataout);

                if (Imageitem)
                {
                    QVariant v         = Imageitem->data(TREE_LevelType, Qt::UserRole);
                    int      levelType = v.toInt();
                    if (levelType == TREETYPE_IMAGEFILE_DATA)
                    {
                        // Loop Image settings
                        QDomNodeList domCompressions   = eleImage.elementsByTagName("Compression");
                        int          countCompressions = domCompressions.count();
                        for (int i = 0; i < countCompressions; i++)
                        {
                            QDomNode domCompress = domCompressions.at(i);
                            if (domCompress.isElement())
                            {
                                numCompressedItems++;

                                QDomElement eleCompress = domCompress.toElement();
                                QString     Setting     = eleCompress.attribute("Setting");
                                QString     Enabled     = eleCompress.attribute("Enabled").toUpper();

                                // See also cpMainComponents - for new C_Destination_Options usage
                                C_Destination_Options* m_data = new C_Destination_Options();
                                if (m_data)
                                {
                                    m_data->m_compname                = Setting;
                                    m_data->m_FileInfoDestinationName = Setting;
                                    m_data->m_sourceFileNamePath      = FilePathName;

                                    if (m_dataout)
                                    {
                                        m_data->m_DstWidth        = m_dataout->m_Width;
                                        m_data->m_DstHeight       = m_dataout->m_Height;
                                        m_data->m_HeightStr       = QString().number(m_data->m_DstHeight) + " px";
                                        m_data->m_WidthStr        = QString().number(m_data->m_DstWidth) + " px";
                                        m_data->m_SourceImageSize = m_dataout->m_ImageSize;
                                    }

                                    QDomNode child = eleCompress.firstChild();
                                    while (!child.isNull())
                                    {
                                        if (child.toElement().tagName() == "Source")
                                        {
                                            QDomElement eleDestination   = child.toElement();
                                            m_data->m_sourceFileNamePath = resolveProjectPath(eleDestination.text());
                                        }
                                        else if (child.toElement().tagName() == "Destination")
                                        {
                                            QDomElement eleDestination = child.toElement();
                                            m_data->m_destFileNamePath = resolveProjectPath(eleDestination.text());

                                            // V4.2 change to just load the files as is. No check done until its used
                                            // QFileInfo fileInfo(m_data->m_destFileNamePath);
                                            // if (!fileInfo.isWritable())
                                            // {
                                            //     QFileInfo fileInfo2(m_curProjectFilePathName);
                                            //     m_data->m_destFileNamePath = fileInfo2.dir().path();
                                            //     m_data->m_destFileNamePath.append(QDir::separator());
                                            //     m_data->m_destFileNamePath.append(fileInfo.fileName());
                                            //     m_data->m_destFileNamePath.replace("/", "\\");
                                            //     //if current project file path is still not writable then change to app local path
                                            //     QFileInfo fileInfo3(m_data->m_destFileNamePath);
                                            //     if (!fileInfo3.isWritable())
                                            //     {
                                            //         m_data->m_destFileNamePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
                                            //         m_data->m_destFileNamePath.append(QDir::separator());
                                            //         m_data->m_destFileNamePath.append(fileInfo.fileName());
                                            //         m_data->m_destFileNamePath.replace("/", "\\");
                                            //     }
                                            // }
                                            // else
                                            //     m_data->m_FileInfoDestinationName = Setting + "." + fileInfo.suffix();
                                        }
                                        else if (child.toElement().tagName() == "fd")
                                        {
                                            QDomElement eleFD  = child.toElement();
                                            QString     format = eleFD.text();
                                            QMetaObject meta   = C_Destination_Options::staticMetaObject;

                                            int       indexCompression    = meta.indexOfEnumerator("eCompression_options");
                                            QMetaEnum metaEnumCompression = meta.enumerator(indexCompression);
                                            m_data->m_Compression =
                                                (C_Destination_Options::eCompression_options)metaEnumCompression.keysToValue(format.toLatin1().data());
                                        }
                                        else if (child.toElement().tagName() == "Quality")
                                        {
                                            QDomElement eleFD   = child.toElement();
                                            QString     Quality = eleFD.text();
                                            bool        ok;
                                            m_data->m_Quality = Quality.toFloat(&ok);
                                            if (!ok)
                                                m_data->m_Quality = AMD_CODEC_QUALITY_DEFAULT;
                                        }
                                        else if (child.toElement().tagName() == "WeightR")
                                        {
                                            QDomElement eleFD   = child.toElement();
                                            QString     WeightR = eleFD.text();
                                            bool        ok;
                                            m_data->X_RED = WeightR.toFloat(&ok);
                                            if (!ok)
                                                m_data->X_RED = 0.3086;
                                        }
                                        else if (child.toElement().tagName() == "WeightG")
                                        {
                                            QDomElement eleFD   = child.toElement();
                                            QString     WeightG = eleFD.text();
                                            bool        ok;
                                            m_data->Y_GREEN = WeightG.toFloat(&ok);
                                            if (!ok)
                                                m_data->Y_GREEN = 0.6094;
                                        }
                                        else if (child.toElement().tagName() == "WeightB")
                                        {
                                            QDomElement eleFD   = child.toElement();
                                            QString     WeightB = eleFD.text();
                                            bool        ok;
                                            m_data->Z_BLUE = WeightB.toFloat(&ok);
                                            if (!ok)
                                                m_data->Z_BLUE = 0.0820;
                                        }
                                        else if (child.toElement().tagName() == "AlphaThreshold")
                                        {
                                            QDomElement eleFD          = child.toElement();
                                            QString     AlphaThreshold = eleFD.text();
                                            bool        ok;
                                            m_data->Threshold = AlphaThreshold.toInt(&ok);
                                            if (!ok)
                                                m_data->Threshold = 0;
                                        }
                                        else if (child.toElement().tagName() == "RefineSteps")
                                        {
                                            QDomElement eleFD = child.toElement();
                                            QString     value = eleFD.text();
                                            bool        ok;
                                            m_data->Refine_Steps = value.toInt(&ok);
                                            if (!ok)
                                                m_data->Refine_Steps = 0;
                                        }
                                        else if (child.toElement().tagName() == "BlockRate")
                                        {
                                            QDomElement eleFD     = child.toElement();
                                            QString     BlockRate = eleFD.text();
                                            m_data->m_Bitrate     = BlockRate;
                                            if (BlockRate.length() < 2)  // Default back to 4x4!
                                                m_data->m_Bitrate = "8.00";
                                        }
                                        child = child.nextSibling();
                                    }
                                    //======
                                    // Node
                                    //======
                                    // Always add from the TREETYPE_IMAGE_SETTING_NODE STR_AddDestinationSetting
                                    Tree_AddCompressFile(Imageitem->child(0), Setting, true, Enabled.contains("TRUE"), TREETYPE_COMPRESSION_DATA, m_data);
                                }
                            }
                        }
                    }
                    else if (levelType == TREETYPE_3DMODEL_DATA)
                    {
                        // Loop Image settings
                        QDomNodeList domSubModel = eleImage.elementsByTagName("SubModel");
                        int          countFiles  = domSubModel.count();
                        for (int i = 0; i < countFiles; i++)
                        {
                            QDomNode dom3DFile = domSubModel.at(i);
                            if (dom3DFile.isElement())
                            {
                                QDomElement ele3Dest = dom3DFile.toElement();
                                QDomNode    child    = ele3Dest.firstChild();
                                while (!child.isNull())
                                {
                                    if (child.toElement().tagName() == "FileName")
                                    {
                                        QDomElement  eleDestination = child.toElement();
                                        QDomNodeList domDelFlags    = ele3Dest.elementsByTagName("DelFlags");
                                        int          countDel       = domDelFlags.count();
                                        QList<bool>  loadedDelFlags;
                                        for (int i = 0; i < countDel; i++)
                                        {
                                            QDomNode domDel = domDelFlags.at(i);
                                            if (domDel.isElement())
                                            {
                                                QString delFlag = domDel.toElement().attribute("Deleted");
                                                if (delFlag == "true")
                                                    loadedDelFlags.append(true);
                                                else
                                                    loadedDelFlags.append(false);
                                            }
                                        }

                                        //======
                                        // Node
                                        //======
                                        Tree_Add3DSubModelFile(Imageitem, eleDestination.text(), &loadedDelFlags);
                                        QString name1 = Imageitem->text(0);

                                        QTreeWidgetItem* ChildImageitem;
                                        if (Imageitem->childCount() > 0)
                                            ChildImageitem = Imageitem->child(Imageitem->childCount() - 1);
                                        else
                                            ChildImageitem = Imageitem->child(0);

                                        QString name = ChildImageitem->text(0);

                                        // Loop Compression Mesh
                                        QDomNodeList domCompressions_mesh   = ele3Dest.elementsByTagName("Compression_mesh");
                                        int          countCompressions_mesh = domCompressions_mesh.count();
                                        for (int i = 0; i < countCompressions_mesh; i++)
                                        {
                                            QDomNode domCompress = domCompressions_mesh.at(i);
                                            if (domCompress.isElement())
                                            {
                                                numCompressedItems++;

                                                QDomElement eleCompress = domCompress.toElement();
                                                QString     Setting     = eleCompress.attribute("Setting");
                                                QString     Enabled     = eleCompress.attribute("Enabled").toUpper();

                                                // See also cpMainComponents - for new C_Destination_Options usage
                                                C_Destination_Options* m_data = new C_Destination_Options();
                                                if (m_data)
                                                {
                                                    m_data->m_isModelData             = true;
                                                    m_data->m_compname                = Setting;
                                                    m_data->m_FileInfoDestinationName = Setting;
                                                    m_data->m_sourceFileNamePath      = FilePathName;

                                                    if (m_dataout)
                                                    {
                                                        m_data->m_DstWidth        = m_dataout->m_Width;
                                                        m_data->m_DstHeight       = m_dataout->m_Height;
                                                        m_data->m_HeightStr       = QString().number(m_data->m_DstHeight) + " px";
                                                        m_data->m_WidthStr        = QString().number(m_data->m_DstWidth) + " px";
                                                        m_data->m_SourceImageSize = m_dataout->m_ImageSize;
                                                    }

                                                    QDomNode child = eleCompress.firstChild();
                                                    while (!child.isNull())
                                                    {
                                                        if (child.toElement().tagName() == "Source")
                                                        {
                                                            QDomElement eleDestination   = child.toElement();
                                                            m_data->m_sourceFileNamePath = eleDestination.text();
                                                        }
                                                        else if (child.toElement().tagName() == "Destination")
                                                        {
                                                            QDomElement eleDestination = child.toElement();
                                                            m_data->m_destFileNamePath = eleDestination.text();

                                                            QFileInfo fileInfo(m_data->m_destFileNamePath);
                                                            if (!fileInfo.isWritable())
                                                            {
                                                                QFileInfo fileInfo2(m_curProjectFilePathName);
                                                                m_data->m_destFileNamePath = fileInfo2.dir().path();
                                                                m_data->m_destFileNamePath.append(QDir::separator());
                                                                m_data->m_destFileNamePath.append(fileInfo.fileName());
                                                                m_data->m_destFileNamePath.replace("/", "\\");
                                                            }
                                                            else
                                                                m_data->m_FileInfoDestinationName = Setting + "." + fileInfo.suffix();
                                                        }
                                                        else if (child.toElement().tagName() == "ThreeDSource")
                                                        {
                                                            QDomElement eleDestination = child.toElement();
                                                            m_data->m_modelSource      = eleDestination.text();
                                                        }
                                                        else if (child.toElement().tagName() == "ThreeDDestination")
                                                        {
                                                            QDomElement eleDestination = child.toElement();
                                                            m_data->m_modelDest        = eleDestination.text();
                                                        }
                                                        else if (child.toElement().tagName() == "fd")
                                                        {
                                                            // This is inavid for Compression_mesh type and should be ignored!
                                                            QDomElement eleFD  = child.toElement();
                                                            QString     format = eleFD.text();
                                                            QMetaObject meta   = C_Destination_Options::staticMetaObject;

                                                            int       indexCompression    = meta.indexOfEnumerator("eCompression_options");
                                                            QMetaEnum metaEnumCompression = meta.enumerator(indexCompression);
                                                            m_data->m_Compression =
                                                                (C_Destination_Options::eCompression_options)metaEnumCompression.keysToValue(
                                                                    format.toLatin1().data());
                                                        }
                                                        child = child.nextSibling();
                                                    }
                                                    //======
                                                    // Node
                                                    //======
                                                    // Always add from the TREETYPE_IMAGE_SETTING_NODE STR_AddDestinationSetting
                                                    Tree_AddCompressFile(
                                                        ChildImageitem->child(0), Setting, true, Enabled.contains("TRUE"), TREETYPE_MESH_DATA, m_data);
                                                }
                                            }
                                        }

                                        // Loop Compression Image Settings
                                        QDomNodeList domCompressions   = ele3Dest.elementsByTagName("Compression");
                                        int          countCompressions = domCompressions.count();
                                        for (int i = 0; i < countCompressions; i++)
                                        {
                                            QDomNode domCompress = domCompressions.at(i);
                                            if (domCompress.isElement())
                                            {
                                                numCompressedItems++;

                                                QDomElement eleCompress = domCompress.toElement();
                                                QString     Setting     = eleCompress.attribute("Setting");
                                                QString     Enabled     = eleCompress.attribute("Enabled").toUpper();

                                                // See also cpMainComponents - for new C_Destination_Options usage
                                                C_Destination_Options* m_data = new C_Destination_Options();
                                                if (m_data)
                                                {
                                                    m_data->m_isModelData             = false;
                                                    m_data->m_compname                = Setting;
                                                    m_data->m_FileInfoDestinationName = Setting;
                                                    m_data->m_sourceFileNamePath      = FilePathName;

                                                    if (m_dataout)
                                                    {
                                                        m_data->m_DstWidth        = m_dataout->m_Width;
                                                        m_data->m_DstHeight       = m_dataout->m_Height;
                                                        m_data->m_HeightStr       = QString().number(m_data->m_DstHeight) + " px";
                                                        m_data->m_WidthStr        = QString().number(m_data->m_DstWidth) + " px";
                                                        m_data->m_SourceImageSize = m_dataout->m_ImageSize;
                                                    }

                                                    QDomNode child = eleCompress.firstChild();
                                                    while (!child.isNull())
                                                    {
                                                        if (child.toElement().tagName() == "Source")
                                                        {
                                                            QDomElement eleDestination   = child.toElement();
                                                            m_data->m_sourceFileNamePath = eleDestination.text();
                                                        }
                                                        else if (child.toElement().tagName() == "Destination")
                                                        {
                                                            QDomElement eleDestination = child.toElement();
                                                            m_data->m_destFileNamePath = eleDestination.text();

                                                            QFileInfo fileInfo(m_data->m_destFileNamePath);
                                                            if (!fileInfo.isWritable())
                                                            {
                                                                QFileInfo fileInfo2(m_curProjectFilePathName);
                                                                m_data->m_destFileNamePath = fileInfo2.dir().path();
                                                                m_data->m_destFileNamePath.append(QDir::separator());
                                                                m_data->m_destFileNamePath.append(fileInfo.fileName());
                                                                m_data->m_destFileNamePath.replace("/", "\\");
                                                            }
                                                            else
                                                                m_data->m_FileInfoDestinationName = Setting + "." + fileInfo.suffix();
                                                        }
                                                        else if (child.toElement().tagName() == "ThreeDSource")
                                                        {
                                                            QDomElement eleDestination = child.toElement();
                                                            m_data->m_modelSource      = eleDestination.text();
                                                        }
                                                        else if (child.toElement().tagName() == "ThreeDDestination")
                                                        {
                                                            QDomElement eleDestination = child.toElement();
                                                            m_data->m_modelDest        = eleDestination.text();
                                                        }
                                                        else if (child.toElement().tagName() == "fd")
                                                        {
                                                            QDomElement eleFD  = child.toElement();
                                                            QString     format = eleFD.text();
                                                            QMetaObject meta   = C_Destination_Options::staticMetaObject;

                                                            int       indexCompression    = meta.indexOfEnumerator("eCompression_options");
                                                            QMetaEnum metaEnumCompression = meta.enumerator(indexCompression);
                                                            m_data->m_Compression =
                                                                (C_Destination_Options::eCompression_options)metaEnumCompression.keysToValue(
                                                                    format.toLatin1().data());
                                                        }
                                                        else if (child.toElement().tagName() == "Quality")
                                                        {
                                                            QDomElement eleFD   = child.toElement();
                                                            QString     Quality = eleFD.text();
                                                            bool        ok;
                                                            m_data->m_Quality = Quality.toFloat(&ok);
                                                            if (!ok)
                                                                m_data->m_Quality = AMD_CODEC_QUALITY_DEFAULT;
                                                        }
                                                        else if (child.toElement().tagName() == "WeightR")
                                                        {
                                                            QDomElement eleFD   = child.toElement();
                                                            QString     WeightR = eleFD.text();
                                                            bool        ok;
                                                            m_data->X_RED = WeightR.toFloat(&ok);
                                                            if (!ok)
                                                                m_data->X_RED = 0.3086;
                                                        }
                                                        else if (child.toElement().tagName() == "WeightG")
                                                        {
                                                            QDomElement eleFD   = child.toElement();
                                                            QString     WeightG = eleFD.text();
                                                            bool        ok;
                                                            m_data->Y_GREEN = WeightG.toFloat(&ok);
                                                            if (!ok)
                                                                m_data->Y_GREEN = 0.6094;
                                                        }
                                                        else if (child.toElement().tagName() == "WeightB")
                                                        {
                                                            QDomElement eleFD   = child.toElement();
                                                            QString     WeightB = eleFD.text();
                                                            bool        ok;
                                                            m_data->Z_BLUE = WeightB.toFloat(&ok);
                                                            if (!ok)
                                                                m_data->Z_BLUE = 0.0820;
                                                        }
                                                        else if (child.toElement().tagName() == "AlphaThreshold")
                                                        {
                                                            QDomElement eleFD          = child.toElement();
                                                            QString     AlphaThreshold = eleFD.text();
                                                            bool        ok;
                                                            m_data->Threshold = AlphaThreshold.toInt(&ok);
                                                            if (!ok)
                                                                m_data->Threshold = 0;
                                                        }
                                                        else if (child.toElement().tagName() == "RefineSteps")
                                                        {
                                                            QDomElement eleFD = child.toElement();
                                                            QString     value = eleFD.text();
                                                            bool        ok;
                                                            m_data->Refine_Steps = value.toInt(&ok);
                                                            if (!ok)
                                                                m_data->Refine_Steps = 0;
                                                        }
                                                        else if (child.toElement().tagName() == "BlockRate")
                                                        {
                                                            QDomElement eleFD     = child.toElement();
                                                            QString     BlockRate = eleFD.text();
                                                            m_data->m_Bitrate     = BlockRate;
                                                            if (BlockRate.length() < 2)  // Default back to 4x4!
                                                                m_data->m_Bitrate = "8.00";
                                                        }
                                                        child = child.nextSibling();
                                                    }
                                                    //======
                                                    // Node
                                                    //======
                                                    // Always add from the TREETYPE_IMAGE_SETTING_NODE STR_AddDestinationSetting
                                                    Tree_AddCompressFile(
                                                        ChildImageitem->child(0), Setting, true, Enabled.contains("TRUE"), TREETYPE_COMPRESSION_DATA, m_data);
                                                }
                                            }
                                        }
                                    }
                                    child = child.nextSibling();
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    setCurrentProjectName(fileToLoad);
    m_saveProjectChanges = false;

    emit OnProjectLoaded(numCompressedItems);

    return true;
}

void ProjectView::openProjectFile()
{
    if (!userSaveProjectAndContinue())
        return;

    QString filename = QFileDialog::getOpenFileName(this, tr("Open project files"), m_curProjectFilePathName, tr("Project files (*.cprj)"));
    if (filename.length() > 0)
        loadProjectFile(filename);
}

void ProjectView::removeSelectedImage()
{
    if (!g_bCompressing)
        UserDeleteItems();
}

// Prompt the user to save prior project and continue or
// cancel the current task

bool ProjectView::userSaveProjectAndContinue()
{
    // Check if user wants t osave prior changes
    if (m_saveProjectChanges)
    {
        int action = PromptSaveChanges();
        switch (action)
        {
        case QMessageBox::AcceptRole:
            saveAsProjectFile();
            break;
        case QMessageBox::Cancel:
            return false;
            break;
        }
        m_saveProjectChanges = false;
    }
    return true;
}

void ProjectView::openNewProjectFile()
{
    // Check if user wants to save prior changes
    if (!userSaveProjectAndContinue())
        return;
    m_newProjectwindow->GetNewFilePathName(m_curProjectFilePathName);
    emit OnProjectLoaded(0);
}

void ProjectView::onSetNewProject(QString& FilePathName)
{
    // Remove the old view
    clearProjectTreeView();
    // setCurrentProjectName redirects to a writable folder when FilePathName is not writable
    setCurrentProjectName(FilePathName);
    m_saveProjectChanges = false;
    if (m_CompressStatusDialog)
        m_CompressStatusDialog->onClearText();
    m_CurrentCompressedImageItem = NULL;
    emit UpdateData(NULL);
}

bool ProgressCallback(float fProgress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
{
    // Keep Qt responsive
    QApplication::processEvents();

    // Process the CmdLine Messages

    if (g_fProgress != fProgress)
    {
        UNREFERENCED_PARAMETER(pUser1);
        UNREFERENCED_PARAMETER(pUser2);

        if ((g_CmdPrams.noprogressinfo) || (g_CmdPrams.silent))
            return g_bAbortCompression;

        emit static_processmsghandler.signalProcessMessage();
        g_fProgress = fProgress;
    }

    return g_bAbortCompression;
}

void ProjectView::onSignalProcessMessage()
{
    if (g_pProgressDlg)
    {
        g_pProgressDlg->SetValue(g_fProgress);
    }
}

void ProjectView::AddDefaultDestToItems(const std::vector<QTreeWidgetItem*>& items)
{
    assert(m_parent);
    if (!m_parent || items.size() == 0)
        return;

    CSetCompressOptions* compressOptions = ((cpMainComponents*)m_parent)->m_setcompressoptions;
    assert(compressOptions);

    // first we set up the compress settings dialog box for the first item
    {
        compressOptions->m_DestinationData.init();

        QVariant       v         = items[0]->data(TREE_SourceInfo, Qt::UserRole);
        C_Source_Info* imageFile = v.value<C_Source_Info*>();
        QFileInfo      fileinfo(imageFile->m_Name);

        compressOptions->m_CBSourceFile->clear();
        compressOptions->m_CBSourceFile->addItem(fileinfo.fileName());
        compressOptions->m_DestinationData.m_compname                 = fileinfo.baseName();
        compressOptions->m_DestinationData.m_sourceFileNamePath       = imageFile->m_Full_Path;
        compressOptions->m_DestinationData.m_SourceImageSize          = imageFile->m_ImageSize;
        compressOptions->m_DestinationData.m_SourceIscompressedFormat = CMP_IsCompressedFormat(imageFile->m_Format);
        compressOptions->m_DestinationData.m_SourceIsFloatFormat      = CMP_IsFloatFormat(imageFile->m_Format);
        compressOptions->m_item                                       = items[0];
        compressOptions->m_automaticProcessing                        = true;
    }

    // show the dialog box
    // this could be wrapped by "if (!m_processFromContext)", but I'm not sure if it is useful
    if (compressOptions->updateDisplayContent())
    {
        if (!compressOptions->isVisible())
        {
            QPoint pos = QCursor::pos();
            compressOptions->move(pos);
            compressOptions->m_LEName->setEnabled(false);
            compressOptions->setWindowFlags(Qt::Dialog);
            compressOptions->exec();
            compressOptions->m_LEName->setEnabled(true);
        }
    }

    if (!compressOptions->m_automaticProcessing)
    {
        g_bAbortCompression = true;
        return;
    }

    g_bAbortCompression = false;

    // using the dialog box settings, set up all of the items
    for (auto& item : items)
    {
        QVariant       v         = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Source_Info* imageFile = v.value<C_Source_Info*>();
        QFileInfo      fileInfo(imageFile->m_Name);
        QString        srcExt = fileInfo.suffix().toUpper();

        compressOptions->m_CBSourceFile->clear();
        compressOptions->m_CBSourceFile->addItem(fileInfo.fileName());
        compressOptions->m_DestinationData.m_sourceFileNamePath       = imageFile->m_Full_Path;
        compressOptions->m_DestinationData.m_SourceImageSize          = imageFile->m_ImageSize;
        compressOptions->m_DestinationData.m_SourceIscompressedFormat = CMP_IsCompressedFormat(imageFile->m_Format);
        compressOptions->m_DestinationData.m_SourceIsFloatFormat      = CMP_IsFloatFormat(imageFile->m_Format);

        int childCount = item->childCount();
        if (imageFile->m_extnum <= childCount)
            imageFile->m_extnum = childCount;

        compressOptions->m_extnum                      = imageFile->m_extnum++;
        compressOptions->m_DestinationData.m_DstWidth  = imageFile->m_Width;
        compressOptions->m_DestinationData.m_DstHeight = imageFile->m_Height;

        compressOptions->m_DestinationData.m_compname = fileInfo.baseName();

        if (!g_Application_Options.m_useOriginalFileNames)
            compressOptions->m_DestinationData.m_compname += "_" + srcExt;

        compressOptions->m_DestinationData.m_editing = false;
        compressOptions->m_item                      = item;
        compressOptions->SaveCompressedInfo();
    }

    compressOptions->m_automaticProcessing = false;
}

void ProjectView::AddSettingtoEmptyTree()
{
    std::vector<QTreeWidgetItem*> items;

    QTreeWidgetItemIterator it(m_projectTreeView);
    while (*it)
    {
        QTreeWidgetItem* item       = *it;
        int              itemType   = GetItemType(item);
        int              childCount = item->childCount();

        if (itemType == TREETYPE_IMAGEFILE_DATA && childCount == 1)
        {
            items.push_back(item);
        }

        ++it;
    }

    AddDefaultDestToItems(items);
}

void ProjectView::analyseMeshData()
{
    if (ContextMenu_ImageItem)
    {
        QVariant v         = ContextMenu_ImageItem->data(TREE_LevelType, Qt::UserRole);
        int      levelType = v.toInt();
        switch (levelType)
        {
        case TREETYPE_3DMODEL_DATA: {
            QVariant        fv     = ContextMenu_ImageItem->data(TREE_SourceInfo, Qt::UserRole);
            C_3DModel_Info* m_data = fv.value<C_3DModel_Info*>();
            if (m_data)
            {
                QFileInfo                       fileInfo(m_data->m_Full_Path);
                QString                         EXT   = fileInfo.suffix();
                QByteArray                      ba    = EXT.toUpper().toLatin1();
                char*                           c_ext = ba.data();
                PluginInterface_3DModel_Loader* m_plugin_loader;
                m_plugin_loader = reinterpret_cast<PluginInterface_3DModel_Loader*>(g_pluginManager.GetPlugin("3DMODEL_LOADER", c_ext));

                void* msgHandler = NULL;

                cpMainComponents* mainComponents = NULL;
                mainComponents                   = (cpMainComponents*)m_parent;
                if (mainComponents)
                    msgHandler = (void*)mainComponents->PrintStatus;

                if (m_plugin_loader)
                {
                    m_plugin_loader->TC_PluginSetSharedIO(g_GUI_CMIPS);

                    int         result;
                    std::string filename = m_data->m_Full_Path.toStdString();

                    if (strcmp(c_ext, "GLTF") == 0)
                    {
                        if (isGLTFDracoFile(filename))
                        {
                            std::size_t dotPos      = filename.rfind('.');
                            std::string tempdstFile = filename.substr(0, dotPos) + "_tmpdecoded.glTF";
                            if (!decompressglTFfile(filename, tempdstFile, g_CmdPrams.use_Draco_Encode, g_CmdPrams.CompressOptions))
                            {
                                PrintInfo("Error: Decoding glTF file [%s] failed.\n", filename.c_str());
                                return;
                            }

                            filename = tempdstFile;
                        }
                    }

                    if (result = m_plugin_loader->LoadModelData(filename.c_str(), "", &g_pluginManager, msgHandler, &ProgressCallback) != 0)
                    {
                        if (result != 0)
                        {
                            if (m_CompressStatusDialog)
                            {
                                m_CompressStatusDialog->onClearText();
                                m_CompressStatusDialog->showOutput();
                            }
                            PrintInfo("Error Loading Model Data");
                            return;
                        }
                    }

                    if (strcmp(c_ext, "GLTF") == 0)
                    {
                        m_data->m_ModelData             = (*(CMP_GLTFCommon*)m_plugin_loader->GetModelData()).m_meshBufferData;
                        m_3DMeshAnalysisDlg->m_fileName = m_data->m_Full_Path;
                    }
                    else
                    {
                        m_data->m_ModelData             = (*(CMODEL_DATA*)m_plugin_loader->GetModelData());
                        m_3DMeshAnalysisDlg->m_fileName = QString::fromStdString(m_data->m_ModelData.m_model_name);
                    }
                }
                hideProgressDialog();
                run3DMeshAnalysis(&(m_data->m_ModelData), NULL);
                if (m_plugin_loader)
                {
                    delete m_plugin_loader;
                    m_plugin_loader = nullptr;
                }
            }
            break;
        }
        case TREETYPE_3DSUBMODEL_DATA: {
            QVariant           fv     = ContextMenu_ImageItem->data(TREE_SourceInfo, Qt::UserRole);
            C_3DSubModel_Info* m_data = fv.value<C_3DSubModel_Info*>();
            if (m_data)
            {
                QFileInfo                       fileInfo(m_data->m_Full_Path);
                QString                         EXT   = fileInfo.suffix();
                QByteArray                      ba    = EXT.toUpper().toLatin1();
                char*                           c_ext = ba.data();
                PluginInterface_3DModel_Loader* m_subplugin_loader;
                m_subplugin_loader = reinterpret_cast<PluginInterface_3DModel_Loader*>(g_pluginManager.GetPlugin("3DMODEL_LOADER", c_ext));

                void* msgHandler = NULL;

                cpMainComponents* mainComponents = NULL;
                mainComponents                   = (cpMainComponents*)m_parent;
                if (mainComponents)
                    msgHandler = (void*)mainComponents->PrintStatus;

                if (m_subplugin_loader)
                {
                    m_subplugin_loader->TC_PluginSetSharedIO(g_GUI_CMIPS);
                    std::string filename = m_data->m_Full_Path.toStdString();

                    if (strcmp(c_ext, "GLTF") == 0)
                    {
                        if (isGLTFDracoFile(filename))
                        {
                            std::size_t dotPos      = filename.rfind('.');
                            std::string tempdstFile = filename.substr(0, dotPos) + "_tmpdecoded.glTF";
                            if (!decompressglTFfile(filename, tempdstFile, g_CmdPrams.use_Draco_Encode, g_CmdPrams.CompressOptions))
                            {
                                PrintInfo("Error: Decoding glTF file [%s] failed.\n", filename.c_str());
                                return;
                            }

                            filename = tempdstFile;
                        }
                    }

                    int result;
                    if (result = m_subplugin_loader->LoadModelData(filename.c_str(), "", &g_pluginManager, msgHandler, &ProgressCallback) != 0)
                    {
                        if (result != 0)
                        {
                            if (m_CompressStatusDialog)
                            {
                                m_CompressStatusDialog->onClearText();
                                m_CompressStatusDialog->showOutput();
                            }
                            PrintInfo("Error Loading Model Data");
                            return;
                        }
                    }

                    if (strcmp(c_ext, "GLTF") == 0)
                    {
                        m_data->m_ModelData             = (*(CMP_GLTFCommon*)m_subplugin_loader->GetModelData()).m_meshBufferData;
                        m_3DMeshAnalysisDlg->m_fileName = m_data->m_Full_Path;
                    }
                    else
                    {
                        m_data->m_ModelData             = (*(CMODEL_DATA*)m_subplugin_loader->GetModelData());
                        m_3DMeshAnalysisDlg->m_fileName = QString::fromStdString(m_data->m_ModelData.m_model_name);
                    }
                }
                QTreeWidgetItem* ParentItem = ContextMenu_ImageItem->parent();
                if (ParentItem)
                {
                    QVariant        parentv    = ParentItem->data(TREE_SourceInfo, Qt::UserRole);
                    C_3DModel_Info* parentdata = parentv.value<C_3DModel_Info*>();

                    if (parentdata)
                    {
                        QFileInfo                       fileInfo(parentdata->m_Full_Path);
                        QString                         EXT   = fileInfo.suffix();
                        QByteArray                      ba    = EXT.toUpper().toLatin1();
                        char*                           c_ext = ba.data();
                        PluginInterface_3DModel_Loader* m_plugin_loader;
                        m_plugin_loader = reinterpret_cast<PluginInterface_3DModel_Loader*>(g_pluginManager.GetPlugin("3DMODEL_LOADER", c_ext));

                        void* msgHandler = NULL;

                        cpMainComponents* mainComponents = NULL;
                        mainComponents                   = (cpMainComponents*)m_parent;
                        if (mainComponents)
                            msgHandler = (void*)mainComponents->PrintStatus;

                        if (m_plugin_loader)
                        {
                            m_plugin_loader->TC_PluginSetSharedIO(g_GUI_CMIPS);
                            std::string filename = parentdata->m_Full_Path.toStdString();

                            if (strcmp(c_ext, "GLTF") == 0)
                            {
                                if (isGLTFDracoFile(filename))
                                {
                                    std::size_t dotPos      = filename.rfind('.');
                                    std::string tempdstFile = filename.substr(0, dotPos) + "_tmpdecoded.glTF";
                                    if (!decompressglTFfile(filename, tempdstFile, g_CmdPrams.use_Draco_Encode, g_CmdPrams.CompressOptions))
                                    {
                                        PrintInfo("Error: Decoding glTF file [%s] failed.\n", filename.c_str());
                                        return;
                                    }

                                    filename = tempdstFile;
                                }
                            }

                            int result;
                            if (result = m_plugin_loader->LoadModelData(filename.c_str(), "", &g_pluginManager, msgHandler, &ProgressCallback) != 0)
                            {
                                if (result != 0)
                                {
                                    if (m_CompressStatusDialog)
                                    {
                                        m_CompressStatusDialog->onClearText();
                                        m_CompressStatusDialog->showOutput();
                                    }
                                    PrintInfo("Error Loading Model Data");
                                    return;
                                }
                            }

                            if (strcmp(c_ext, "GLTF") == 0)
                            {
                                parentdata->m_ModelData                = (*(CMP_GLTFCommon*)m_plugin_loader->GetModelData()).m_meshBufferData;
                                m_3DMeshAnalysisDlg->m_fileNameCompare = parentdata->m_Full_Path;
                            }
                            else
                            {
                                parentdata->m_ModelData                = (*(CMODEL_DATA*)m_plugin_loader->GetModelData());
                                m_3DMeshAnalysisDlg->m_fileNameCompare = QString::fromStdString(parentdata->m_ModelData.m_model_name);
                            }
                        }

                        hideProgressDialog();
                        run3DMeshAnalysis(&(m_data->m_ModelData), &(parentdata->m_ModelData));

                        if (m_plugin_loader)
                        {
                            delete m_plugin_loader;
                            m_plugin_loader = nullptr;
                        }
                    }
                    else
                        run3DMeshAnalysis(&(m_data->m_ModelData), NULL);
                }
                else
                {
                    run3DMeshAnalysis(&(m_data->m_ModelData), NULL);
                }

                if (m_subplugin_loader)
                {
                    delete m_subplugin_loader;
                    m_subplugin_loader = nullptr;
                }
                if (g_pProgressDlg)
                    g_pProgressDlg->SetValue(0);
                hideProgressDialog();
            }
            break;
        }

        default:
            break;
        }
    }
}

void ProjectView::viewDiffImageFromChild()
{
    // Get the active Image view node
    if (m_CurrentCompressedImageItem)
    {
        // view image
        QVariant               v      = m_CurrentCompressedImageItem->data(TREE_SourceInfo, Qt::UserRole);
        C_Destination_Options* m_data = v.value<C_Destination_Options*>();
        if (m_data)
        {
            QFileInfo fileinfo(m_data->m_destFileNamePath);
            QFile     file(m_data->m_destFileNamePath);
            if (file.exists() && (fileinfo.suffix().length() > 0))
            {
                QTreeWidgetItem* parent     = m_CurrentCompressedImageItem->parent();
                QString          sourcefile = GetSourceFileNamePath(parent);

                QFileInfo fileinfo(sourcefile);
                QFile     file(sourcefile);
                if (file.exists() && (fileinfo.suffix().length() > 0))
                {
                    m_curDiffSourceFile = m_data->m_sourceFileNamePath;
                    m_curDiffDestFile   = m_data->m_destFileNamePath;

                    diffImageFiles();
                }
            }
        }
    }
}

void ProjectView::viewDiff3DModelFromChild()
{
    // Get the active Image view node
    if (m_CurrentCompressedImageItem)
    {
        // view image
        QVariant           v      = m_CurrentCompressedImageItem->data(TREE_SourceInfo, Qt::UserRole);
        C_3DSubModel_Info* m_data = v.value<C_3DSubModel_Info*>();
        if (m_data)
        {
            QFileInfo fileinfodest(m_data->m_Full_Path);
            ;
            if (QFile::exists(m_data->m_Full_Path) && (fileinfodest.suffix().length() > 0))
            {
                QTreeWidgetItem* parent     = m_CurrentCompressedImageItem->parent();
                QString          sourcefile = GetSourceFileNamePath(parent);

                QFileInfo fileinfosrc(sourcefile);
                if (QFile::exists(sourcefile) && (fileinfosrc.suffix().length() > 0))
                {
                    m_curDiffSourceFile = sourcefile;
                    m_curDiffDestFile   = m_data->m_Full_Path;
                    emit View3DModelFileDiff(m_data, sourcefile, m_data->m_Full_Path);
                }
                else if (!(QFile::exists(sourcefile)))
                {
                    if (m_CompressStatusDialog)
                    {
                        m_CompressStatusDialog->onClearText();
                        m_CompressStatusDialog->showOutput();
                    }
                    PrintInfo("Error: File %s does not exist.\n", sourcefile.toStdString().c_str());
                    return;
                }
            }
            else if (!(QFile::exists(m_data->m_Full_Path)))
            {
                if (m_CompressStatusDialog)
                {
                    m_CompressStatusDialog->onClearText();
                    m_CompressStatusDialog->showOutput();
                }
                PrintInfo("Error: File %s does not exist.\n", m_data->m_Full_Path.toStdString().c_str());
                return;
            }
        }
    }
}

void ProjectView::viewImageDiff()
{
    // Get the active Image view node
    if (m_CurrentCompressedImageItem)
    {
        // view image
        QVariant               v      = m_CurrentCompressedImageItem->data(1, Qt::UserRole);
        C_Destination_Options* m_data = v.value<C_Destination_Options*>();
        if (m_data)
        {
            QFileInfo fileinfo(m_data->m_destFileNamePath);
            QFile     file(m_data->m_destFileNamePath);
            if (file.exists() && (fileinfo.suffix().length() > 0))
            {
                QTreeWidgetItem* parent     = m_CurrentCompressedImageItem->parent();
                QString          sourcefile = GetSourceFileNamePath(parent);

                QFileInfo fileinfo(sourcefile);
                QFile     file(sourcefile);
                if (file.exists() && (fileinfo.suffix().length() > 0))
                {
                    emit ViewImageFileDiff(m_data, sourcefile, m_data->m_destFileNamePath);
                }
            }
        }
    }
}

void ProjectView::SetupTreeView()
{
    m_projectTreeView = new cpTreeWidget(this);

#ifdef USE_DELEGATE
    cpItemDelegate* Linedelegate = new cpItemDelegate;
    m_projectTreeView->setItemDelegate(Linedelegate);
#endif

    m_projectTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_projectTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_projectTreeView->setColumnCount(MAX_PROJECTVIEW_COLUMNS);
    m_projectTreeView->hideColumn(1);
    m_projectTreeView->setColumnWidth(1, 0);
    m_projectTreeView->setColumnWidth(2, 5);
    m_CurrentItem = NULL;

    // Set Icon size
    QSize size;
    size.setHeight(32);
    size.setWidth(32);
    m_projectTreeView->setIconSize(size);

    // Hide the tree view header and allow first column to stretch
    QHeaderView* Header = m_projectTreeView->header();

    Header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    Header->setSectionResizeMode(2, QHeaderView::Fixed);
    Header->setSectionsMovable(false);
    Header->hide();

    m_projectTreeView->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_projectTreeView->setAcceptDrops(true);

    connect(m_projectTreeView, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(onTree_ItemClicked(QTreeWidgetItem*, int)));
    connect(m_projectTreeView, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(onTree_ItemDoubleClicked(QTreeWidgetItem*, int)));
    connect(m_projectTreeView, SIGNAL(DroppedImageItem(QString&, int)), this, SLOT(onDroppedImageItem(QString&, int)));
    connect(m_projectTreeView, SIGNAL(entered(const QModelIndex&)), this, SLOT(onEntered(const QModelIndex&)));
    connect(m_projectTreeView, SIGNAL(event_mousePress(QMouseEvent*, bool)), this, SLOT(onTreeMousePress(QMouseEvent*, bool)));
    connect(m_projectTreeView, SIGNAL(event_keyPress(QKeyEvent*)), this, SLOT(onTreeKeyPress(QKeyEvent*)));

    connect(&m_globalProcessSetting, SIGNAL(globalPropertyChanged(int&)), this, SLOT(onGlobalPropertyChanged(int&)));

    // Top level Root Node
    Tree_AddRootNode();

    // Context Menu Items
    // m_projectTreeView->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_projectTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_projectTreeView, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onCustomContextMenu(const QPoint&)));

    actOpenContainingFolder = new QAction("Open containing folder", this);
    actCopyFullPath         = new QAction("Copy full path", this);

#ifdef USE_CONTEXT_PROJECT
    actsaveProjectFile = new QAction("Save Project", this);
    actopenProjectFile = new QAction("Open Project", this);
#endif

    actCompressProjectFiles = new QAction("Compress selected images", this);
    actAnalyseMeshData      = new QAction("Analyse Mesh Data", this);
    actViewImageDiff        = new QAction("View Image Diff", this);
    actViewImageDiff->setEnabled(false);
    actView3DModelDiff = new QAction("View 3DModel Diff", this);
    actView3DModelDiff->setEnabled(false);
    actRemoveImage = new QAction("Delete selected image", this);
    actSeperator   = new QAction(this);
    actSeperator->setSeparator(true);

    connect(actOpenContainingFolder, SIGNAL(triggered()), this, SLOT(openContainingFolder()));
    connect(actCopyFullPath, SIGNAL(triggered()), this, SLOT(copyFullPath()));

#ifdef USE_CONTEXT_PROJECT
    connect(actsaveProjectFile, SIGNAL(triggered()), this, SLOT(saveProjectFile()));
    connect(actopenProjectFile, SIGNAL(triggered()), this, SLOT(openProjectFile()));
#endif

    connect(actCompressProjectFiles, SIGNAL(triggered()), this, SLOT(onContextMenuCompression()));
    connect(actAnalyseMeshData, SIGNAL(triggered()), this, SLOT(analyseMeshData()));
    connect(actViewImageDiff, SIGNAL(triggered()), this, SLOT(viewDiffImageFromChild()));
    connect(actView3DModelDiff, SIGNAL(triggered()), this, SLOT(viewDiff3DModelFromChild()));
    connect(actRemoveImage, SIGNAL(triggered()), this, SLOT(removeSelectedImage()));

    m_contextMenu = new QMenu(m_projectTreeView);

    m_contextMenu->addAction(actCompressProjectFiles);
    m_contextMenu->addAction(actSeperator);
    m_contextMenu->addAction(actOpenContainingFolder);
    m_contextMenu->addAction(actCopyFullPath);
#ifdef USE_CONTEXT_PROJECT
    m_contextMenu->addAction(actsaveProjectFile);
    m_contextMenu->addAction(actopenProjectFile);
#endif
    m_contextMenu->addAction(actSeperator);
    m_contextMenu->addAction(actViewImageDiff);
    m_contextMenu->addAction(actView3DModelDiff);
    m_contextMenu->addAction(actSeperator);
    m_contextMenu->addAction(actRemoveImage);
    m_contextMenu->addAction(actSeperator);
    m_contextMenu->addAction(actAnalyseMeshData);
}

void ProjectView::Tree_AddRootNode()
{
    m_treeRootItem = new QTreeWidgetItem(m_projectTreeView);
    m_treeRootItem->setFlags(Qt::ItemIsEnabled);
    m_NumItems++;

    // Keep first char as space so when treeview is sorted by file path its aways on top of view
    m_treeRootItem->setText(0, "Double Click here to add files...");
    m_treeRootItem->setIcon(0, QIcon(":/compressonatorgui/images/plus.png"));
    // This item has gray color
    QFont  font("Default", 9, QFont::Bold);
    QBrush b(Qt::gray);
    m_treeRootItem->setForeground(0, b);
    m_treeRootItem->setFont(0, font);

    m_treeRootItem->setData(TREE_LevelType, Qt::UserRole, QVariant::fromValue(TREETYPE_Double_Click_here_to_add_files));
}

void ProjectView::showProgressDialog(QString header)
{
    if (g_pProgressDlg)
    {
        g_pProgressDlg->SetValue(0);
        g_pProgressDlg->SetHeader(header);
        g_pProgressDlg->SetLabelText("");
        g_pProgressDlg->show();
    }
}

void ProjectView::hideProgressDialog()
{
    if (g_pProgressDlg)
        g_pProgressDlg->hide();
}

QTreeWidgetItem* ProjectView::Tree_AddImageFile(QString filePathName, int index, C_Source_Info** m_dataout)
{
    Q_UNUSED(index);

    QTreeWidgetItem* treeItem = NULL;
    QString          filename;
    QFile            SourceFile(filePathName);

    // file not found!
    if (!SourceFile.exists())
    {
        if (filePathName.length() > 3)
            PrintInfo("Error: SourceFile %s not found.\n", filePathName.toStdString().c_str());
        else
            PrintInfo("Error: SourceFile not found.\n");
        return NULL;
    }

    // Check if item already exist if so just exit
    if (Tree_FindImageItem(filePathName, false))
    {
        PrintInfo("Error: item already exist in project tree (%s).\n", filePathName.toStdString().c_str());
        return NULL;
    }

    // Get file Extension and check if it can be loaded by our AMD plugin or is supported by QT, return if is unsupported format
    QFileInfo   finfo(filePathName.toUpper());
    QString     name       = finfo.fileName();
    QStringList list1      = name.split(".");
    QString     PlugInType = list1[list1.size() - 1];
    QByteArray  ba         = PlugInType.toLatin1();
    const char* Ext        = ba.data();

    bool isImage = true;

    QImageReader imageFormat(filePathName);
    if (!(imageFormat.canRead()) && !(g_pluginManager.PluginSupported("IMAGE", (char*)Ext)))
    {
        // The file is not an image checking other supported formats
        if (!g_pluginManager.PluginSupported("3DMODEL_LOADER", (char*)Ext))
        {
            return NULL;
        }
        isImage = false;
    }

    //.cprj is detected as .svg file by qt support format so alway can be drop, below is the fix
    if (strcmp(Ext, "CPRJ") == 0)
    {
        PrintInfo("Error:.cprj is detected as .svg file by qt support format ");
        return NULL;
    }

    //check the support format detected by QT
    //QByteArray supportFormat = QImageReader::imageFormat(filePathName);

    treeItem = new QTreeWidgetItem(m_projectTreeView);
    m_NumItems++;

    // --------------------------------------------
    // Set the Image items size to  32x32
    // for the original image on the Project View
    //---------------------------------------------
    QPixmap filepixmap(filePathName);
    if (filepixmap.size().height() == 0)
    {
        filepixmap.load(":/compressonatorgui/images/file.png");
    }

    QPixmap newPixmap = filepixmap.scaled(QSize(32, 32), Qt::IgnoreAspectRatio);
    QIcon   treeicon;

    treeicon.addPixmap(newPixmap, QIcon::Normal, QIcon::On);

    treeItem->setSizeHint(0, QSize(33, 33));
    treeItem->setSizeHint(2, QSize(5, 33));
    if (isImage)
        treeItem->setIcon(0, treeicon);  //  QIcon(filePathName));
    else
        treeItem->setIcon(0, QIcon(":/compressonatorgui/images/3dfile.png"));

    treeItem->setToolTip(0, filePathName);

    QFileInfo fileInfo(SourceFile.fileName());
    filename = fileInfo.fileName();
    treeItem->setText(0, filename);
    treeItem->setData(TREE_LevelType, Qt::UserRole, QVariant::fromValue(isImage ? TREETYPE_IMAGEFILE_DATA : TREETYPE_3DMODEL_DATA));

    // Create Data for this file and set current know values
    if (isImage)
    {
        C_Source_Info* m_data = new C_Source_Info();
        if (m_data == NULL)
        {
            PrintInfo("Error: Tree_AddImageFile m_data is null");
            return NULL;
        }

        *m_dataout = m_data;

        m_data->m_Name      = filename;
        m_data->m_Full_Path = filePathName;

        QFile file(filePathName);
        m_data->m_FileSize = file.size();

        if (m_data->m_FileSize > 1024000)
            m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024000, 'f', 2) + " MB";
        else if (m_data->m_FileSize > 1024)
            m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024, 'f', 1) + " KB";
        else
            m_data->m_FileSizeStr = QString().number(m_data->m_FileSize) + " Bytes";

        // Load the Image File and MIP data!
        m_data->m_MipImages = m_imageloader->LoadPluginImage(filePathName.toStdString());

        // Mip levels
        if (m_data->m_MipImages)
        {
            if (m_data->m_MipImages->errMsg != "")
            {
                if (m_CompressStatusDialog)
                {
                    m_CompressStatusDialog->onClearText();
                    m_CompressStatusDialog->showOutput();
                }
                PrintInfo("Add Image Error: %s.\n", (m_data->m_MipImages->errMsg).c_str());
                return NULL;
            }
            if (m_data->m_MipImages->mipset)
            {
                if (m_data->m_MipImages->mipset->m_nMipLevels > 1)
                    m_data->m_Mip_Levels = m_data->m_MipImages->mipset->m_nMipLevels - 1;

                // Interal data use
                m_data->m_Height      = m_data->m_MipImages->mipset->m_nHeight;
                m_data->m_Width       = m_data->m_MipImages->mipset->m_nWidth;
                m_data->m_Depth       = m_data->m_MipImages->mipset->m_nDepth;       // depthsupport
                m_data->m_TextureType = m_data->m_MipImages->mipset->m_TextureType;  // depthsupport
                m_data->m_Format      = m_data->m_MipImages->mipset->m_format;

                // User interface
                m_data->m_HeightStr      = QString().number(m_data->m_MipImages->mipset->m_nHeight) + " px";
                m_data->m_WidthStr       = QString().number(m_data->m_MipImages->mipset->m_nWidth) + " px";
                m_data->m_DepthStr       = QString().number(m_data->m_MipImages->mipset->m_nDepth);  // depthsupport
                m_data->m_FormatStr      = GetFormatDesc(m_data->m_MipImages->mipset->m_format);
                m_data->m_TextureTypeStr = GetTextureTypeDesc((CMP_TextureType)m_data->m_MipImages->mipset->m_TextureType);  // depthsupport

                CMIPS     CMips;
                MipLevel* pInMipLevel = CMips.GetMipLevel(m_data->m_MipImages->mipset, 0, 0);
                if (pInMipLevel)
                {
                    m_data->m_ImageSize = pInMipLevel->m_dwLinearSize;

                    if (m_data->m_ImageSize > 1024000)
                        m_data->m_ImageSizeStr = QString().number((double)m_data->m_ImageSize / 1024000, 'f', 2) + " MB";
                    else if (m_data->m_ImageSize > 1024)
                        m_data->m_ImageSizeStr = QString().number((double)m_data->m_ImageSize / 1024, 'f', 1) + " KB";
                    else
                        m_data->m_ImageSizeStr = QString().number(m_data->m_ImageSize) + " Bytes";
                }
                else
                    m_data->m_ImageSizeStr = UNKNOWN_IMAGE;
            }
        }
        treeItem->setData(TREE_SourceInfo, Qt::UserRole, QVariant::fromValue(m_data));

        // Add compression setting option under the new item
        Tree_AddCompressFile(treeItem, STR_AddDestinationSetting, false, false, TREETYPE_Add_destination_setting, NULL);

        emit OnAddedImageSourceNode();
    }
    else
    {
#ifdef _WIN32
        if (g_Application_Options.getGLTFRender() == C_Application_Options::RenderModelsWith::glTF_DX12_EX)
        {
            typedef LONG NTSTATUS, *PNTSTATUS;

            typedef NTSTATUS(WINAPI * RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
            RTL_OSVERSIONINFOW win10OSver = {0};

            HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
            if (hMod)
            {
                RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
                if (fxPtr != nullptr)
                {
                    RTL_OSVERSIONINFOW rovi  = {0};
                    rovi.dwOSVersionInfoSize = sizeof(rovi);
                    if (STATUS_SUCCESS == fxPtr(&rovi))
                    {
                        win10OSver = rovi;
                    }
                }
            }

            if ((win10OSver.dwBuildNumber != 0) && (win10OSver.dwBuildNumber < 15063))
            {
                if (m_CompressStatusDialog)
                {
                    m_CompressStatusDialog->onClearText();
                    m_CompressStatusDialog->showOutput();
                }
                treeItem->setText(2, "???");
                treeItem->setToolTip(2, "glTF DX12 is not supported!");
                PrintInfo("Error: Loading glTF failed. glTF with DX12 only supported on Win10 RS2 or later.\n");
                return NULL;
            }
        }
#endif
        treeItem->setText(2, "...");
        treeItem->setToolTip(2, "show model image files");

        C_3DModel_Info* m_data = new C_3DModel_Info();
        if (m_data == NULL)
        {
            return NULL;
        }

        m_data->m_Name      = filename;
        m_data->m_Full_Path = filePathName;

        QFile file(filePathName);
        m_data->m_FileSize = file.size();

        if (m_data->m_FileSize > 1024000)
            m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024000, 'f', 2) + " MB";
        else if (m_data->m_FileSize > 1024)
            m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024, 'f', 1) + " KB";
        else
            m_data->m_FileSizeStr = QString().number(m_data->m_FileSize) + " Bytes";

        // adding any textures to this 3D Model
        QFileInfo finfo(filePathName);
        QString   filePath = finfo.absolutePath();

        std::ifstream fstream(filePathName.toStdString());
        if (!fstream)
        {
            return nullptr;
        }

        showProgressDialog("Loading Model Data");

        if (strcmp(Ext, "GLTF") == 0)
        {
            m_data->ModelType = eModelType::GLTF;
            // Load the glTF json text file
            fstream >> m_data->m_gltf;
            fstream.close();

            //**********
            // Vertex
            // ************/
            auto buffers = m_data->m_gltf["buffers"];
            for (unsigned int i = 0; i < buffers.size(); i++)
            {
                std::string name = buffers[i]["uri"].get<std::string>();
                QString     str  = filePath + "/" + name.c_str();

                if (g_pProgressDlg)
                    g_pProgressDlg->SetLabelText(QString::fromStdString(name));

                QTreeWidgetItem* child = NULL;

                if (str.contains("bin"))
                    child = Tree_Add3DModelMeshFile(
                        treeItem, filePath + "/" + name.c_str(), m_data->m_Full_Path, false, false, TREETYPE_VIEWMESH_ONLY_NODE, &ProgressCallback);
                else
                    PrintInfo("Note: embedded glTF mesh process is not supported yet. only .bin mesh is supported now.");

                int image_filesize = 0;

                // get the child node we just added and its data
                if (child)
                {
                    QVariant       v           = child->data(TREE_SourceInfo, Qt::UserRole);
                    C_Source_Info* m_ChildData = v.value<C_Source_Info*>();

                    if (m_ChildData)
                    {
                        image_filesize = m_ChildData->m_ImageSize;
                    }
                }
                Model_Image model_image;
                model_image.m_isImage      = false;
                model_image.m_srcDelFlag   = false;
                model_image.m_FilePathName = str;
                model_image.m_FileSize     = image_filesize;
                model_image.m_Width        = 0;
                model_image.m_Height       = 0;
                model_image.child          = child;
                m_data->m_Model_Images.append(model_image);
            }

            /**********
            Images
            ************/
            auto Images = m_data->m_gltf["images"];
            for (unsigned int i = 0; i < Images.size(); i++)
            {
                std::string name = Images[i]["uri"].get<std::string>();
                QString     str  = filePath + "/" + name.c_str();
                if (g_pProgressDlg)
                    g_pProgressDlg->SetLabelText(QString::fromStdString(name));

                QTreeWidgetItem* child =
                    Tree_Add3DModelImageFiles(treeItem, filePath + "/" + name.c_str(), false, false, TREETYPE_VIEWIMAGE_ONLY_NODE, &ProgressCallback);

                int image_filesize = 0;
                int image_width    = 0;
                int image_height   = 0;

                if (child)
                {
                    QVariant       v           = child->data(TREE_SourceInfo, Qt::UserRole);
                    C_Source_Info* m_ChildData = v.value<C_Source_Info*>();

                    if (m_ChildData)
                    {
                        image_filesize = m_ChildData->m_ImageSize;
                        image_width    = m_ChildData->m_Width;
                        image_height   = m_ChildData->m_Height;
                    }
                }

                Model_Image model_image;
                model_image.m_isImage      = true;
                model_image.m_srcDelFlag   = false;
                model_image.m_FilePathName = str;
                model_image.m_FileSize     = image_filesize;
                model_image.m_Width        = image_width;
                model_image.m_Height       = image_height;
                model_image.child          = child;
                m_data->m_Model_Images.append(model_image);
            }

            auto asset = m_data->m_gltf["asset"];
            if (asset.size() > 0)
            {
                auto gen = asset["generator"];
                if (gen.size() > 0)
                    m_data->m_GeneratorStr = gen.get<std::string>().c_str();

                auto ver = asset["version"];
                if (ver.size() > 0)
                    m_data->m_VersionStr = ver.get<std::string>().c_str();
            }
            // Add compression setting option under the new item
            treeItem->setData(TREE_SourceInfo, Qt::UserRole, QVariant::fromValue(m_data));
            Tree_AddCompressFile(treeItem, STR_AddModelDestinationSetting, false, false, TREETYPE_Add_Model_destination_settings, NULL);

            // Add the image to the diff image list if it is not in the list
            if ((!(m_ImagesinProjectTrees.contains(filePathName))) && (!(filePathName.contains(".gltf"))) && (!(filePathName.contains(".obj"))))
                m_ImagesinProjectTrees.append(filePathName);
        }
        else if (strcmp(Ext, "OBJ") == 0)
        {
            m_data->ModelType = eModelType::OBJ;
            // Add compression setting option under the new item
            treeItem->setData(TREE_SourceInfo, Qt::UserRole, QVariant::fromValue(m_data));
            Tree_AddCompressFile(treeItem, STR_AddModelDestinationSetting, false, false, TREETYPE_Add_Model_destination_settings, NULL);

            Model_Image model_image;
            model_image.m_isImage      = false;
            model_image.m_srcDelFlag   = false;
            model_image.m_FilePathName = filePathName;
            model_image.m_FileSize     = m_data->m_FileSize;
            model_image.m_Width        = 0;
            model_image.m_Height       = 0;
            m_data->m_Model_Images.append(model_image);

            //Load obj file
#ifdef USE_LOADMODELDATA
            if (g_pProgressDlg)
                g_pProgressDlg->SetLabelText("Loading " + filename);

            PluginInterface_3DModel_Loader* plugin_loader = NULL;
            plugin_loader = reinterpret_cast<PluginInterface_3DModel_Loader*>(g_pluginManager.GetPlugin("3DMODEL_LOADER", "OBJ"));
            if (plugin_loader)
            {
                plugin_loader->TC_PluginSetSharedIO(g_GUI_CMIPS);
                int result = plugin_loader->LoadModelData(m_data->m_Full_Path.toStdString().data(), "", &g_pluginManager, NULL, &ProgressCallback);
                if (result != 0)
                {
                    if (m_CompressStatusDialog)
                        m_CompressStatusDialog->appendText("Error in loading mesh file.");
                }
                else
                {
                    CMODEL_DATA* temp   = (CMODEL_DATA*)(plugin_loader->GetModelData());
                    m_data->m_ModelData = temp[0];
                }
            }
            else
            {
                if (m_CompressStatusDialog)
                    m_CompressStatusDialog->appendText("File format not supported.");
            }
            if (plugin_loader)
            {
                delete plugin_loader;
                plugin_loader = NULL;
            }
#endif
        }
        else if (strcmp(Ext, "DRC") == 0)
        {
            m_data->ModelType = eModelType::DRC;
            treeItem->setData(TREE_SourceInfo, Qt::UserRole, QVariant::fromValue(m_data));
        }

        hideProgressDialog();
    }

    m_saveProjectChanges = true;

    return treeItem;
}

void ProjectView::Tree_Add3DSubModelFile(QTreeWidgetItem* ParentItem, QString filePathName, QList<bool>* srcDelFlags)
{
    QTreeWidgetItem* treeItem = NULL;
    QString          filename;
    QFile            SourceFile(filePathName);

    // QFileInfo finfo(filePathName.toUpper());
    // QString name = finfo.fileName();
    // QStringList list1 = name.split(".");
    // QString PlugInType = list1[list1.size() - 1];
    // ba = PlugInType.toLatin1();
    // const char *Ext = ba.data();

    treeItem = new QTreeWidgetItem();

    m_NumItems++;

    // --------------------------------------------
    // Set the Image items size to  32x32
    // for the original image on the Project View
    //---------------------------------------------
    QPixmap filepixmap(filePathName);
    if (filepixmap.size().height() == 0)
    {
        filepixmap.load(":/compressonatorgui/images/file.png");
    }

    QPixmap newPixmap = filepixmap.scaled(QSize(32, 32), Qt::IgnoreAspectRatio);
    QIcon   treeicon;

    treeicon.addPixmap(newPixmap, QIcon::Normal, QIcon::On);
    treeItem->setSizeHint(0, QSize(33, 33));
    treeItem->setIcon(0, QIcon(":/compressonatorgui/images/3dfile.png"));

    treeItem->setToolTip(0, filePathName);

    QFileInfo fileInfo(SourceFile.fileName());
    filename = fileInfo.fileName();
    treeItem->setText(0, filename);
    treeItem->setData(TREE_LevelType, Qt::UserRole, QVariant::fromValue(TREETYPE_3DSUBMODEL_DATA));

    // Create Data for this file and set current know values
    C_3DSubModel_Info* m_data = new C_3DSubModel_Info();
    if (m_data == NULL)
    {
        return;
    }

    m_data->m_Name      = filename;
    m_data->m_Full_Path = filePathName;
    QFile file(filePathName);
    m_data->m_FileSize = file.size();

    if (m_data->m_FileSize > 1024000)
        m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024000, 'f', 2) + " MB";
    else if (m_data->m_FileSize > 1024)
        m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024, 'f', 1) + " KB";
    else
        m_data->m_FileSizeStr = QString().number(m_data->m_FileSize) + " Bytes";

    // adding any textures to this 3D Model
    QFileInfo finfo(filePathName);
    QString   filePath = finfo.absolutePath();

    eModelType ModelType = eModelType::GLTF;

    // Add parent item data to child  : Should Check its type!
    QVariant        parentv      = ParentItem->data(TREE_SourceInfo, Qt::UserRole);
    C_3DModel_Info* m_parentdata = parentv.value<C_3DModel_Info*>();
    if (m_parentdata)
    {
        m_data->m_ModelSource_gltf       = m_parentdata->m_Full_Path;
        m_data->m_Model_Images           = m_parentdata->m_Model_Images;
        m_data->m_ModelData              = m_parentdata->m_ModelData;
        m_data->m_ModelData.m_model_name = filePathName.toStdString();
        ModelType                        = m_parentdata->ModelType;
        m_data->ModelType                = ModelType;
    }

    if (ModelType == eModelType::GLTF)
    {
        std::ifstream fstream(filePathName.toStdString());
        if (!fstream)
        {
            return;
        }

        // Load the glTF json text file
        nlohmann::json j3;
        fstream >> j3;
        fstream.close();

        /*********
        Note: m_SubModel_Images size = size of buffers + size of images
        *********/

        /**********
        Buffers
        ************/
        auto buffers = j3["buffers"];
        for (unsigned int i = 0; i < buffers.size(); i++)
        {
            std::string name = buffers[i]["uri"].get<std::string>();
            QString     str  = filePath + "/" + name.c_str();
            Model_Image model_image;
            model_image.m_FilePathName = str;
            model_image.m_srcDelFlag   = false;
            model_image.m_isImage      = false;
            m_data->m_SubModel_Images.append(model_image);  // This should be replaced by m_SubModel_Images
        }

        /**********
        Images
        ************/
        auto Images = j3["images"];
        for (unsigned int i = 0; i < Images.size(); i++)
        {
            std::string name = Images[i]["uri"].get<std::string>();
            QString     str  = filePath + "/" + name.c_str();
            Model_Image model_image;
            model_image.m_FilePathName = str;
            model_image.m_srcDelFlag   = false;
            model_image.m_isImage      = true;
            m_data->m_SubModel_Images.append(model_image);  // This should be replaced by m_SubModel_Images
        }

        if (srcDelFlags != NULL)
        {
            int indexStart = (int)buffers.size();
            if (srcDelFlags->size() <= (m_data->m_Model_Images.size() - indexStart))
            {
                for (int i = 0; i < srcDelFlags->size(); i++)
                    m_data->m_SubModel_Images[i + indexStart].m_srcDelFlag = srcDelFlags->at(i);
            }
        }

        auto asset = j3["asset"];
        if (asset.size() > 0)
        {
            std::string name = asset["generator"].get<std::string>();
            if (name.length() > 0)
                m_data->m_GeneratorStr = name.c_str();
            name = asset["version"].get<std::string>();
            if (name.length() > 0)
                m_data->m_VersionStr = name.c_str();
        }
    }
    else if (ModelType == eModelType::OBJ)
    {
        // For Obj file the source is the file itself..!
        std::string name = m_data->m_Name.toStdString();
        QString     str  = filePath + "/" + name.c_str();
        Model_Image model_image;
        model_image.m_FilePathName = str;
        model_image.m_srcDelFlag   = false;
        model_image.m_isImage      = false;
        m_data->m_SubModel_Images.append(model_image);
    }

    // Add compression setting option under the new item
    treeItem->setData(TREE_SourceInfo, Qt::UserRole, QVariant::fromValue(m_data));
    // Add compression setting option under the new item
    Tree_AddCompressFile(treeItem, STR_AddDestinationSetting, false, false, TREETYPE_Add_destination_setting, NULL);

    // QTreeWidgetItem *parent = ParentItem->parent();
    ParentItem->addChild(treeItem);
}

QTreeWidgetItem* ProjectView::Tree_Add3DModelImageFiles(QTreeWidgetItem*  ParentItem,
                                                        QString           filePathName,
                                                        bool              checkable,
                                                        bool              checked,
                                                        int               levelType,
                                                        CMP_Feedback_Proc pFeedbackProc)
{
    Q_UNUSED(checkable)
    Q_UNUSED(checked)
    Q_UNUSED(levelType)
    if (ParentItem == NULL)
        return NULL;

    /**************
    File Exists
    ***************/
    QString filename;
    QFile   f(filePathName);

    // file not found!
    if (!f.exists())
    {
        return NULL;
    }

    /*************************
    Image file type supported
    **************************/
    // Get file Extension and check if it can be loaded by our AMD plugin or is supported by QT, return if is unsupported format
    QFileInfo   fi(filePathName.toUpper());
    QString     name       = fi.fileName();
    QStringList list1      = name.split(".");
    QString     PlugInType = list1[list1.size() - 1];
    QByteArray  ba         = PlugInType.toLatin1();
    const char* Ext        = ba.data();

    QImageReader imageFormat(filePathName);
    if (!(imageFormat.canRead()) && !(g_pluginManager.PluginSupported("IMAGE", (char*)Ext)))
    {
        return NULL;
    }

    /***********************
    Add the image file item
    ***********************/
    QTreeWidgetItem* treeItem = new QTreeWidgetItem();
    m_NumItems++;

    // --------------------------------------------
    // Set the Image items size to  32x32
    // for the original image on the Project View
    //---------------------------------------------
    QPixmap filepixmap(filePathName);
    if (filepixmap.size().height() == 0)
    {
        filepixmap.load(":/compressonatorgui/images/file.png");
    }

    QPixmap newPixmap = filepixmap.scaled(QSize(32, 32), Qt::IgnoreAspectRatio);
    QIcon   treeicon;

    treeicon.addPixmap(newPixmap, QIcon::Normal, QIcon::On);

    treeItem->setSizeHint(0, QSize(33, 33));
    treeItem->setIcon(0, treeicon);  //  QIcon(filePathName));

    treeItem->setToolTip(0, filePathName);

    QFileInfo fileInfo(f.fileName());
    filename = fileInfo.fileName();
    treeItem->setText(0, filename);
    treeItem->setData(TREE_LevelType, Qt::UserRole, QVariant::fromValue(levelType));

    /*********************************************************
    Create Data for this file and set current know values
    *********************************************************/
    C_Source_Info* m_data = new C_Source_Info();
    if (m_data == NULL)
    {
        return NULL;
    }

    m_data->m_Name      = filename;
    m_data->m_Full_Path = filePathName;
    QFile file(filePathName);
    m_data->m_FileSize = file.size();

    if (m_data->m_FileSize > 1024000)
        m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024000, 'f', 2) + " MB";
    else if (m_data->m_FileSize > 1024)
        m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024, 'f', 1) + " KB";
    else
        m_data->m_FileSizeStr = QString().number(m_data->m_FileSize) + " Bytes";

    // Load the Image File and MIP data!
    m_data->m_MipImages = m_imageloader->LoadPluginImage(filePathName.toStdString(), pFeedbackProc);

    // Mip levels
    if (m_data->m_MipImages)
    {
        if (m_data->m_MipImages->mipset)
        {
            if (m_data->m_MipImages->mipset->m_nMipLevels > 1)
                m_data->m_Mip_Levels = m_data->m_MipImages->mipset->m_nMipLevels - 1;

            m_data->m_Height    = m_data->m_MipImages->mipset->m_nHeight;
            m_data->m_Width     = m_data->m_MipImages->mipset->m_nWidth;
            m_data->m_HeightStr = QString().number(m_data->m_Height) + " px";
            m_data->m_WidthStr  = QString().number(m_data->m_Width) + " px";
            m_data->m_FormatStr = GetFormatDesc(m_data->m_MipImages->mipset->m_format);
            m_data->m_Format    = m_data->m_MipImages->mipset->m_format;

            CMIPS     CMips;
            MipLevel* pInMipLevel = CMips.GetMipLevel(m_data->m_MipImages->mipset, 0, 0);
            if (pInMipLevel)
            {
                m_data->m_ImageSize = pInMipLevel->m_dwLinearSize;

                if (m_data->m_ImageSize > 1024000)
                    m_data->m_ImageSizeStr = QString().number((double)m_data->m_ImageSize / 1024000, 'f', 2) + " MB";
                else if (m_data->m_ImageSize > 1024)
                    m_data->m_ImageSizeStr = QString().number((double)m_data->m_ImageSize / 1024, 'f', 1) + " KB";
                else
                    m_data->m_ImageSizeStr = QString().number(m_data->m_ImageSize) + " Bytes";
            }
            else
                m_data->m_ImageSizeStr = UNKNOWN_IMAGE;
        }
    }

    // Save the child node and its data
    treeItem->setData(TREE_SourceInfo, Qt::UserRole, QVariant::fromValue(m_data));
    ParentItem->addChild(treeItem);
    treeItem->setHidden(true);

    m_saveProjectChanges = true;

    return treeItem;
}

QTreeWidgetItem* ProjectView::Tree_Add3DModelMeshFile(QTreeWidgetItem*  ParentItem,
                                                      QString           filePathName,
                                                      QString           pfilePathName,
                                                      bool              checkable,
                                                      bool              checked,
                                                      int               levelType,
                                                      CMP_Feedback_Proc pFeedbackProc)
{
    Q_UNUSED(checkable)
    Q_UNUSED(checked)
    Q_UNUSED(levelType)
    Q_UNUSED(pFeedbackProc)

    if (ParentItem == NULL)
        return NULL;

    /**************
    File Exists
    ***************/
    QString filename;
    QFile   f(filePathName);

    // file not found!
    if (!f.exists())
    {
        return NULL;
    }

    /***********************
    Add the file item
    ***********************/
    QTreeWidgetItem* treeItem = new QTreeWidgetItem();
    m_NumItems++;

    // --------------------------------------------
    // Add icon file mesh file
    //---------------------------------------------
    QPixmap filepixmap(filePathName);
    if (filepixmap.size().height() == 0)
    {
        filepixmap.load(":/compressonatorgui/images/file.png");
    }

    QPixmap newPixmap = filepixmap.scaled(QSize(32, 32), Qt::IgnoreAspectRatio);
    QIcon   treeicon;

    treeicon.addPixmap(newPixmap, QIcon::Normal, QIcon::On);
    treeItem->setSizeHint(0, QSize(33, 33));
    treeItem->setIcon(0, treeicon);  //  QIcon(filePathName));
    treeItem->setToolTip(0, filePathName);

    QFileInfo fileInfo(f.fileName());
    filename = fileInfo.fileName();
    treeItem->setText(0, filename);
    treeItem->setData(TREE_LevelType, Qt::UserRole, QVariant::fromValue(levelType));

    /*********************************************************
    Create Data for this file and set current know values
    *********************************************************/
    C_Mesh_Buffer_Info* m_data = new C_Mesh_Buffer_Info();
    if (m_data == NULL)
    {
        return NULL;
    }

    m_data->m_Name      = filename;
    m_data->m_Full_Path = filePathName;
    QFile file(filePathName);
    m_data->m_FileSize      = file.size();
    m_data->m_glTF_filePath = pfilePathName;

    if (m_data->m_FileSize > 1024000)
        m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024000, 'f', 2) + " MB";
    else if (m_data->m_FileSize > 1024)
        m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024, 'f', 1) + " KB";
    else
        m_data->m_FileSizeStr = QString().number(m_data->m_FileSize) + " Bytes";

    // Save the child node and its data
    treeItem->setData(TREE_SourceInfo, Qt::UserRole, QVariant::fromValue(m_data));
    ParentItem->addChild(treeItem);
    treeItem->setHidden(true);
    m_saveProjectChanges = true;

    return treeItem;
}

void ProjectView::Tree_AddCompressFile(QTreeWidgetItem*       ParentItem,
                                       QString                description,
                                       bool                   checkable,
                                       bool                   checked,
                                       int                    levelType,
                                       C_Destination_Options* m_data)
{
    if (ParentItem == NULL)
        return;

    QTreeWidgetItem* treeItem = new QTreeWidgetItem();
    m_NumItems++;

    treeItem->setText(0, description);
    treeItem->setData(TREE_LevelType, Qt::UserRole, QVariant::fromValue(levelType));

    switch (levelType)
    {
    case TREETYPE_Add_destination_setting:
    case TREETYPE_Add_Model_destination_settings: {
        // This item has gray color
        QFont  font("", 9, QFont::Bold);
        QBrush b(Qt::gray);
        treeItem->setForeground(0, b);
        treeItem->setFont(0, font);
        ParentItem->addChild(treeItem);
        treeItem->setFlags(Qt::ItemIsEnabled);
        treeItem->setIcon(0, QIcon(":/compressonatorgui/images/plussettings.png"));
    }
    break;

    case TREETYPE_MESH_DATA: {
        if (m_data)
        {
            if (checkable && m_EnableCheckedItemsView)
            {
                treeItem->setFlags(treeItem->flags() | Qt::ItemIsUserCheckable);
                if (checked)
                    treeItem->setCheckState(0, Qt::Checked);
                else
                    treeItem->setCheckState(0, Qt::Unchecked);
            }
            else
                treeItem->setFlags(treeItem->flags() | Qt::ItemIsSelectable);

            // treeItem->setFlags(treeItem->flags() | Qt::ItemIsEditable);
            treeItem->setData(TREE_SourceInfo, Qt::UserRole, QVariant::fromValue(m_data));
            treeItem->setToolTip(0, m_data->m_destFileNamePath);

            Tree_updateCompressIcon(treeItem, m_data->m_destFileNamePath, false);

            // get parent
            QTreeWidgetItem* parent = ParentItem->parent();

            // if parent is null, return!
            if (!parent)
            {
                return;
            }

            if (parent)
            {
                QVariant v           = parent->data(TREE_LevelType, Qt::UserRole);
                int      levelType   = v.toInt();
                m_data->m_SourceType = levelType;

                if (levelType == TREETYPE_3DSUBMODEL_DATA)
                {
                    QVariant           v         = parent->data(TREE_SourceInfo, Qt::UserRole);
                    C_3DSubModel_Info* imagedata = v.value<C_3DSubModel_Info*>();
                    if (imagedata)
                    {
                        m_data->m_OriginalMipImages = NULL;
                        int index                   = 0;
                        while (index < imagedata->m_Model_Images.size())
                        {
                            if (m_data->m_sourceFileNamePath == imagedata->m_Model_Images[index].m_FilePathName)
                            {
                                imagedata->m_SubModel_Images[index].m_srcDelFlag = true;
                                break;
                            }
                            index++;
                        }
                        // use the image index to get more info on the source images sizes
                        if (index < imagedata->m_Model_Images.size())
                        {
                            m_data->m_DstWidth        = imagedata->m_Model_Images[index].m_Width;
                            m_data->m_DstHeight       = imagedata->m_Model_Images[index].m_Height;
                            m_data->m_HeightStr       = QString().number(m_data->m_DstHeight) + " px";
                            m_data->m_WidthStr        = QString().number(m_data->m_DstWidth) + " px";
                            m_data->m_SourceImageSize = imagedata->m_Model_Images[index].m_FileSize;
                        }
                        else
                        {
                            m_data->m_DstWidth  = 0;
                            m_data->m_DstHeight = 0;
                            m_data->m_HeightStr = "";
                            m_data->m_WidthStr  = "";
                        }

                        m_data->m_Model_Images = imagedata->m_Model_Images;
                        m_data->setMeshData(imagedata->m_ModelData);
                        parent->addChild(treeItem);
                    }
                    else
                        return;
                }
                else
                    return;  // Some error occured as Mesh Data node should have a parent!
            }

            ParentItem->setExpanded(true);
            treeItem->setExpanded(true);

            emit OnAddedCompressSettingNode();

            m_saveProjectChanges = true;
        }
    }
    break;
    case TREETYPE_COMPRESSION_DATA: {
        if (m_data)
        {
            if (checkable && m_EnableCheckedItemsView)
            {
                treeItem->setFlags(treeItem->flags() | Qt::ItemIsUserCheckable);
                if (checked)
                    treeItem->setCheckState(0, Qt::Checked);
                else
                    treeItem->setCheckState(0, Qt::Unchecked);
            }
            else
                treeItem->setFlags(treeItem->flags() | Qt::ItemIsSelectable);

            // treeItem->setFlags(treeItem->flags() | Qt::ItemIsEditable);
            treeItem->setData(TREE_SourceInfo, Qt::UserRole, QVariant::fromValue(m_data));
            treeItem->setToolTip(0, m_data->m_destFileNamePath);
            Tree_updateCompressIcon(treeItem, m_data->m_destFileNamePath, false);

            // get parent
            QTreeWidgetItem* parent = ParentItem->parent();

            // if parent is null, Node itself is parent
            if (!parent)
            {
                parent = ParentItem;
            }

            if (parent)
            {
                parent->addChild(treeItem);

                QVariant v           = parent->data(TREE_LevelType, Qt::UserRole);
                int      levelType   = v.toInt();
                m_data->m_SourceType = levelType;

                if (levelType == TREETYPE_IMAGEFILE_DATA)
                {
                    QVariant       v         = parent->data(TREE_SourceInfo, Qt::UserRole);
                    C_Source_Info* imagedata = v.value<C_Source_Info*>();
                    if (imagedata)
                    {
                        m_data->m_DstWidth                 = imagedata->m_Width;
                        m_data->m_DstHeight                = imagedata->m_Height;
                        m_data->m_HeightStr                = QString().number(m_data->m_DstHeight) + " px";
                        m_data->m_WidthStr                 = QString().number(m_data->m_DstWidth) + " px";
                        m_data->m_SourceIscompressedFormat = CMP_IsCompressedFormat(imagedata->m_Format);
                        m_data->m_SourceIsFloatFormat      = CMP_IsFloatFormat(imagedata->m_Format);
                        m_data->m_OriginalMipImages        = imagedata->m_MipImages;
                    }
                    else
                        return;
                }
                else if (levelType == TREETYPE_3DMODEL_DATA)
                {  // This case should not occur!
                    QVariant        v         = parent->data(TREE_SourceInfo, Qt::UserRole);
                    C_3DModel_Info* imagedata = v.value<C_3DModel_Info*>();
                    if (imagedata)
                    {
                        m_data->m_DstWidth          = 0;
                        m_data->m_DstHeight         = 0;
                        m_data->m_HeightStr         = "";
                        m_data->m_WidthStr          = "";
                        m_data->m_OriginalMipImages = NULL;
                        m_data->m_modelSource       = imagedata->m_Full_Path;
                        m_data->m_Model_Images      = imagedata->m_Model_Images;
                    }
                    else
                        return;
                }
                else if (levelType == TREETYPE_3DSUBMODEL_DATA)
                {
                    QVariant           v         = parent->data(TREE_SourceInfo, Qt::UserRole);
                    C_3DSubModel_Info* imagedata = v.value<C_3DSubModel_Info*>();
                    if (imagedata)
                    {
                        m_data->m_OriginalMipImages = NULL;
                        int index                   = 0;
                        while (index < imagedata->m_Model_Images.size())
                        {
                            if (m_data->m_sourceFileNamePath == imagedata->m_Model_Images[index].m_FilePathName)
                            {
                                imagedata->m_SubModel_Images[index].m_srcDelFlag = true;
                                break;
                            }
                            index++;
                        }
                        // use the image index to get more info on the source images sizes
                        if (index < imagedata->m_Model_Images.size())
                        {
                            m_data->m_DstWidth        = imagedata->m_Model_Images[index].m_Width;
                            m_data->m_DstHeight       = imagedata->m_Model_Images[index].m_Height;
                            m_data->m_HeightStr       = QString().number(m_data->m_DstHeight) + " px";
                            m_data->m_WidthStr        = QString().number(m_data->m_DstWidth) + " px";
                            m_data->m_SourceImageSize = imagedata->m_Model_Images[index].m_FileSize;
                        }
                        else
                        {
                            m_data->m_DstWidth  = 0;
                            m_data->m_DstHeight = 0;
                            m_data->m_HeightStr = "";
                            m_data->m_WidthStr  = "";
                        }

                        m_data->m_Model_Images = imagedata->m_Model_Images;
                    }
                    else
                        return;
                }
            }
            ParentItem->setExpanded(true);
            treeItem->setExpanded(true);

            emit OnAddedCompressSettingNode();

            m_saveProjectChanges = true;
        }
    }
    break;
    default:
        if (treeItem)
        {
            delete treeItem;
            treeItem = NULL;
        }
        break;
    }
}

bool ProjectView::Tree_updateCompressIcon(QTreeWidgetItem* item, QString FileNamePath, bool RedIcon)
{
    if (!item)
        return false;

    bool        result = false;
    std::string state  = CMP_ORIGINAL;
    QFileInfo   fileinfo(FileNamePath);
    QFile       file(FileNamePath);

    //for unprocessed obj setting node
    if (FileNamePath.contains(".obj") || FileNamePath.contains(".OBJ"))
    {
#ifdef _WIN32
        state = readObjFileState(FileNamePath.toStdString());
        if (state == CMP_FILE_ERROR)
            return result;
#endif
    }

    if (file.exists() && (fileinfo.suffix().length() > 0) && (state != CMP_COPY))
    {
        item->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallgreenstone.png")));
        result = true;
    }
    else
    {
        if (RedIcon)
            item->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallredstone.png")));
        else
            item->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallwhiteblank.png")));
    }

    return result;
}

void ProjectView::Tree_selectAllChildItems(QTreeWidgetItem* item)
{
    if (item)
    {
        if (item->childCount() > 0)
        {
            for (int r = 0; r < item->childCount(); r++)
            {
                QTreeWidgetItem* child = item->child(r);
                child->setSelected(true);
                for (int j = 0; j < child->childCount(); j++)
                {
                    QTreeWidgetItem* child2 = child->child(j);
                    child2->setSelected(true);
                }
            }
        }
    }
}

int ProjectView::GetNumItemsByType(int typeFlags)
{
    int numItems = 0;

    for (QTreeWidgetItemIterator it(m_treeRootItem); *it; ++it)
    {
        QTreeWidgetItem* item     = *it;
        int              itemType = GetItemType(item);

        if (itemType == TREETYPE_Double_Click_here_to_add_files || itemType == TREETYPE_Add_destination_setting ||
            itemType == TREETYPE_Add_Model_destination_settings)
            continue;

        if (typeFlags == 0 || (itemType & typeFlags))
            ++numItems;
    }

    return numItems;
}
int ProjectView::GetNumItems()
{
    return GetNumItemsByType(0);
}
int ProjectView::GetNumDestItems()
{
    return GetNumItemsByType(TREETYPE_COMPRESSION_DATA | TREETYPE_MESH_DATA);
}

int ProjectView::GetNumSelectedItemsByType(int typeFlags)
{
    int numItems = 0;

    for (QTreeWidgetItemIterator it(m_treeRootItem); *it; ++it)
    {
        QTreeWidgetItem* item = *it;

        if (!item->isSelected())
            continue;

        int itemType = GetItemType(item);

        if (itemType == TREETYPE_Double_Click_here_to_add_files || itemType == TREETYPE_Add_destination_setting ||
            itemType == TREETYPE_Add_Model_destination_settings)
            continue;

        if (typeFlags == 0 || (itemType & typeFlags))
            ++numItems;
    }

    return numItems;
}
int ProjectView::GetNumSelectedItems()
{
    return GetNumSelectedItemsByType(0);
}
int ProjectView::GetNumSelectedDestItems()
{
    return GetNumSelectedItemsByType(TREETYPE_COMPRESSION_DATA | TREETYPE_MESH_DATA);
}

void ProjectView::Tree_clearAllItemsSelected()
{
    // Find the item and set it as selected
    QTreeWidgetItemIterator it(m_treeRootItem);
    while (*it)
    {
        if (m_EnableCheckedItemsView)
        {
            (*it)->setCheckState(0, Qt::Unchecked);
        }
        (*it)->setSelected(false);
        ++it;
    }
}

void ProjectView::Tree_setAllItemsSelected()
{
    // Find the item and set it as selected
    QTreeWidgetItemIterator it(m_treeRootItem);
    while (*it)
    {
        if (m_EnableCheckedItemsView)
        {
            (*it)->setCheckState(0, Qt::Checked);
        }
        (*it)->setSelected(true);
        ++it;
    }
}

std::vector<QTreeWidgetItem*> ProjectView::GetAllSelectedImages()
{
    std::vector<QTreeWidgetItem*> selectedImages;
    for (QTreeWidgetItemIterator it(m_treeRootItem); *it; ++it)
    {
        QVariant variant = (*it)->data(TREE_LevelType, Qt::UserRole);
        int      type    = variant.toInt();
        if (type == TREETYPE_IMAGEFILE_DATA && (*it)->isSelected())
            selectedImages.push_back(*it);
    }

    return selectedImages;
}

QTreeWidgetItem* ProjectView::Tree_FindImageItem(QString filePathName, bool includeDestination)
{
    // Make sure FileName does not contain
    // any specialized prefixes
    // Currently Differance us used for image compare
    //
    if (filePathName.contains(DIFFERENCE_IMAGE_TXT))
    {
        filePathName.remove(0, sizeof(DIFFERENCE_IMAGE_TXT) - 1);
    }

    // Find the item and set it as selected
    QTreeWidgetItemIterator it(m_treeRootItem);
    while (*it)
    {
        QString Source_FilePathName = GetSourceFileNamePath(*it);
        // Match any Source file
        if (filePathName.compare(Source_FilePathName, Qt::CaseInsensitive) == 0)
        {
            return (*it);
        }

        if (includeDestination)
        {
            QString Destination_FilePathName = GetDestinationFileNamePath(*it);
            // Match any Destination file
            if (filePathName.compare(Destination_FilePathName, Qt::CaseInsensitive) == 0)
            {
                return (*it);
            }
        }

        ++it;
    }

    return NULL;
}

void ProjectView::onTree_ItemClicked(QTreeWidgetItem* item, int column)
{
    if (!item)
        return;
    if (m_processBusy)
        return;
    bool ViewImage = (m_clicked_onIcon && g_Application_Options.m_clickIconToViewImage) || (!g_Application_Options.m_clickIconToViewImage);

    int islevelType = GetItemType(item);

    // Some of the main tools bar action button are dependent on what type
    // of Tree item is selected, example Del should be enabled for any item
    // and MipGen only for Images and Not Models
    emit OnSetToolBarActions(islevelType);

    // Reset Image Diff files for click on invalid types
    m_curDiffSourceFile = "";
    m_curDiffDestFile   = "";

    if (islevelType == TREETYPE_Double_Click_here_to_add_files)
    {
        // Clears of selected items when user clicks on this node
        Tree_clearAllItemsSelected();
        SignalUpdateData(item, islevelType);
    }
    else if (islevelType == TREETYPE_VIEWMESH_ONLY_NODE)
    {
        m_CurrentCompressedImageItem = NULL;

        // view gltf bin
        QVariant            v      = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Mesh_Buffer_Info* m_data = v.value<C_Mesh_Buffer_Info*>();
        if (m_data && (column == 0) && (ViewImage))
        {
            QString text = m_data->m_Full_Path;
            emit    ViewImageFile(text, item);
        }

        // Update the bin poperty view for the item clicked
        SignalUpdateData(item, islevelType);
    }
    else if ((islevelType == TREETYPE_IMAGEFILE_DATA) || (islevelType == TREETYPE_VIEWIMAGE_ONLY_NODE))
    {
        m_CurrentCompressedImageItem = NULL;

        emit OnSourceImage(item->childCount());

        // view image
        QVariant       v      = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Source_Info* m_data = v.value<C_Source_Info*>();
        if (m_data && (column == 0) && (ViewImage))
        {
            QString text = m_data->m_Full_Path;
            emit    ViewImageFile(text, item);
        }

        Tree_selectAllChildItems(item);

        // Update the image poperty view for the item clicked
        SignalUpdateData(item, islevelType);
    }
    else if (islevelType == TREETYPE_COMPRESSION_DATA)
    {
        // view image
        QVariant               v      = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Destination_Options* m_data = v.value<C_Destination_Options*>();
        // Since user has clicked on this item : prepair image diff file incase user want to do a diff
        // using the main apps tool bar
        if (m_data)
        {
            m_curDiffSourceFile = m_data->m_sourceFileNamePath;
            m_curDiffDestFile   = m_data->m_destFileNamePath;
        }
        if (m_data && (column == 0) && (ViewImage))
        {
            QFileInfo fileinfo(m_data->m_destFileNamePath);
            QFile     file(m_data->m_destFileNamePath);
            actViewImageDiff->setEnabled(file.exists());
            m_CurrentCompressedImageItem = item;
            QFile fileInfo(m_data->m_destFileNamePath);
            m_data->m_FileSize = fileInfo.size();

            if (m_data->m_FileSize > 1024000)
                m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024000, 'f', 2) + " MB";
            else if (m_data->m_FileSize > 1024)
                m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024, 'f', 1) + " KB";
            else
                m_data->m_FileSizeStr = QString().number(m_data->m_FileSize) + " Bytes";

            QFile  SourcefileInfo(m_data->m_sourceFileNamePath);
            qint64 SourceImageSize = m_data->m_SourceImageSize;

            if ((m_data->m_FileSize > 0) && (SourceImageSize > 0))
            {
                double CompressionRatio = SourceImageSize / (double)m_data->m_FileSize;
                char   buffer[128];
                sprintf(buffer, "%2.2f", CompressionRatio);
                m_data->m_CompressionRatio = QString("%1 to 1").arg(buffer);
            }

            if (file.exists() && (column == 0) && (m_clicked_onIcon))
            {
                m_CurrentCompressedImageItem = item;
                emit OnDecompressImage();
#ifdef _WIN32
                Sleep(100);
#else
                usleep(100000);
#endif
                emit ViewImageFile(m_data->m_destFileNamePath, item);
            }
        }
        // Update the compression data poperty view for the item clicked
        SignalUpdateData(item, islevelType);
    }
    else if (islevelType == TREETYPE_3DMODEL_DATA)
    {
        m_CurrentCompressedImageItem = NULL;

        if (column == 2)
        {
            item->setExpanded(true);
            if (item->text(2).compare("...") == 0)
            {
                item->setText(2, "(-)");
                item->setToolTip(2, "Hide model image files");
                if (item->childCount() > 0)
                {
                    for (int r = 0; r < item->childCount(); r++)
                    {
                        QTreeWidgetItem* child = item->child(r);
                        if (GetItemType(child) == TREETYPE_VIEWIMAGE_ONLY_NODE || GetItemType(child) == TREETYPE_VIEWMESH_ONLY_NODE)
                        {
                            child->setHidden(false);
                        }
                    }
                }
            }
            else
            {
                item->setText(2, "...");
                item->setToolTip(2, "Show model image files");
                if (item->childCount() > 0)
                {
                    for (int r = 0; r < item->childCount(); r++)
                    {
                        QTreeWidgetItem* child = item->child(r);
                        if (GetItemType(child) == TREETYPE_VIEWIMAGE_ONLY_NODE || GetItemType(child) == TREETYPE_VIEWMESH_ONLY_NODE)
                        {
                            child->setHidden(true);
                        }
                    }
                }
            }
        }
        else
        {
            m_CurrentCompressedImageItem = item;
            QVariant        v            = item->data(TREE_SourceInfo, Qt::UserRole);
            C_3DModel_Info* m_data       = v.value<C_3DModel_Info*>();
            if (m_data && (column == 0) && (ViewImage))
            {
                QString text = m_data->m_Full_Path;
                emit    ViewImageFile(text, item);
            }
            Tree_selectAllChildItems(item);
            // Update the image poperty view for the item clicked
            SignalUpdateData(item, islevelType);
        }
    }
    else if (islevelType == TREETYPE_3DSUBMODEL_DATA)
    {
        m_CurrentCompressedImageItem = item;
        QVariant           v         = item->data(TREE_SourceInfo, Qt::UserRole);
        C_3DSubModel_Info* m_data    = v.value<C_3DSubModel_Info*>();
        if (m_data && (column == 0) && (ViewImage))
        {
            QString text = m_data->m_Full_Path;
            emit    ViewImageFile(text, item);
        }
        // Update the image poperty view for the item clicked
        SignalUpdateData(item, islevelType);
    }
    else if (islevelType == TREETYPE_MESH_DATA)
    {
        m_CurrentCompressedImageItem = item;

        // view image
        QVariant               v      = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Destination_Options* m_data = v.value<C_Destination_Options*>();
        // Since user has clicked on this item : prepair image diff file incase user want to do a diff
        // using the main apps tool bar
        if (m_data)
        {
            m_curDiffSourceFile = m_data->m_sourceFileNamePath;
            m_curDiffDestFile   = m_data->m_destFileNamePath;
            if (column == 0)
            {
                QFileInfo fileinfo(m_data->m_destFileNamePath);
                QFile     file(m_data->m_destFileNamePath);
                actViewImageDiff->setEnabled(file.exists());
                m_CurrentCompressedImageItem = item;
                QFile fileInfo(m_data->m_destFileNamePath);
                m_data->m_FileSize = fileInfo.size();

                if (m_data->m_FileSize > 1024000)
                    m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024000, 'f', 2) + " MB";
                else if (m_data->m_FileSize > 1024)
                    m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024, 'f', 1) + " KB";
                else
                    m_data->m_FileSizeStr = QString().number(m_data->m_FileSize) + " Bytes";

                QFile  SourcefileInfo(m_data->m_sourceFileNamePath);
                qint64 SourceImageSize = m_data->m_SourceImageSize;

                if ((m_data->m_FileSize > 0) && (SourceImageSize > 0))
                {
                    double CompressionRatio = SourceImageSize / (double)m_data->m_FileSize;
                    char   buffer[128];
                    sprintf(buffer, "%2.2f", CompressionRatio);
                    m_data->m_CompressionRatio = QString("%1 to 1").arg(buffer);
                }

                if ((column == 0) && (m_clicked_onIcon))
                {
#ifdef USE_MESH_DRACO_EXTENSION
                    QFile compfile(m_data->m_destFileNamePath + ".drc");
                    if ((m_data->getDo_Mesh_Compression() == m_data->Draco) && (compfile.exists()))
                    {
                        QString fileName = compfile.fileName();
                        emit    ViewImageFile(fileName, item);
                    }
                    else
#endif
                        if (file.exists())
                    {
                        emit ViewImageFile(m_data->m_destFileNamePath, item);
                    }
                }
            }
        }

        // Update the image poperty view for the item clicked
        SignalUpdateData(item, islevelType);
    }
    else
    {
        m_CurrentCompressedImageItem = NULL;
        emit UpdateData(NULL);
    }
}

void ProjectView::onTree_ItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    if (!item)
        return;

    Q_UNUSED(column);
    QVariant v = item->data(TREE_LevelType, Qt::UserRole);

    switch (v.toInt())
    {
    case TREETYPE_Double_Click_here_to_add_files: {  // [+] Double Click here to add files
        // Clears of selected items when user clicks on this node
        Tree_clearAllItemsSelected();

        // Add new file
        // then Update the poperty view for the item clicked
        if (OpenImageFile())
        {
            SignalUpdateData(item, TREETYPE_IMAGEFILE_DATA);
        }
    }
    break;
    case TREETYPE_Add_destination_setting:           // [+] Add Destination Setting
    case TREETYPE_Add_Model_destination_settings: {  // STR_AddDestinationSetting
        emit AddCompressSettings(item);
    }
    break;
    }  // switch case
}

void ProjectView::onTreeMousePress(QMouseEvent* event, bool onIcon)
{
    m_CurrentItem    = m_projectTreeView->m_currentItem;
    m_clicked_onIcon = onIcon;
    if (m_CurrentItem)
    {
        //bool SHIFT_Key = event->modifiers() & Qt::ShiftModifier;
        bool CTRL_key = event->modifiers() & Qt::ControlModifier;
        //bool MouseLeft = event->button() & Qt::LeftButton;

        if (CTRL_key)
        {
            bool selected   = m_CurrentItem->isSelected();
            int  childCount = m_CurrentItem->childCount();
            if (childCount > 1)
            {
                QTreeWidgetItem* childItem;
                // Vaild Index start from 0 to childCount
                while (childCount > 1)
                {
                    childItem = m_CurrentItem->child(childCount - 1);
                    if (childItem)
                        childItem->setSelected(selected);
                    childCount--;
                }
            }
        }
    }
}

void ProjectView::onTreeKeyPress(QKeyEvent* event)
{
    Q_UNUSED(event);
    m_CurrentItem = m_projectTreeView->m_currentItem;
}

void ProjectView::onImageLoadStart()
{
    if (actAnalyseMeshData)
        actAnalyseMeshData->setEnabled(false);
    if (actCompressProjectFiles)
        actCompressProjectFiles->setEnabled(false);
    if (actRemoveImage)
        actRemoveImage->setEnabled(false);
}

void ProjectView::onImageLoadDone()
{
    if (actAnalyseMeshData)
        actAnalyseMeshData->setEnabled(true);
    if (actCompressProjectFiles)
        actCompressProjectFiles->setEnabled(true);
    if (actRemoveImage)
        actRemoveImage->setEnabled(true);
}

void ProjectView::onSetCurrentItem(QString& FilePathName)
{
    SelectImageItem(FilePathName);
}

void ProjectView::onCustomContextMenu(const QPoint& point)
{
    QModelIndex index = m_projectTreeView->indexAt(point);

    // Show or Hide thes Context menu items based on location in the Project View
    actOpenContainingFolder->setVisible(false);
    actCopyFullPath->setVisible(false);
#ifdef USE_CONTEXT_PROJECT
    actsaveProjectFile->setVisible(false);
    actopenProjectFile->setVisible(false);
#endif
    actAnalyseMeshData->setVisible(false);
    actCompressProjectFiles->setVisible(false);
    actViewImageDiff->setVisible(false);
    actView3DModelDiff->setVisible(false);
    actRemoveImage->setVisible(false);
    ContextMenu_ImageItem = NULL;

    if (index.isValid())
    {
        QString text;
        text = "Process ";

        // Get the item user right clicked on
        QTreeWidgetItem* item = m_projectTreeView->itemAt(point);
        if (item)
        {
            actViewImageDiff->setEnabled(false);
            actView3DModelDiff->setEnabled(false);

            // Get number of setected Items
            QList<QTreeWidgetItem*> selected = m_projectTreeView->selectedItems();

            int numCompressItems = GetNumDestItems();

            if (numCompressItems == 0)
                actCompressProjectFiles->setVisible(false);
            else
                actCompressProjectFiles->setVisible(true);

            int numSelected = GetNumSelectedDestItems();

            if (numSelected > 1)
            {
                text.append(QString::number(numSelected));
                text.append(" selected image(s)");
                actCompressProjectFiles->setText(text);
            }

            int levelType = GetItemType(item);
            switch (levelType)
            {
            case TREETYPE_Double_Click_here_to_add_files:    // [+] Add Image item
            case TREETYPE_Add_destination_setting:           // [+] Add destination setting
            case TREETYPE_Add_Model_destination_settings: {  // [+] Add glTF destination settings
                if (numSelected > 1)
                {
                    actCompressProjectFiles->setText("Process selected images");
                }
                else
                    actCompressProjectFiles->setVisible(false);
                break;
            }

            case TREETYPE_3DMODEL_DATA: {
#ifdef USE_CONTEXT_PROJECT
                actsaveProjectFile->setVisible(true);
                actopenProjectFile->setVisible(true);
#endif
                actAnalyseMeshData->setVisible(true);
                actOpenContainingFolder->setVisible(true);
                actCopyFullPath->setVisible(true);
                ContextMenu_ImageItem  = item;
                QVariant        fv     = item->data(TREE_SourceInfo, Qt::UserRole);
                C_3DModel_Info* m_data = fv.value<C_3DModel_Info*>();

                if ((numCompressItems > 0) && (item->childCount() > 1))
                {
                    if (m_data && (numSelected < (item->childCount() - 1)))
                    {
                        text = "Process all setting for ";
                        text.append(m_data->m_Name);
                        actCompressProjectFiles->setText(text);
                        Tree_selectAllChildItems(item);
                    }
                    actCompressProjectFiles->setVisible(true);
                }
                break;
            }
            case TREETYPE_3DSUBMODEL_DATA: {
#ifdef USE_CONTEXT_PROJECT
                actsaveProjectFile->setVisible(true);
                actopenProjectFile->setVisible(true);
#endif
                actOpenContainingFolder->setVisible(true);
                actCopyFullPath->setVisible(true);
                ContextMenu_ImageItem     = item;
                QVariant           fv     = item->data(TREE_SourceInfo, Qt::UserRole);
                C_3DSubModel_Info* m_data = fv.value<C_3DSubModel_Info*>();

                if (m_data)
                {
                    QFileInfo fileinfo(m_data->m_Full_Path);
                    QFile     file(m_data->m_Full_Path);
                    bool      fileexist = file.exists();
                    if ((m_data->ModelType == eModelType::GLTF) &&
                        (g_Application_Options.m_GLTFRenderWith != C_Application_Options::RenderModelsWith::glTF_Vulkan))
                    {
                        actView3DModelDiff->setVisible(fileexist);
                        actView3DModelDiff->setEnabled(fileexist);
                    }
                    else
                    {
                        actView3DModelDiff->setVisible(false);
                    }
                    actAnalyseMeshData->setVisible(fileexist);
                    m_CurrentCompressedImageItem = item;
                }

                if ((numCompressItems > 0) && (item->childCount() > 1))
                {
                    if (m_data && (numSelected < (item->childCount() - 1)))
                    {
                        text = "Process all setting for ";
                        text.append(m_data->m_Name);
                        actCompressProjectFiles->setText(text);
                        Tree_selectAllChildItems(item);
                    }
                    actCompressProjectFiles->setVisible(true);
                }
                break;
            }
            case TREETYPE_IMAGEFILE_DATA: {  // Original Image item
#ifdef USE_CONTEXT_PROJECT
                actsaveProjectFile->setVisible(true);
                actopenProjectFile->setVisible(true);
#endif
                actOpenContainingFolder->setVisible(true);
                actCopyFullPath->setVisible(true);
                ContextMenu_ImageItem = item;
                QVariant       fv     = item->data(TREE_SourceInfo, Qt::UserRole);
                C_Source_Info* m_data = fv.value<C_Source_Info*>();

                if ((numCompressItems > 0) && (item->childCount() > 1))
                {
                    if (m_data && (numSelected < (item->childCount() - 1)))
                    {
                        text = "Process all setting for ";
                        text.append(m_data->m_Name);
                        actCompressProjectFiles->setText(text);
                        Tree_selectAllChildItems(item);
                    }
                    actCompressProjectFiles->setVisible(true);
                }
                break;
            }
            case TREETYPE_MESH_DATA:
            case TREETYPE_COMPRESSION_DATA: {  // Compress Image item
                if (numCompressItems > 0)
                {
                    actOpenContainingFolder->setVisible(true);
                    actCopyFullPath->setVisible(true);
                    m_CurrentCompressedImageItem  = item;
                    ContextMenu_ImageItem         = item;
                    QVariant               cv     = item->data(TREE_SourceInfo, Qt::UserRole);
                    C_Destination_Options* m_data = cv.value<C_Destination_Options*>();
                    if (m_data)
                    {
                        // Enable 2D image diff for images
                        if (levelType == TREETYPE_COMPRESSION_DATA)
                        {
                            QFileInfo fileinfo(m_data->m_destFileNamePath);
                            QFile     file(m_data->m_destFileNamePath);
                            bool      fileexist = file.exists();

                            actViewImageDiff->setVisible(fileexist);
                            actViewImageDiff->setEnabled(fileexist);
                        }
                    }
                    if (m_data)
                    {
                        text.append(m_data->m_compname);
                        actCompressProjectFiles->setText(text);
                    }
                    actCompressProjectFiles->setVisible(true);
                    actRemoveImage->setVisible(true);
                }
                break;
            }
            }
            m_contextMenu->exec(m_projectTreeView->mapToGlobal(point));
        }
    }
}

//=====================================================
// This gets call many times (for all qDebug messages and printf's)
void ProjectView::OnGlobalMessage(const char* msg)
{
    if (m_CompressStatusDialog && g_bCompressing)
    {
        m_CompressStatusDialog->appendText(msg);
    }
}

void ProjectView::onContextMenuCompression()
{
    m_processFromContext = true;
    if (m_CompressStatusDialog)
    {
        g_bAbortCompression = false;
        m_CompressStatusDialog->onClearText();
        m_CompressStatusDialog->showOutput();
        CompressSelectedItems();
    }
}

void ProjectView::OnCloseCompression()
{
    if (m_CompressStatusDialog)
    {
        g_bAbortCompression = true;
        m_CompressStatusDialog->hideOutput();
    }
}

void ProjectView::onEntered(const QModelIndex& index)
{
    Q_UNUSED(index);
}

void ProjectView::onDroppedImageItem(QString& filePathName, int index)
{
    C_Source_Info* m_dataout = NULL;
    Tree_AddImageFile(filePathName, index, &m_dataout);
    m_saveProjectChanges = true;
}

void ProjectView::onGlobalPropertyChanged(int& setting)
{
    QTreeWidgetItem* item = this->m_CurrentItem;
    if (item)
    {
        QVariant v         = item->data(TREE_LevelType, Qt::UserRole);
        int      levelType = v.toInt();
        if (levelType == TREETYPE_Double_Click_here_to_add_files)
        {
            QColor ColorEnabled(128, 128, 0);     // Light Yellow
            QColor ColorDisabled(255, 255, 255);  // White
            if (setting > 0)
                item->setBackgroundColor(0, ColorEnabled);
            else
                item->setBackgroundColor(0, ColorDisabled);
        }
    }
}

void ProjectView::CompressSelectedItems()
{
    if (m_CompressStatusDialog)
    {
        saveProjectFile();
        compressProjectFiles(NULL);
        if (m_AllItemsSelected)
        {
            // Reset the list of selections
            Tree_clearAllItemsSelected();
            m_AllItemsSelected = false;
        }
    }
}

/*=====================================================*/
// Project Tree list layout
//
// TREETYPE_Double_Click_here_to_add_files
// TREETYPE_IMAGEFILE_DATA
//    TREETYPE_Add_destination_setting
//    TREETYPE_COMPRESSION_DATA
// TREETYPE_3DMODEL_DATA
//    TREETYPE_VIEWIMAGE_ONLY_NODE
//    TREETYPE_Add_glTF_destination_settings
//    TREETYPE_3DSUBMODEL_DATA
//       TREETYPE_Add_destination_setting
//       TREETYPE_COMPRESSION_DATA
/*=====================================================*/

struct TAnalysisData
{
    int    processCount;
    double processTime;
    double SSIM_Total;
    double PSNR_Total;
};

bool processItem(QFile*                  file,
                 ProjectView*            ProjectView,
                 QString                 FilePathName,
                 int                     miplevels,
                 MipSet*                 sourceImageMipSet,
                 int&                    NumberOfItemCompressed,
                 int&                    NumberOfItemCompressedFailed,
                 int&                    NumberOfItemsSkipped,
                 TAnalysisData&          m_AnalaysisData,
                 QTreeWidgetItemIterator it)
{
    // Use STD vectors to hold argv ** and keep the data in scope
    typedef std::vector<char>      CharArray;
    typedef std::vector<CharArray> ArgumentVector;
    ArgumentVector                 argvVec;
    std::vector<char*>             argv;
    C_Destination_Options          setDefaultOptions;

    if (*it)
    {
        argvVec.clear();
        argv.clear();

        // Push App name string
        std::string app = "compressonatorcli.exe";
        argvVec.push_back(CharArray(app.begin(), app.end()));
        argvVec.back().push_back(0);  // Terminate String
        argv.push_back(argvVec.back().data());

        // Push string
        std::string SourceFile = FilePathName.toStdString();
        argvVec.push_back(CharArray(SourceFile.begin(), SourceFile.end()));
        argvVec.back().push_back(0);  // Terminate String
        argv.push_back(argvVec.back().data());

        float            fqualty_setting = 0.05f;
        QTreeWidgetItem* Imageitem       = (*it);
        QString          Setting         = Imageitem->text(0);
        QVariant         v               = (*it)->data(TREE_LevelType, Qt::UserRole);
        int              sublevelType    = v.toInt();
        // save the settings item
        if (sublevelType == TREETYPE_COMPRESSION_DATA)
        {
            v                               = Imageitem->data(TREE_SourceInfo, Qt::UserRole);
            C_Destination_Options* data     = v.value<C_Destination_Options*>();
            bool                   testitem = false;

            //Compression setting and item is checked
            if (ProjectView->m_EnableCheckedItemsView)
            {
                //qDebug() << "override checked item using m_EnableCheckedItemsView";
                testitem = (Imageitem->checkState(0) == Qt::Checked);
            }
            else
            {
                //qDebug() << "override checked item is selected";
                testitem = Imageitem->isSelected();
            }

            if (data)
            {
                if (!testitem)
                    testitem = data->m_isselected;
            }

            //qDebug() << "testitem " << testitem;
            if (data && testitem)
            {
                //qDebug() << " valid Data";
                // Reset force compression use flag
                data->m_isselected = false;

                // Since we are now compressing
                // Indicate that the resulting files data
                // has been changed through compression
                // so that image view will reload a new image
                // This flag is also set when compression data has been edited
                data->m_data_has_been_changed = true;

                // make compression format and file extension compatible
                makeFormatExtCompatible(data);

                //"Destination" = data->m_destFileNamePath
                std::string DestinationFile = data->m_destFileNamePath.toStdString();
                QString     msgCommandLine;
                bool        useWeightChannel = false;
                bool        useAlphaChannel  = false;
                QFileInfo   fileInfo(data->m_destFileNamePath);
                QDir        dir(fileInfo.absoluteDir());
                QString     DestPath = dir.absolutePath();
                if (!dir.exists())
                {
                    dir.mkpath(".");
                }

                msgCommandLine = "<b>Processing: ";
                msgCommandLine.append(data->m_compname);
                msgCommandLine.append("<\b>");
                g_pProgressDlg->SetLabelText(msgCommandLine);
                g_pProgressDlg->SetValue(0);

                if (ProjectView->m_CompressStatusDialog)
                {
                    ProjectView->m_CompressStatusDialog->appendText(msgCommandLine);
                }

                // Saving paramaters to Batch file
                if (file != NULL)
                {
                    msgCommandLine = "";
                }
                else
                {
                    // Compressing
                    msgCommandLine = "compressonatorcli.exe  ";
                }

                // Check that the paths dont contain spaces else we need to add quotes
                // to the paths, this ensures path is ok for cut and paste if user decided
                // to use this for a command line exec or outputs to a batch file
                QString addqstr = " ";
                if (QString(data->m_sourceFileNamePath).contains(" ") || QString(data->m_destFileNamePath).contains(" "))
                    addqstr = "\"";

                msgCommandLine.append(addqstr);
                msgCommandLine.append(data->m_sourceFileNamePath);
                msgCommandLine.append(addqstr);
                msgCommandLine.append(" ");
                msgCommandLine.append(addqstr);
                msgCommandLine.append(data->m_destFileNamePath);
                msgCommandLine.append(addqstr);
                msgCommandLine.append(" ");

                argvVec.push_back(CharArray(DestinationFile.begin(), DestinationFile.end()));
                argvVec.back().push_back(0);  // Terminate String
                argv.push_back(argvVec.back().data());

                //"-fd Format"
                QMetaObject meta = C_Destination_Options::staticMetaObject;
                const char* key  = NULL;

                int       indexCompression    = meta.indexOfEnumerator("eCompression_options");
                QMetaEnum metaEnumCompression = meta.enumerator(indexCompression);
                key                           = metaEnumCompression.valueToKey(data->m_Compression);
                CMP_FORMAT cmp_format         = CMP_ParseFormat((char*)key);

                //"fd" = key
                if (key != NULL)
                {
                    std::string format = "-fd";
                    argvVec.push_back(CharArray(format.begin(), format.end()));
                    argvVec.back().push_back(0);  // Terminate String
                    argv.push_back(argvVec.back().data());
                    std::string formatValue = key;

                    msgCommandLine.append(" -fd ");
                    msgCommandLine.append(key);
                    msgCommandLine.append(" ");

                    argvVec.push_back(CharArray(formatValue.begin(), formatValue.end()));
                    argvVec.back().push_back(0);  // Terminate String
                    argv.push_back(argvVec.back().data());
                }
                else
                {
                    // do some check!
                }

                ////using GPU to compress
                if ((!g_useCPUEncode) && (key))
                {
                    std::string format = key;
                    //                     if (
                    // #ifdef USE_GTC
                    //                        format == "GTC" ||
                    // #endif
                    // #ifdef USE_BASIS
                    //                        format == "BASIS" ||
                    // #endif
                    //                         format == "BC1"   ||
                    //                         format == "BC2"   ||
                    //                         format == "BC3"   ||
                    //                         format == "BC4"   ||
                    //                         format == "BC5"   ||
                    //                         format == "BC7"   ||
                    //                         format == "DXT1"  ||
                    //                         format == "DXT3"  ||
                    //                         format == "DXT5"  ||
                    //                         format == "BC6H_SF"  ||
                    //                         format == "BC6H")
                    {
                        std::string usegpu;
                        msgCommandLine.append(" -EncodeWith ");
                        usegpu = "-EncodeWith";
                        argvVec.push_back(CharArray(usegpu.begin(), usegpu.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                        if (g_Application_Options.m_ImageEncode == C_Application_Options::ImageEncodeWith::HPC)
                        {
                            msgCommandLine.append(" HPC ");
                            usegpu = "HPC";
                            argvVec.push_back(CharArray(usegpu.begin(), usegpu.end()));
                            argvVec.back().push_back(0);  // Terminate String
                            argv.push_back(argvVec.back().data());
                        }
                        else if (g_Application_Options.m_ImageEncode == C_Application_Options::ImageEncodeWith::GPU_OpenCL)
                        {
                            msgCommandLine.append(" OCL ");
                            usegpu = "OCL";
                            argvVec.push_back(CharArray(usegpu.begin(), usegpu.end()));
                            argvVec.back().push_back(0);  // Terminate String
                            argv.push_back(argvVec.back().data());
                        }
                        else if (g_Application_Options.m_ImageEncode == C_Application_Options::ImageEncodeWith::GPU_HW)
                        {
                            msgCommandLine.append(" GPU ");
                            usegpu = "GPU";
                            argvVec.push_back(CharArray(usegpu.begin(), usegpu.end()));
                            argvVec.back().push_back(0);  // Terminate String
                            argv.push_back(argvVec.back().data());
                        }
#ifdef USE_GPU_PIPELINE_VULKAN
                        else if (g_Application_Options.m_ImageEncode == C_Application_Options::ImageEncodeWith::VLK)
                        {
                            msgCommandLine.append(" VLK ");
                            usegpu = "VLK";
                            argvVec.push_back(CharArray(usegpu.begin(), usegpu.end()));
                            argvVec.back().push_back(0);  // Terminate String
                            argv.push_back(argvVec.back().data());
                        }
#endif
                        else if (g_Application_Options.m_ImageEncode == C_Application_Options::ImageEncodeWith::GPU_DirectX)
                        {
                            msgCommandLine.append(" DXC ");
                            usegpu = "DXC";
                            argvVec.push_back(CharArray(usegpu.begin(), usegpu.end()));
                            argvVec.back().push_back(0);  // Terminate String
                            argv.push_back(argvVec.back().data());
                        }
                    }
                    //else
                    //{
                    //    g_useCPUEncode = true;
                    //}
                }

                //=====================================================
                // User set generate MipMap using GPU HW
                //=====================================================
                if (g_Application_Options.m_useGPUMipMaps && (g_Application_Options.m_ImageEncode == C_Application_Options::ImageEncodeWith::GPU_HW))
                {
                    std::string usepram;
                    msgCommandLine.append(" -GenGPUMipMaps ");
                    usepram = "-GenGPUMipMaps";
                    argvVec.push_back(CharArray(usepram.begin(), usepram.end()));
                    argvVec.back().push_back(0);  // Terminate String
                    argv.push_back(argvVec.back().data());
                }
                if (g_Application_Options.m_useSRGBFrames && (g_Application_Options.m_ImageEncode == C_Application_Options::ImageEncodeWith::GPU_HW))
                {
                    std::string usepram;
                    msgCommandLine.append(" -UseSRGBFrames ");
                    usepram = "-UseSRGBFrames";
                    argvVec.push_back(CharArray(usepram.begin(), usepram.end()));
                    argvVec.back().push_back(0);
                    argv.push_back(argvVec.back().data());
                }

                // MipLevels
                if (miplevels > 1)
                {
                    msgCommandLine.append(" -miplevels ");
                    msgCommandLine.append(QString::number(miplevels));
                    msgCommandLine.append(" ");

                    std::string smiplevel = "-miplevels";
                    argvVec.push_back(CharArray(smiplevel.begin(), smiplevel.end()));
                    argvVec.back().push_back(0);  // Terminate String
                    argv.push_back(argvVec.back().data());

                    smiplevel = std::to_string(miplevels);
                    argvVec.push_back(CharArray(smiplevel.begin(), smiplevel.end()));
                    argvVec.back().push_back(0);  // Terminate String
                    argv.push_back(argvVec.back().data());
                }

                //=============================
                // Quality Settings
                //=============================
                if (FormatSupportsQualitySetting(cmp_format))
                {
                    if ((data->m_Quality != setDefaultOptions.m_Quality) || (ProjectView->m_globalProcessSetting.m_GlobalSettingEnabled))
                    {
                        // Override the setting
                        if (ProjectView->m_globalProcessSetting.m_GlobalSettingEnabled)
                            fqualty_setting = ProjectView->m_globalProcessSetting.m_Quality;
                        else
                            fqualty_setting = data->m_Quality;
                        // User Msg
                        QString value = QString::number(fqualty_setting, 'f', 4);
                        msgCommandLine.append(" -Quality ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string squality = "-Quality";
                        argvVec.push_back(CharArray(squality.begin(), squality.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        squality = value.toStdString();
                        argvVec.push_back(CharArray(squality.begin(), squality.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }
                }

                //=====================================================
                // User set Number of threads that is not default 8
                //=====================================================
                if (g_Application_Options.m_threads != 0)
                {
                    // Display Msg to user on the process message pannel
                    QString value = QString::number(g_Application_Options.m_threads);
                    msgCommandLine.append(" -NumThreads ");
                    msgCommandLine.append(value);
                    msgCommandLine.append(" ");
                }

                //=====================================================
                // User set Refine BCn option
                if (FormatSupportsDXTCBase(cmp_format))
                {
                    //=============================
                    // Channel Weighting
                    //=============================
                    int CHRed      = ceil(data->X_RED * 100);
                    int DefCHRed   = ceil(setDefaultOptions.X_RED * 100);
                    int CHGreen    = ceil(data->Y_GREEN * 100);
                    int DefCHGreen = ceil(setDefaultOptions.Y_GREEN * 100);
                    int CHBlue     = ceil(data->Z_BLUE * 100);
                    int DefCHBlue  = ceil(setDefaultOptions.Z_BLUE * 100);

                    if ((!useWeightChannel) && ((CHRed != DefCHRed) || (CHGreen != DefCHGreen) || (CHBlue != DefCHBlue)))
                    {
                        msgCommandLine.append(" -UseChannelWeighting 1 ");
                        useWeightChannel = true;

                        // User Setting Text
                        std::string suseweighChannel = "-UseChannelWeighting";
                        argvVec.push_back(CharArray(suseweighChannel.begin(), suseweighChannel.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                        // User Setting Value
                        suseweighChannel = "1";
                        argvVec.push_back(CharArray(suseweighChannel.begin(), suseweighChannel.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }

                    if (CHRed != DefCHRed)
                    {
                        // User Msg
                        QString value = QString::number(data->X_RED, 'f', 4);

                        msgCommandLine.append(" -WeightR ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string sweightr = "-WeightR";
                        argvVec.push_back(CharArray(sweightr.begin(), sweightr.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        sweightr = value.toStdString();
                        argvVec.push_back(CharArray(sweightr.begin(), sweightr.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }

                    if (CHGreen != DefCHGreen)
                    {
                        // User Msg
                        QString value = QString::number(data->Y_GREEN, 'f', 4);

                        msgCommandLine.append(" -WeightG ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string sweightg = "-WeightG";
                        argvVec.push_back(CharArray(sweightg.begin(), sweightg.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        sweightg = value.toStdString();
                        argvVec.push_back(CharArray(sweightg.begin(), sweightg.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }

                    if (CHBlue != DefCHBlue)
                    {
                        // User Msg
                        QString value = QString::number(data->Z_BLUE, 'f', 4);

                        msgCommandLine.append(" -WeightB ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string sweightb = "-WeightB";
                        argvVec.push_back(CharArray(sweightb.begin(), sweightb.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        sweightb = value.toStdString();
                        argvVec.push_back(CharArray(sweightb.begin(), sweightb.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }
                }

                // ====================================
                // DXTC1 settings only
                // ====================================
                if ((cmp_format == CMP_FORMAT_DXT1) || (cmp_format == CMP_FORMAT_BC1))
                {
                    if (data->Threshold > 0)
                    {
                        // User Msg
                        QString value = QString::number(data->Threshold);
                        if (!useAlphaChannel)
                        {
                            msgCommandLine.append(" -DXT1UseAlpha 1 ");
                            useAlphaChannel = true;

                            // User Setting Text
                            std::string susealphaChannel = "-DXT1UseAlpha";
                            argvVec.push_back(CharArray(susealphaChannel.begin(), susealphaChannel.end()));
                            argvVec.back().push_back(0);  // Terminate String
                            argv.push_back(argvVec.back().data());
                            // User Setting Value
                            susealphaChannel = "1";
                            argvVec.push_back(CharArray(susealphaChannel.begin(), susealphaChannel.end()));
                            argvVec.back().push_back(0);  // Terminate String
                            argv.push_back(argvVec.back().data());
                        }

                        msgCommandLine.append(" -AlphaThreshold ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string sthreshold = "-AlphaThreshold";
                        argvVec.push_back(CharArray(sthreshold.begin(), sthreshold.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        sthreshold = value.toStdString();
                        argvVec.push_back(CharArray(sthreshold.begin(), sthreshold.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        //g_CmdPrams.CompressOptions.bDXT1UseAlpha = true;
                        //g_CmdPrams.CompressOptions.nAlphaThreshold = data->Threshold;
                    }

                    if ((data->Refine_Steps > 0) ||
                        (ProjectView->m_globalProcessSetting.m_GlobalSettingEnabled && (ProjectView->m_globalProcessSetting.m_Refine_Steps > 0)))
                    {
                        // User Msg
                        QString value;
                        if (ProjectView->m_globalProcessSetting.m_Refine_Steps > 0)
                            value = QString::number(ProjectView->m_globalProcessSetting.m_Refine_Steps);
                        else
                            value = QString::number(data->Refine_Steps);
                        msgCommandLine.append(" -RefineSteps ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string strSetText = "-RefineSteps";
                        argvVec.push_back(CharArray(strSetText.begin(), strSetText.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        strSetText = value.toStdString();
                        argvVec.push_back(CharArray(strSetText.begin(), strSetText.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }
                }

                // ====================================
                // Bitrate Settings
                // ====================================

                if (
#ifdef USE_GTC
                    (cmp_format == CMP_FORMAT_GTC) ||
#endif
#if (OPTION_BUILD_ASTC == 1)
                    (cmp_format == CMP_FORMAT_ASTC) ||
#endif
#ifdef USE_APC
                    (cmp_format == CMP_FORMAT_APC) ||
#endif
                    (cmp_format == CMP_FORMAT_BROTLIG))
                {
                    if (data->m_Bitrate != setDefaultOptions.m_Bitrate)
                    {
                        // User Msg
                        //QString value = data->m_correctBitrate;
                        QString value = data->m_Bitrate;
                        msgCommandLine.append(" -BlockRate ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string sbitrate = "-BlockRate";
                        argvVec.push_back(CharArray(sbitrate.begin(), sbitrate.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        sbitrate = value.toStdString();  //  std::to_string(data->m_Bitrate);
                        argvVec.push_back(CharArray(sbitrate.begin(), sbitrate.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }
                }

                // ==========================================
                // Input HDR Settings for Float->Byte process
                // ==========================================
                if (data->m_SourceIsFloatFormat && !(CMP_IsFloatFormat(cmp_format)))
                {
                    if (data->m_Defog != setDefaultOptions.m_Defog)
                    {
                        // User Msg
                        QString value = QString::number(data->m_Defog, 'f', 4);
                        msgCommandLine.append(" -InDefog ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string sdefog = "-InDefog";
                        argvVec.push_back(CharArray(sdefog.begin(), sdefog.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        sdefog = value.toStdString();
                        argvVec.push_back(CharArray(sdefog.begin(), sdefog.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }

                    if (data->m_Exposure != setDefaultOptions.m_Exposure)
                    {
                        // User Msg
                        QString value = QString::number(data->m_Exposure, 'f', 4);
                        msgCommandLine.append(" -InExposure ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string sexposure = "-InExposure";
                        argvVec.push_back(CharArray(sexposure.begin(), sexposure.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        sexposure = value.toStdString();
                        argvVec.push_back(CharArray(sexposure.begin(), sexposure.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }

                    if (data->m_KneeLow != setDefaultOptions.m_KneeLow)
                    {
                        // User Msg
                        QString value = QString::number(data->m_KneeLow, 'f', 4);
                        msgCommandLine.append(" -InKneeLow ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string skneelow = "-InKneeLow";
                        argvVec.push_back(CharArray(skneelow.begin(), skneelow.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        skneelow = value.toStdString();
                        argvVec.push_back(CharArray(skneelow.begin(), skneelow.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }

                    if (data->m_KneeHigh != setDefaultOptions.m_KneeHigh)
                    {
                        // User Msg
                        QString value = QString::number(data->m_KneeHigh, 'f', 4);
                        msgCommandLine.append(" -InKneeHigh ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string skneehigh = "-InKneeHigh";
                        argvVec.push_back(CharArray(skneehigh.begin(), skneehigh.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        skneehigh = value.toStdString();
                        argvVec.push_back(CharArray(skneehigh.begin(), skneehigh.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }

                    if (data->m_Gamma != setDefaultOptions.m_Gamma)
                    {
                        // User Msg
                        QString value = QString::number(data->m_Gamma, 'f', 4);
                        msgCommandLine.append(" -Gamma ");
                        msgCommandLine.append(value);
                        msgCommandLine.append(" ");

                        // User Setting Text
                        std::string sgamma = "-Gamma";
                        argvVec.push_back(CharArray(sgamma.begin(), sgamma.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());

                        // User Setting Value
                        sgamma = value.toStdString();
                        argvVec.push_back(CharArray(sgamma.begin(), sgamma.end()));
                        argvVec.back().push_back(0);  // Terminate String
                        argv.push_back(argvVec.back().data());
                    }
                }

                //===========================
                // Exporting to Batch file
                //===========================
                if (file != NULL)
                {
                    if (msgCommandLine.length() > 0)
                    {
                        file->write("compressonatorcli.exe ");
                        file->write(msgCommandLine.toLatin1());
                        file->write("\n");
                    }

                    if (ProjectView->m_CompressStatusDialog)
                    {
                        ProjectView->m_CompressStatusDialog->appendText(" Done");
                    }
                }
                else
                {
                    //======================
                    // Compression Data
                    //======================

                    //Show Command line to user
                    ProjectView->m_CompressStatusDialog->appendText(msgCommandLine);
                    QString msg = "File: " + data->m_compname;

                    g_pProgressDlg->SetLabelText(msg);

                    emit ProjectView->OnProcessing(data->m_destFileNamePath);
                    // Pass over the command line params
                    if (ParseParams((int)argv.size(), (CMP_CHAR**)argv.data()))
                    {
                        // Overriding Some Command Line Features for GUI app!
                        g_CmdPrams.showperformance               = true;
                        g_CmdPrams.conversion_fDuration          = 0;
                        g_CmdPrams.doDecompress                  = false;
                        g_CmdPrams.CompressOptions.dwnumThreads  = g_Application_Options.m_threads;
                        g_CmdPrams.CompressOptions.getPerfStats  = true;
                        g_CmdPrams.CompressOptions.getDeviceInfo = true;

                        if (g_Application_Options.m_logresults)
                        {
                            g_CmdPrams.logresults       = true;
                            g_CmdPrams.logresultsToFile = false;
                            g_CmdPrams.SSIM             = 0;
                        }
                        else
                        {
                            g_CmdPrams.logresults       = false;
                            g_CmdPrams.logresultsToFile = false;
                        }

                        // Do the Compression by loading a new MIP set
                        if (ProcessCMDLine(&ProgressCallback, sourceImageMipSet) == 0)
                        {
                            if (g_bAbortCompression)
                            {
                                Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallgraystone.png")));
                                g_pProgressDlg->SetValue(0);
                                argvVec.clear();
                                argv.clear();
                                return false;
                            }
                            else
                            {
                                // Success in compression
                                // CLI may redirect the output (e.g. read only install folder as cwd), so
                                // sync the tree data to the actual file it wrote before checking for it.
                                if (!g_CmdPrams.DestFile.empty())
                                    data->m_destFileNamePath = QString::fromStdString(g_CmdPrams.DestFile);

                                if (ProjectView->Tree_updateCompressIcon(Imageitem, data->m_destFileNamePath, true))
                                {
                                    // Destination File Size
                                    QFile fileInfo(data->m_destFileNamePath);
                                    data->m_FileSize = fileInfo.size();
                                    if (data->m_FileSize > 1024000)
                                        data->m_FileSizeStr = QString().number((double)data->m_FileSize / 1024000, 'f', 2) + " MB";
                                    else if (data->m_FileSize > 1024)
                                        data->m_FileSizeStr = QString().number((double)data->m_FileSize / 1024, 'f', 1) + " KB";
                                    else
                                        data->m_FileSizeStr = QString().number(data->m_FileSize) + " Bytes";
                                    // Add Compressoin Time
                                    if (g_CmdPrams.conversion_fDuration > 0)
                                    {
                                        data->m_CompressionTime = g_CmdPrams.conversion_fDuration;
                                        double CompressionRatio = data->m_SourceImageSize / (double)data->m_FileSize;
                                        char   buffer[128];
                                        sprintf(buffer, "%2.2f", CompressionRatio);
                                        data->m_CompressionRatio = QString("%1 to 1").arg(buffer);

                                        if (g_CmdPrams.conversion_fDuration < 60)
                                            data->m_CompressionTimeStr = QString().number((double)data->m_CompressionTime, 'f', 3) + " Sec";
                                        else if (g_CmdPrams.conversion_fDuration < 3600)
                                            data->m_CompressionTimeStr = QString().number((double)data->m_CompressionTime / 60, 'f', 2) + " Min";
                                        else
                                            data->m_CompressionTimeStr = QString().number((double)data->m_CompressionTime / 3600, 'f', 2) + " Hrs";
                                    }
                                    else
                                    {
                                        data->m_CompressionTime    = 0;
                                        data->m_CompressionTimeStr = DESTINATION_IMAGE_NOTPROCESSED;
                                    }

                                    NumberOfItemCompressed++;
                                    g_pProgressDlg->SetValue(0);

                                    if (g_Application_Options.m_logresults && (g_CmdPrams.SSIM > 0.0F))
                                    {
                                        m_AnalaysisData.processCount++;
                                        m_AnalaysisData.SSIM_Total += g_CmdPrams.SSIM;
                                        m_AnalaysisData.PSNR_Total += g_CmdPrams.PSNR;
                                        m_AnalaysisData.processTime += g_CmdPrams.compress_fDuration;
                                        if (g_Application_Options.m_analysisResultTable)
                                        {
                                            ProjectView->m_analysisTable.AddTestResults(g_CmdPrams.DestFile,
                                                                                        data->m_compname,
                                                                                        fqualty_setting,
                                                                                        g_CmdPrams.CompressOptions.perfStats.m_computeShaderElapsedMS,
                                                                                        g_CmdPrams.CompressOptions.perfStats.m_CmpMTxPerSec,
                                                                                        g_CmdPrams.compress_fDuration,
                                                                                        g_CmdPrams.PSNR,
                                                                                        g_CmdPrams.SSIM);
                                        }
                                    }

                                    argvVec.clear();
                                    argv.clear();
                                    return true;
                                }
                                else
                                {
                                    NumberOfItemCompressedFailed++;
                                    g_pProgressDlg->SetValue(0);

                                    argvVec.clear();
                                    argv.clear();
                                    return false;
                                }
                            }
                        }
                        else
                        {
                            NumberOfItemCompressedFailed++;
                            Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallredstone.png")));
                            g_pProgressDlg->SetValue(0);

                            argvVec.clear();
                            argv.clear();
                            return false;
                        }
                    }

                    g_pProgressDlg->SetValue(0);

                    argvVec.clear();
                    argv.clear();
                }
            }
        }
        else
        {
            if (Imageitem->isSelected())
                NumberOfItemsSkipped++;
        }

        return true;
    }
    else
    {
        return false;
    }
}

void replaceExt(std::string& s, const std::string& newExt)
{
    std::string::size_type i = s.rfind('.', s.length());

    if (i != std::string::npos)
    {
        s.replace(i + 1, newExt.length(), newExt);
    }
}

AnalysisTableWidget::AnalysisTableWidget()
{
    Qt::WindowFlags flags = windowFlags();
    setWindowFlags(flags | Qt::WindowStaysOnTopHint);  // can use Qt::WindowTitleHint to remove close button
    setWindowTitle("Analysis");
    setStyleSheet("QHeaderView::section { background-color:lightgrey;}");  // font: bold 14px
    setColumnCount(8);
    QStringList header;
    header << "File Path"
           << "File Name"
           << "Quality"
           << "KPerf(ms)"
           << "MTx/s"
           << "Time(s)"
           << "PSNR(dB)"
           << "SSIM";
    setHorizontalHeaderLabels(header);
    setColumnWidth(0, 150);  // Path
    setColumnWidth(1, 150);  // Filename
    setColumnWidth(2, 55);   // Quality
    setColumnWidth(3, 64);   // PerfTime
    setColumnWidth(4, 64);   // MTx per sec
    setColumnWidth(5, 64);   // Time
    setColumnWidth(6, 75);   // PSNR
    setColumnWidth(7, 50);   // SSIM
    setMinimumWidth(696);
}

void AnalysisTableWidget::keyPressEvent(QKeyEvent* event)
{
    // selected cells
    if (!selectedIndexes().isEmpty())
    {
        if (event->matches(QKeySequence::Copy))
        {
            QString             text;
            QItemSelectionRange range = selectionModel()->selection().first();
            for (auto i = range.top(); i <= range.bottom(); ++i)
            {
                QStringList rowContents;
                for (auto j = range.left(); j <= range.right(); ++j)
                    rowContents << model()->index(i, j).data().toString();
                text += rowContents.join("\t");
                text += "\n";
            }
            QApplication::clipboard()->setText(text);
        }
        else
            QTableView::keyPressEvent(event);
    }
}

void AnalysisTableWidget::AddAverageResults(QString deviceName, QString processName, QString Time, QString psnr, QString ssim)
{
    int rowCount = this->rowCount();
    this->insertRow(rowCount);
    setWindowTitle("Analysis Encode with " + processName + " " + deviceName);
    processName = "Average " + processName;
    setItem(rowCount, 0, new QTableWidgetItem(" "));
    setItem(rowCount, 1, new QTableWidgetItem(processName));
    setItem(rowCount, 2, new QTableWidgetItem(" "));
    setItem(rowCount, 3, new QTableWidgetItem(" "));
    setItem(rowCount, 4, new QTableWidgetItem(" "));
    setItem(rowCount, 5, new QTableWidgetItem(Time));
    setItem(rowCount, 6, new QTableWidgetItem(psnr));
    setItem(rowCount, 7, new QTableWidgetItem(ssim));
}

void AnalysisTableWidget::AddTestResults(std::string processPath,
                                         QString     processName,
                                         float       Quality,
                                         double      PerfTime,
                                         double      MPxPerSec,
                                         double      Time,
                                         double      psnr,
                                         double      ssim)
{
    QString str_time;
    QString str_perftime;
    QString str_Mpx;
    QString str_psnr;
    QString str_ssim;
    QString str_quality;

    str_quality.sprintf("%01.2f", Quality);
    str_perftime.sprintf("%03.3f", PerfTime * 1000);
    str_Mpx.sprintf("%03.3f", MPxPerSec);
    str_time.sprintf("%03.3f", Time);
    str_psnr.sprintf("%03.2f", psnr);
    str_ssim.sprintf("%01.4f", ssim);

    int rowCount = this->rowCount();
    this->insertRow(rowCount);
    setItem(rowCount, 0, new QTableWidgetItem(QString(processPath.c_str())));
    setItem(rowCount, 1, new QTableWidgetItem(processName));
    setItem(rowCount, 2, new QTableWidgetItem(str_quality));
    setItem(rowCount, 3, new QTableWidgetItem(str_perftime));
    setItem(rowCount, 4, new QTableWidgetItem(str_Mpx));
    setItem(rowCount, 5, new QTableWidgetItem(str_time));
    setItem(rowCount, 6, new QTableWidgetItem(str_psnr));
    setItem(rowCount, 7, new QTableWidgetItem(str_ssim));
}

void AnalysisTableWidget::ClearResults()
{
    this->setRowCount(0);
}

void CompressFiles(QFile* file, ProjectView* ProjectView)
{
    // if ((file == NULL) || (ProjectView == NULL))
    //     return;

    struct Image_Data
    {
        QString                 FilePathName;
        QTreeWidgetItemIterator it;
    };

    TAnalysisData m_AnalaysisData = {0};

    if (g_Application_Options.m_analysisResultTable && g_Application_Options.m_logresults)
    {
        ProjectView->m_analysisTable.ClearResults();
        ProjectView->m_analysisTable.show();
    }
    else
    {
        ProjectView->m_analysisTable.hide();
    }

    ProjectView->m_CompressStatusDialog->showOutput();

    if (g_pProgressDlg == NULL)
        return;

    g_pProgressDlg->SetValue(0);
    g_pProgressDlg->SetHeader("Processing");
    g_pProgressDlg->SetLabelText("");
    g_pProgressDlg->show();

    // Use STD vectors to hold argv ** and keep the data in scope
    typedef std::vector<char>      CharArray;
    typedef std::vector<CharArray> ArgumentVector;
    ArgumentVector                 argvVec;
    std::vector<char*>             argv;

    g_bCompressing = true;

    int     NumberOfItemCompressed       = 0;
    int     NumberOfItemCompressedFailed = 0;
    int     NumberOfItemsSkipped         = 0;
    int     childcount                   = 0;
    MipSet* sourceImageMipSet;

    // Parse the Project view tree
    QTreeWidgetItemIterator it(ProjectView->m_projectTreeView);

    // QTreeWidgetItem *it_IMAGEFILE;
    // QTreeWidgetItem *it_3DMODEL;
    // QTreeWidgetItem *it_3DSUBMODEL;

    double conversion_loopStartTime = timeStampsec();

    while (*it)
    {
        QString name = (*it)->text(0);
        childcount   = (*it)->childCount();

        switch (GetItemType(*it))
        {
        case TREETYPE_Double_Click_here_to_add_files:
            break;
        case TREETYPE_IMAGEFILE_DATA: {
            g_pProgressDlg->SetHeader("Processing: Image");
            //==========================================
            // Get Image Info and data
            //==========================================
            QVariant       v      = (*it)->data(TREE_SourceInfo, Qt::UserRole);
            C_Source_Info* m_data = v.value<C_Source_Info*>();
            if (m_data == NULL)
            {
                ++it;
                continue;
            }

            QString FilePathName;
            FilePathName      = m_data->m_Full_Path;
            int miplevels     = 0;
            sourceImageMipSet = NULL;
            //==========================================
            // TREETYPE_Add_destination_setting:
            // TREETYPE_COMPRESSION_DATA:
            //==========================================
            //std::string filename = FilePathName.toStdString();
            //printf("item:%s\n", filename.c_str());
            while (childcount > 0)
            {
                if (g_bAbortCompression)
                    break;
                it++;
                childcount--;

                v                                 = (*it)->data(TREE_SourceInfo, Qt::UserRole);
                C_Destination_Options* dst_option = v.value<C_Destination_Options*>();
                if (!dst_option)
                    continue;

                // filename = dst_option->m_destFileNamePath.toStdString();
                // printf("[%5s] child:%s \n", (*it)->isSelected() ? "TRUE" : "FALSE", filename.c_str());
                if (GetItemType(*it) == TREETYPE_COMPRESSION_DATA && (*it)->isSelected())
                {
                    if (m_data->m_MipImages)  // source
                    {
                        if (m_data->m_MipImages->mipset)
                        {
                            miplevels         = m_data->m_MipImages->mipset->m_nMipLevels;
                            sourceImageMipSet = m_data->m_MipImages->mipset;
                        }
                    }
                    processItem(file,
                                ProjectView,
                                FilePathName,
                                miplevels,
                                sourceImageMipSet,
                                NumberOfItemCompressed,
                                NumberOfItemCompressedFailed,
                                NumberOfItemsSkipped,
                                m_AnalaysisData,
                                it);
                }
            }
        }
        break;

        case TREETYPE_3DMODEL_DATA: {
            QVariant        v      = (*it)->data(TREE_SourceInfo, Qt::UserRole);
            C_3DModel_Info* m_data = v.value<C_3DModel_Info*>();
            if (m_data == NULL)
            {
                ++it;
                continue;
            }

            QString FilePathName;
            FilePathName = m_data->m_Full_Path;
            std::list<Image_Data> image_list;

            // we have  data in the model to process
            while (childcount > 0)
            {
                if (g_bAbortCompression)
                    break;
                it++;
                childcount--;
                switch (GetItemType(*it))
                {
                case TREETYPE_VIEWIMAGE_ONLY_NODE: {
                    QVariant       v      = (*it)->data(TREE_SourceInfo, Qt::UserRole);
                    C_Source_Info* m_data = v.value<C_Source_Info*>();
                    if (m_data)
                    {
                        QString SourceImage = m_data->m_Full_Path;
                        image_list.push_back({SourceImage, it});
                    }
                }
                break;
                case TREETYPE_Add_Model_destination_settings:
                    break;
                case TREETYPE_3DSUBMODEL_DATA:
                    int subchildcount = (*it)->childCount();
                    // TREETYPE_Add_destination_setting:
                    // TREETYPE_COMPRESSION_DATA:
                    while (subchildcount > 0)
                    {
                        if (g_bAbortCompression)
                            break;
                        it++;
                        subchildcount--;
                        if (GetItemType(*it) == TREETYPE_COMPRESSION_DATA)
                        {
                            v                           = (*it)->data(TREE_SourceInfo, Qt::UserRole);
                            C_Destination_Options* data = v.value<C_Destination_Options*>();

                            if (data)
                            {
                                QString sourceImage = data->m_sourceFileNamePath;
                                // find the source image data
                                std::list<Image_Data>::iterator items = image_list.begin();

                                while (items != image_list.end())
                                {
                                    if (items->FilePathName.compare(sourceImage, Qt::CaseInsensitive) == 0)
                                    {
                                        //==========================================
                                        // Get Image Info and data
                                        //==========================================

                                        QVariant       v      = (*(items->it))->data(TREE_SourceInfo, Qt::UserRole);
                                        C_Source_Info* m_data = v.value<C_Source_Info*>();
                                        if (m_data)
                                        {
                                            QString FilePathName;
                                            FilePathName      = m_data->m_Full_Path;
                                            int miplevels     = 0;
                                            sourceImageMipSet = NULL;

                                            if (m_data->m_MipImages)
                                            {
                                                if (m_data->m_MipImages->mipset)
                                                {
                                                    miplevels         = m_data->m_MipImages->mipset->m_nMipLevels;
                                                    sourceImageMipSet = m_data->m_MipImages->mipset;
                                                }
                                            }

                                            if (processItem(file,
                                                            ProjectView,
                                                            FilePathName,
                                                            miplevels,
                                                            sourceImageMipSet,
                                                            NumberOfItemCompressed,
                                                            NumberOfItemCompressedFailed,
                                                            NumberOfItemsSkipped,
                                                            m_AnalaysisData,
                                                            it))
                                            {
                                                UpdateDestglTFAfterProcess((*it));
                                            }
                                        }
                                        break;
                                    }
                                    items++;
                                }
                            }
                        }
                        else if (GetItemType(*it) == TREETYPE_MESH_DATA)
                        {
                            v                               = (*it)->data(TREE_SourceInfo, Qt::UserRole);
                            C_Destination_Options* data     = v.value<C_Destination_Options*>();
                            bool                   testitem = false;
                            //process setting and item is checked
                            if (ProjectView->m_EnableCheckedItemsView)
                            {
                                //override checked item using m_EnableCheckedItemsView
                                testitem = ((*it)->checkState(0) == Qt::Checked);
                            }
                            else
                            {
                                //override checked item is selected
                                testitem = (*it)->isSelected();
                            }

                            if (data)
                            {
                                if (!testitem)
                                    testitem = data->m_isselected;
                            }
                            if (data && testitem)
                            {
                                // Reset force processing use flag
                                data->m_isselected = false;
                                /*********************************************************/
                                /*        MESH OPTIMIZATION & COMPRESSION                */
                                /*********************************************************/

                                QString ModelSource      = data->m_modelSource;  // was data->m_sourceFileNamePath;
                                QString ModelDestination = data->m_modelDest;    // was data->m_destFileNamePath;
                                bool    bMeshOptimized   = false;

                                QTreeWidgetItem* Imageitem = (*it);
                                // Call mesh compressor codecs
                                QString msgCommandLine;
                                msgCommandLine = "<b>Processing: ";
                                msgCommandLine.append(data->m_compname);
                                msgCommandLine.append("<\b>");

                                g_pProgressDlg->SetLabelText(msgCommandLine);
                                g_pProgressDlg->SetValue(0);
                                g_pProgressDlg->show();

                                if (ProjectView->m_CompressStatusDialog)
                                {
                                    ProjectView->m_CompressStatusDialog->appendText(msgCommandLine);
                                }

                                /********************************/
                                /*    MESH OPZTIMIZATION        */
                                /********************************/

                                if (data->getDo_Mesh_Optimization() != data->NoOpt)
                                {
                                    msgCommandLine = "[Mesh Optimization] Src: " + ModelSource + " Dst: " + ModelDestination;
                                    g_pProgressDlg->SetHeader("Processing: Mesh Optimization");
                                    g_pProgressDlg->SetLabelText(msgCommandLine);
                                    if (ProjectView->m_CompressStatusDialog)
                                        ProjectView->m_CompressStatusDialog->appendText(msgCommandLine);

                                    PluginInterface_Mesh* plugin_Mesh = NULL;

                                    plugin_Mesh = reinterpret_cast<PluginInterface_Mesh*>(g_pluginManager.GetPlugin("MESH_OPTIMIZER", "TOOTLE_MESH"));

                                    if (plugin_Mesh)
                                    {
                                        if (plugin_Mesh->Init() == 0)
                                        {
                                            plugin_Mesh->TC_PluginSetSharedIO(g_GUI_CMIPS);

                                            MeshSettings uimeshsettings;
                                            uimeshsettings.bOptimizeOverdraw = data->getOptimizeOverdrawChecked();
                                            if (uimeshsettings.bOptimizeOverdraw)
                                                uimeshsettings.nOverdrawACMRthreshold = (float)data->getACMRThreshold();
                                            uimeshsettings.bOptimizeVCache = data->getOptimizeVCacheChecked();
                                            if (uimeshsettings.bOptimizeVCache)
                                                uimeshsettings.nCacheSize = data->getCacheSize();
                                            uimeshsettings.bOptimizeVCacheFifo = data->getOptimizeVCacheFifoChecked();
                                            if (uimeshsettings.bOptimizeVCacheFifo)
                                                uimeshsettings.nCacheSize = data->getCacheSizeFifo();
                                            uimeshsettings.bOptimizeVFetch = data->getOptimizeVFetchChecked();
                                            uimeshsettings.bRandomizeMesh  = data->getRandomIndexBufferChecked();
                                            uimeshsettings.bSimplifyMesh   = data->getMeshSimplifyChecked();
                                            if (uimeshsettings.bSimplifyMesh)
                                                uimeshsettings.nlevelofDetails = data->getLODValue();

                                            std::string SourceModelFileNamePath = ModelSource.toStdString();
                                            uimeshsettings.pMeshName            = SourceModelFileNamePath.c_str();

                                            std::string tempdst          = ModelDestination.toStdString();
                                            uimeshsettings.pDestMeshName = tempdst.c_str();

                                            void* modelDataOut = nullptr;
                                            void* modelDataIn  = nullptr;

                                            QFileInfo       fileInfo(ModelSource);
                                            QString         EXT      = fileInfo.suffix();
                                            QByteArray      ba       = EXT.toUpper().toLatin1();
                                            char*           c_ext    = ba.data();
                                            CMP_GLTFCommon* gltfdata = nullptr;

                                            PluginInterface_3DModel_Loader* m_plugin_loader;
                                            m_plugin_loader =
                                                reinterpret_cast<PluginInterface_3DModel_Loader*>(g_pluginManager.GetPlugin("3DMODEL_LOADER", c_ext));

                                            if (m_plugin_loader)
                                            {
                                                m_plugin_loader->TC_PluginSetSharedIO(g_GUI_CMIPS);
                                                void* msgHandler = NULL;
                                                if (ProjectView)
                                                {
                                                    cpMainComponents* mainComponents = NULL;
                                                    mainComponents                   = (cpMainComponents*)ProjectView->m_parent;
                                                    if (mainComponents)
                                                        msgHandler = (void*)mainComponents->PrintStatus;
                                                }

                                                int result;
                                                if (result = m_plugin_loader->LoadModelData(
                                                                 SourceModelFileNamePath.c_str(), "", &g_pluginManager, msgHandler, &ProgressCallback) != 0)
                                                {
                                                    if (result != 0)
                                                        throw("Error Loading Model Data");
                                                }

                                                if (strcmp(c_ext, "GLTF") == 0)
                                                {
                                                    gltfdata = (CMP_GLTFCommon*)m_plugin_loader->GetModelData();
                                                    if (gltfdata)
                                                    {
                                                        if (gltfdata->m_meshBufferData.m_meshData[0].vertices.size() > 0)
                                                            modelDataIn = (void*)&(gltfdata->m_meshBufferData);
                                                        else
                                                        {
                                                            modelDataIn = nullptr;
                                                            if (ProjectView->m_CompressStatusDialog)
                                                                ProjectView->m_CompressStatusDialog->appendText(
                                                                    "[Mesh Optimization] Error in processing mesh. Mesh data format size is not "
                                                                    "supported.");
                                                        }
                                                    }
                                                }
                                                else
                                                    modelDataIn = m_plugin_loader->GetModelData();
                                            }

                                            try
                                            {
                                                modelDataOut = plugin_Mesh->ProcessMesh(modelDataIn, (void*)&uimeshsettings, NULL, &ProgressCallback);
                                            }
                                            catch (std::exception& e)
                                            {
                                                if (ProjectView->m_CompressStatusDialog)
                                                    ProjectView->m_CompressStatusDialog->appendText("[Mesh Optimization] Error: " +
                                                                                                    QString::fromStdString(e.what()));
                                            }

                                            if (modelDataOut)
                                            {
                                                if (g_bAbortCompression)
                                                {
                                                    Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallgraystone.png")));
                                                    g_pProgressDlg->SetValue(0);
                                                }
                                                else
                                                {
                                                    NumberOfItemCompressed++;
                                                    if (ProjectView->m_CompressStatusDialog)
                                                        ProjectView->m_CompressStatusDialog->appendText("[Mesh Optimization] Done.");

                                                    std::vector<CMP_Mesh>* optimized       = ((std::vector<CMP_Mesh>*)modelDataOut);
                                                    QTreeWidgetItem*       subModel_parent = (*it)->parent();
                                                    if (subModel_parent && GetItemType(subModel_parent) == TREETYPE_3DSUBMODEL_DATA)
                                                    {
                                                        QVariant           v             = subModel_parent->data(TREE_SourceInfo, Qt::UserRole);
                                                        C_3DSubModel_Info* subModel_data = v.value<C_3DSubModel_Info*>();

                                                        if (subModel_data)
                                                        {
                                                            subModel_data->m_ModelData.m_meshData.resize(optimized->size());
                                                            for (unsigned int i = 0; i < optimized->size(); i++)
                                                            {
                                                                subModel_data->m_ModelData.m_meshData[i].vertices = (*optimized)[i].vertices;
                                                                subModel_data->m_ModelData.m_meshData[i].indices  = (*optimized)[i].indices;
                                                            }
                                                        }

                                                        //create a new copy of bin (with user specified name) and update the new .bin file to submodel (gltf case only)
                                                        if (strcmp(c_ext, "GLTF") == 0)
                                                        {
                                                            std::ifstream f(ModelSource.toStdString().data());
                                                            if (!f)
                                                            {
                                                                if (ProjectView->m_CompressStatusDialog)
                                                                    ProjectView->m_CompressStatusDialog->appendText(
                                                                        "[Mesh Optimization] Error in update destination gltf file.");
                                                            }

                                                            nlohmann::json j3;

                                                            f >> j3;
                                                            f.close();

                                                            auto buffers = j3["buffers"];

                                                            for (unsigned int i = 0; i < buffers.size(); i++)
                                                            {  //for now only support one buffer (one bin file only)
                                                                std::string name = buffers[i]["uri"].get<std::string>();
                                                                if (name.find(".bin") != std::string::npos)
                                                                {
                                                                    //retrieve original bin file and create a new copy of bin file
                                                                    QFileInfo srcFile(ModelSource);
                                                                    QString   oriBinFile = srcFile.absolutePath() + "/" + QString::fromStdString(name);
                                                                    if (QFile::exists(data->m_destFileNamePath))
                                                                    {
                                                                        bool removeOldcopy = QFile::remove(data->m_destFileNamePath);
                                                                        if (!removeOldcopy)
                                                                        {
                                                                            if (ProjectView->m_CompressStatusDialog)
                                                                                ProjectView->m_CompressStatusDialog->appendText(
                                                                                    "[Mesh Optimization] Error in update destination gltf bin file. "
                                                                                    "Please remove old copy and try again.");
                                                                        }
                                                                    }
                                                                    bool copied = QFile::copy(oriBinFile, data->m_destFileNamePath);
                                                                    if (!copied)
                                                                    {
                                                                        if (ProjectView->m_CompressStatusDialog)
                                                                            ProjectView->m_CompressStatusDialog->appendText(
                                                                                "[Mesh Optimization] Error in update destination gltf bin file.");
                                                                    }

                                                                    QFileInfo binFile(data->m_destFileNamePath);
                                                                    QString   binFileName   = binFile.fileName();
                                                                    j3["buffers"][i]["uri"] = binFileName.toStdString();
                                                                }
                                                                else
                                                                {
                                                                    if (ProjectView->m_CompressStatusDialog)
                                                                        ProjectView->m_CompressStatusDialog->appendText(
                                                                            "[Mesh Optimization] Error in update destination gltf file. embedded is "
                                                                            "not supported.");
                                                                }
                                                            }

                                                            std::ofstream fDest(ModelDestination.toStdString().data(), std::ios_base::out);
                                                            if (!fDest)
                                                            {
                                                                if (ProjectView->m_CompressStatusDialog)
                                                                    ProjectView->m_CompressStatusDialog->appendText(
                                                                        "[Mesh Optimization] Error in update destination gltf file.");
                                                            }

                                                            fDest << j3;
                                                            fDest.close();
                                                        }
                                                    }

                                                    PluginInterface_3DModel_Loader* plugin_save = NULL;
                                                    plugin_save =
                                                        reinterpret_cast<PluginInterface_3DModel_Loader*>(g_pluginManager.GetPlugin("3DMODEL_LOADER", c_ext));
                                                    if (plugin_save)
                                                    {
                                                        plugin_save->TC_PluginSetSharedIO(g_GUI_CMIPS);

                                                        int result = 0;
                                                        if (strcmp(c_ext, "GLTF") == 0)
                                                        {
                                                            if (gltfdata)
                                                            {
                                                                CMP_GLTFCommon optimizedGltf;
                                                                optimizedGltf.buffersData                 = gltfdata->buffersData;
                                                                optimizedGltf.isBinFile                   = gltfdata->isBinFile;
                                                                optimizedGltf.j3                          = gltfdata->j3;
                                                                optimizedGltf.m_CommonLoadTime            = gltfdata->m_CommonLoadTime;
                                                                optimizedGltf.m_distance                  = gltfdata->m_distance;
                                                                optimizedGltf.m_filename                  = gltfdata->m_filename;
                                                                optimizedGltf.m_meshes                    = gltfdata->m_meshes;
                                                                optimizedGltf.m_path                      = gltfdata->m_path;
                                                                optimizedGltf.m_scenes                    = gltfdata->m_scenes;
                                                                optimizedGltf.m_meshBufferData.m_meshData = *optimized;

                                                                if (plugin_save->SaveModelData(ModelDestination.toStdString().data(), &optimizedGltf) == -1)
                                                                {
                                                                    if (ProjectView->m_CompressStatusDialog)
                                                                        ProjectView->m_CompressStatusDialog->appendText(
                                                                            "[Mesh Optimization] Failed to save optimized gltf data.");
                                                                }
                                                            }
                                                            else
                                                            {
                                                                if (ProjectView->m_CompressStatusDialog)
                                                                    ProjectView->m_CompressStatusDialog->appendText(
                                                                        "[Mesh Optimization] Failed to save optimized gltf data.");
                                                            }
                                                        }
                                                        else
                                                        {
                                                            if (plugin_save->SaveModelData(ModelDestination.toStdString().data(), &((*optimized)[0])) != -1)
                                                            {
#ifdef _WIN32
                                                                if (!(writeObjFileState(ModelDestination.toStdString().data(), CMP_PROCESSED)))
                                                                {
                                                                    if (ProjectView->m_CompressStatusDialog)
                                                                        ProjectView->m_CompressStatusDialog->appendText(
                                                                            "[Mesh Optimization] Failed to save optimized obj data.");
                                                                }
#endif
                                                            }
                                                            else
                                                            {
                                                                if (ProjectView->m_CompressStatusDialog)
                                                                    ProjectView->m_CompressStatusDialog->appendText(
                                                                        "[Mesh Optimization] Failed to save optimized obj data.");
                                                            }
                                                        }

                                                        if (result != 0)
                                                        {
                                                            if (ProjectView->m_CompressStatusDialog)
                                                                ProjectView->m_CompressStatusDialog->appendText(
                                                                    "[Mesh Optimization] Error in saving mesh file.");
                                                            if (plugin_save)
                                                            {
                                                                delete plugin_save;
                                                                plugin_save = nullptr;
                                                            }
                                                        }
                                                        else
                                                        {
                                                            bMeshOptimized = true;
                                                            if (ProjectView->m_CompressStatusDialog)
                                                                ProjectView->m_CompressStatusDialog->appendText("[Mesh Optimization] Saving " +
                                                                                                                ModelDestination + " done.");
                                                        }
                                                    }
                                                    else
                                                    {
                                                        if (ProjectView->m_CompressStatusDialog)
                                                            ProjectView->m_CompressStatusDialog->appendText("[Mesh Optimization] File format not supported.");
                                                    }

                                                    plugin_Mesh->CleanUp();
                                                    if (plugin_save)
                                                    {
                                                        delete plugin_save;
                                                        plugin_save = nullptr;
                                                    }
                                                    g_pProgressDlg->SetValue(0);
                                                }

                                                // Update Icon if new file exists
                                                if (!ProjectView->Tree_updateCompressIcon(Imageitem, ModelDestination, true))
                                                {
                                                    NumberOfItemCompressedFailed++;
                                                    g_pProgressDlg->SetValue(0);
                                                }
                                                if (m_plugin_loader)
                                                {
                                                    delete m_plugin_loader;
                                                    m_plugin_loader = nullptr;
                                                }
                                            }
                                            else
                                            {
                                                if (ProjectView->m_CompressStatusDialog)
                                                    ProjectView->m_CompressStatusDialog->appendText("[Mesh Optimization] Error in processing mesh.");

                                                NumberOfItemCompressedFailed++;
                                                Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallredstone.png")));
                                                g_pProgressDlg->SetValue(0);
                                                plugin_Mesh->CleanUp();
                                            }
                                            //hideProgressDialog();
                                        }
                                        else
                                        {
                                            if (ProjectView->m_CompressStatusDialog)
                                                ProjectView->m_CompressStatusDialog->appendText("[Mesh Optimization] Error in init mesh plugin.");
                                        }
                                    }
                                    else
                                    {
                                        if (ProjectView->m_CompressStatusDialog)
                                            ProjectView->m_CompressStatusDialog->appendText("[Mesh Optimization] Error in loading mesh plugin.");
                                    }

                                    if (plugin_Mesh)
                                    {
                                        delete plugin_Mesh;
                                        plugin_Mesh = NULL;
                                    }
                                }

                                /********************************/
                                /*    MESH COMPRESSION          */
                                /********************************/
                                //check for supported file format first
                                QFileInfo  fileInfosrc(ModelSource);
                                QString    EXTsrc = fileInfosrc.suffix();
                                QByteArray ba     = EXTsrc.toUpper().toLatin1();
                                char*      c_ext  = ba.data();

                                QFileInfo  fileInfodst(ModelDestination);
                                QString    EXTdst   = fileInfodst.suffix();
                                QByteArray bdst     = EXTdst.toUpper().toLatin1();
                                char*      c_extdst = bdst.data();

                                //if (strcmp(c_ext, "OBJ") != 0 && strcmp(c_ext, "DRC") != 0)
                                //{
                                //    // if (ProjectView->m_CompressStatusDialog)
                                //    //     ProjectView->m_CompressStatusDialog->appendText("[Mesh Compression] Note: mesh compression is turned off. File format not supported");
                                //    data->setDo_Mesh_Compression(data->NoComp);
                                //}

                                if (data->getDo_Mesh_Compression() != data->NoComp)
                                {
                                    // Case: glTF -> glTF draco compression
                                    if (strcmp(c_ext, "GLTF") == 0 && strcmp(c_extdst, "GLTF") == 0)
                                    {
                                        std::string         err;
                                        tinygltf2::Model    model;
                                        tinygltf2::TinyGLTF loader;
                                        tinygltf2::TinyGLTF saver;

                                        //clean up draco mesh buffer
#ifdef USE_MESH_DRACO_EXTENSION
                                        model.dracomeshes.clear();
#endif
                                        std::string srcFile = ModelSource.toStdString();
                                        std::string dstFile = ModelDestination.toStdString();
                                        // Check if mesh optimization was done if so then source is optimized file
                                        if (bMeshOptimized)
                                        {
                                            srcFile = ModelDestination.toStdString();
                                            if (!(CMP_FileExists(srcFile)))
                                            {
                                                if (ProjectView->m_CompressStatusDialog)
                                                    ProjectView->m_CompressStatusDialog->appendText("Error: Source Model Mesh File is not found.\n");
                                                NumberOfItemCompressedFailed++;
                                                Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallredstone.png")));
                                                g_pProgressDlg->SetValue(0);
                                                return;
                                            }
                                            std::size_t dotPos = srcFile.rfind('.');
                                            dstFile            = srcFile.substr(0, dotPos) + "_drc.glTF";
                                        }

                                        bool ret = loader.LoadASCIIFromFile(&model, &err, srcFile, true);
                                        if (!err.empty())
                                        {
                                            if (ProjectView->m_CompressStatusDialog)
                                                ProjectView->m_CompressStatusDialog->appendText(
                                                    "Error processing gltf source:" + QString::fromStdString(srcFile) + " file. " +
                                                    QString::fromStdString(err) + "\n");
                                            NumberOfItemCompressedFailed++;
                                            Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallredstone.png")));
                                            g_pProgressDlg->SetValue(0);
                                            return;
                                        }
                                        if (!ret)
                                        {
                                            if (ProjectView->m_CompressStatusDialog)
                                                ProjectView->m_CompressStatusDialog->appendText("Failed in loading glTF file : " +
                                                                                                QString::fromStdString(srcFile));
                                            NumberOfItemCompressedFailed++;
                                            Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallredstone.png")));
                                            g_pProgressDlg->SetValue(0);
                                            return;
                                        }
                                        else
                                        {
                                            if (ProjectView->m_CompressStatusDialog)
                                                ProjectView->m_CompressStatusDialog->appendText("Success in loading glTF file : " +
                                                                                                QString::fromStdString(srcFile));
                                        }

                                        bool is_draco_src = false;
#ifdef USE_MESH_DRACO_EXTENSION
                                        if (model.dracomeshes.size() > 0)
                                        {
                                            is_draco_src = true;
                                        }
#endif
                                        err.clear();

                                        CMP_CompressOptions CompressOptions;
                                        CompressOptions.iCmpLevel = data->getCompression_Level();
                                        ;
                                        CompressOptions.iPosBits     = data->getPosition_Bits();
                                        CompressOptions.iTexCBits    = data->getTex_Coords_Bits();
                                        CompressOptions.iNormalBits  = data->getNormals_Bits();
                                        CompressOptions.iGenericBits = data->getGeneric_Bits();

                                        ret = saver.WriteGltfSceneToFile(&model, &err, dstFile, CompressOptions, is_draco_src, true);

                                        if (!err.empty())
                                        {
                                            if (ProjectView->m_CompressStatusDialog)
                                                ProjectView->m_CompressStatusDialog->appendText(
                                                    "Error saving gltf destination:" + QString::fromStdString(dstFile) + " file. " +
                                                    QString::fromStdString(err) + "\n");
                                            NumberOfItemCompressedFailed++;
                                            Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallredstone.png")));
                                            g_pProgressDlg->SetValue(0);
                                            return;
                                        }
                                        if (!ret)
                                        {
                                            if (ProjectView->m_CompressStatusDialog)
                                                ProjectView->m_CompressStatusDialog->appendText("Failed to save glTF file " + QString::fromStdString(dstFile) +
                                                                                                "\n");
                                            NumberOfItemCompressedFailed++;
                                            Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallredstone.png")));
                                            g_pProgressDlg->SetValue(0);
                                            return;
                                        }
                                        else
                                        {
                                            PrintInfo("Success in writting glTF file : [%s].\n", dstFile.c_str());
                                        }

                                        if (g_bAbortCompression)
                                        {
                                            Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallgraystone.png")));
                                            g_pProgressDlg->SetValue(0);
                                        }
                                        else
                                        {
                                            NumberOfItemCompressed++;
                                            if (ProjectView->m_CompressStatusDialog)
                                                ProjectView->m_CompressStatusDialog->appendText("[Mesh Compression] Done.");
                                            g_pProgressDlg->SetValue(0);
                                        }

                                        // Update Icon if new file exists
                                        if (!ProjectView->Tree_updateCompressIcon(Imageitem, ModelDestination, true))
                                        {
                                            NumberOfItemCompressedFailed++;
                                            g_pProgressDlg->SetValue(0);
                                        }
                                    }
                                    //case obj->obj will produce obj->drc draco compression
#if (LIB_BUILD_MESHCOMPRESSOR)
                                    else
                                    {
                                        PluginInterface_3DModel_Loader* m_plugin_loader_drc = NULL;

                                        PluginInterface_Mesh* plugin_MeshComp;
                                        plugin_MeshComp = reinterpret_cast<PluginInterface_Mesh*>(g_pluginManager.GetPlugin("MESH_COMPRESSOR", "DRACO"));

                                        if (plugin_MeshComp)
                                        {
                                            if (plugin_MeshComp->Init() == 0)
                                            {
                                                //showProgressDialog("Process Mesh Data");
                                                plugin_MeshComp->TC_PluginSetSharedIO(g_GUI_CMIPS);

                                                CMP_DracoOptions DracoOptions;
                                                DracoOptions.is_point_cloud               = data->getForce_Input_as_Point_Cloud();
                                                DracoOptions.use_metadata                 = data->getUse_Metadata();
                                                DracoOptions.compression_level            = data->getCompression_Level();
                                                DracoOptions.pos_quantization_bits        = data->getPosition_Bits();
                                                DracoOptions.tex_coords_quantization_bits = data->getTex_Coords_Bits();
                                                DracoOptions.normals_quantization_bits    = data->getNormals_Bits();
                                                DracoOptions.generic_quantization_bits    = data->getGeneric_Bits();

                                                // Check if mesh optimization was done if so then source is optimized file
                                                if (bMeshOptimized)
                                                    DracoOptions.input = ModelDestination.toStdString();
                                                else
                                                    DracoOptions.input = ModelSource.toStdString();

                                                DracoOptions.output = ModelDestination.toStdString() + ".drc";

                                                msgCommandLine = "[Mesh Compression] Src: " + QString(DracoOptions.input.c_str()) +
                                                                 " Dst: " + QString(DracoOptions.output.c_str());
                                                g_pProgressDlg->SetHeader("Processing: Mesh Compression");
                                                g_pProgressDlg->SetLabelText(msgCommandLine);
                                                if (ProjectView->m_CompressStatusDialog)
                                                    ProjectView->m_CompressStatusDialog->appendText(msgCommandLine);

                                                void* modelDataOut = nullptr;
                                                void* modelDataIn  = nullptr;

                                                PluginInterface_3DModel_Loader* m_plugin_loader_drc;
                                                m_plugin_loader_drc =
                                                    reinterpret_cast<PluginInterface_3DModel_Loader*>(g_pluginManager.GetPlugin("3DMODEL_LOADER", "DRC"));

                                                if (m_plugin_loader_drc)
                                                {
                                                    m_plugin_loader_drc->TC_PluginSetSharedIO(g_GUI_CMIPS);
                                                    void* msgHandler = NULL;
                                                    if (ProjectView)
                                                    {
                                                        cpMainComponents* mainComponents = NULL;
                                                        mainComponents                   = (cpMainComponents*)ProjectView->m_parent;
                                                        if (mainComponents)
                                                            msgHandler = (void*)mainComponents->PrintStatus;
                                                    }

                                                    int result;
                                                    if (result = m_plugin_loader_drc->LoadModelData(
                                                                     "OBJ", NULL, &g_pluginManager, &DracoOptions, &ProgressCallback) != 0)
                                                    {
                                                        if (result != 0)
                                                        {
                                                            if (ProjectView->m_CompressStatusDialog)
                                                                ProjectView->m_CompressStatusDialog->appendText("[Mesh Compression] Error Loading Model Data");
                                                            return;
                                                        }
                                                    }

                                                    modelDataIn = m_plugin_loader_drc->GetModelData();

                                                    try
                                                    {
                                                        if (modelDataIn)
                                                            modelDataOut =
                                                                plugin_MeshComp->ProcessMesh(modelDataIn, (void*)&DracoOptions, NULL, &ProgressCallback);
                                                    }
                                                    catch (std::exception& e)
                                                    {
                                                        if (ProjectView->m_CompressStatusDialog)
                                                            ProjectView->m_CompressStatusDialog->appendText(
                                                                "[Mesh Compression] Error: " + QString::fromStdString(e.what()) +
                                                                ". Please try another setting.");
                                                    }

                                                    if (modelDataOut)
                                                    {
                                                        if (g_bAbortCompression)
                                                        {
                                                            Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallgraystone.png")));
                                                            g_pProgressDlg->SetValue(0);
                                                        }
                                                        else
                                                        {
                                                            NumberOfItemCompressed++;
                                                            if (ProjectView->m_CompressStatusDialog)
                                                                ProjectView->m_CompressStatusDialog->appendText("[Mesh Compression] Done.");
                                                            g_pProgressDlg->SetValue(0);
                                                        }

                                                        // Update Icon if new file exists
                                                        if (!ProjectView->Tree_updateCompressIcon(Imageitem, QString(DracoOptions.output.c_str()), true))
                                                        {
                                                            NumberOfItemCompressedFailed++;
                                                            g_pProgressDlg->SetValue(0);
                                                        }
                                                    }
                                                    else
                                                    {
                                                        if (ProjectView->m_CompressStatusDialog)
                                                            ProjectView->m_CompressStatusDialog->appendText("[Mesh Compression] Error in processing mesh.");

                                                        NumberOfItemCompressedFailed++;
                                                        Imageitem->setIcon(0, QIcon(QStringLiteral(":/compressonatorgui/images/smallredstone.png")));
                                                        g_pProgressDlg->SetValue(0);
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                if (ProjectView->m_CompressStatusDialog)
                                                    ProjectView->m_CompressStatusDialog->appendText("[Mesh Compression] Error in init mesh plugin.");
                                            }

                                            plugin_MeshComp->CleanUp();
                                        }
                                        else
                                        {
                                            if (ProjectView->m_CompressStatusDialog)
                                                ProjectView->m_CompressStatusDialog->appendText("[Mesh Compression] Error in loading mesh compression plugin.");
                                        }

                                        if (plugin_MeshComp)
                                        {
                                            delete plugin_MeshComp;
                                            plugin_MeshComp = nullptr;
                                        }
                                        if (m_plugin_loader_drc)
                                        {
                                            delete m_plugin_loader_drc;
                                            m_plugin_loader_drc = nullptr;
                                        }
                                    }
#endif
                                }  // end if Compressed Vertex Checked
                            }
                        }
                    }
                    break;
                }
            }
            image_list.clear();
        }
        break;
        }

        if (g_bAbortCompression)
            break;

        if (*it)
            ++it;
        else
            break;
    }

    g_bCompressing                    = false;
    ProjectView->m_processFromContext = false;

    if (ProjectView->m_CompressStatusDialog && (file == NULL) && (!g_bAbortCompression))
    {
        if ((NumberOfItemCompressed == 0) && (NumberOfItemCompressedFailed == 0))
        {
            QMessageBox msgBox;
            msgBox.setText("No valid image compression setting(s) are selected to process. Please refer to output window for details.");
            msgBox.exec();
            ProjectView->m_CompressStatusDialog->appendText(
                "No valid image compression setting(s) are selected. A compression setting window will pop up, if not,");
            ProjectView->m_CompressStatusDialog->appendText("try:");
            ProjectView->m_CompressStatusDialog->appendText("    - adding images to the project view by \"double click here to add files...\"");
            ProjectView->m_CompressStatusDialog->appendText(
                "      and set destination options by expand the tree view - clicking on the little arrow \">\" on the left side of the orignal "
                "image");
            ProjectView->m_CompressStatusDialog->appendText("      and double click on \"(+) Add destination settings...\"");
            ProjectView->m_CompressStatusDialog->appendText("    - Selecting images to process with Mouse click");
            ProjectView->m_CompressStatusDialog->appendText("      on destination images");
        }
        else
        {
            if (g_bAbortCompression)
                ProjectView->m_CompressStatusDialog->appendText("<b>Compression was canceled!</b>");
            else
            {
                QString MsgDuration;
                MsgDuration     = "Processed in ";
                double duration = timeStampsec() - conversion_loopStartTime;
                MsgDuration.append(QString::number(duration, 'g', 3));
                MsgDuration.append(" seconds");
                ProjectView->m_CompressStatusDialog->appendText(MsgDuration);

                QString Msg = "====== <b>Compress: ";
                Msg.append(QString::number(NumberOfItemCompressed));
                Msg.append(" succeeded, ");
                Msg.append(QString::number(NumberOfItemCompressedFailed));
                Msg.append(" failed");
                //if (NumberOfItemsSkipped > 0)
                //{
                //    Msg.append(", ");
                //    Msg.append(QString::number(NumberOfItemsSkipped));
                //    Msg.append(" skipped ");
                //}

                if (g_Application_Options.m_logresults && (m_AnalaysisData.processCount > 0))
                {
                    QString time;
                    QString psnr;
                    QString ssim;

                    time.sprintf("%03.3f", (m_AnalaysisData.processTime / m_AnalaysisData.processCount));
                    psnr.sprintf("%03.2f", (m_AnalaysisData.PSNR_Total / m_AnalaysisData.processCount));
                    ssim.sprintf("%01.4f", (m_AnalaysisData.SSIM_Total / m_AnalaysisData.processCount));

                    Msg.append(", Time ");
                    Msg.append(time);
                    Msg.append(", PSNR ");
                    Msg.append(psnr);
                    Msg.append(", SSIM ");
                    Msg.append(ssim);

                    QString EncodeWith;
                    QString DeviceName;

                    switch (g_Application_Options.m_ImageEncode)
                    {
                    case C_Application_Options::ImageEncodeWith::HPC:
                        // Check if last encodewith worked on GPU!
                        if (g_CmdPrams.CompressOptions.format_support_hostEncoder)
                            EncodeWith = "HPC";
                        else
                            EncodeWith = "CPU, HPC Failed";
                        break;
                    case C_Application_Options::ImageEncodeWith::GPU_DirectX:
                        if (g_CmdPrams.CompressOptions.format_support_hostEncoder)
                            EncodeWith = "DXC";
                        else
                            EncodeWith = "CPU, DXC Failed";
                        DeviceName = g_CmdPrams.CompressOptions.deviceInfo.m_deviceName;
                        break;
                    case C_Application_Options::ImageEncodeWith::GPU_OpenCL:
                        if (g_CmdPrams.CompressOptions.format_support_hostEncoder)
                            EncodeWith = "OCL";
                        else
                            EncodeWith = "CPU, OCL Failed";
                        DeviceName = g_CmdPrams.CompressOptions.deviceInfo.m_deviceName;
                        break;
                    case C_Application_Options::ImageEncodeWith::GPU_HW:
                        if (g_CmdPrams.CompressOptions.format_support_hostEncoder)
                            EncodeWith = "GPU";
                        else
                            EncodeWith = "CPU, GPU Failed";
                        DeviceName = g_CmdPrams.CompressOptions.deviceInfo.m_deviceName;
                        break;
                    default:
                        EncodeWith = "CPU";
                        break;
                    }

                    if (g_Application_Options.m_analysisResultTable)
                    {
                        ProjectView->m_analysisTable.AddAverageResults(DeviceName, EncodeWith, time, psnr, ssim);
                        ProjectView->m_analysisTable.show();
                    }
                }

                Msg.append("<\b> ======");
                ProjectView->m_CompressStatusDialog->appendText(Msg);
            }
        }
    }

    g_pProgressDlg->hide();

    g_bCompressing      = false;
    g_bAbortCompression = false;
}

void ProjectView::compressProjectFiles(QFile* file)
{
    if (m_CompressStatusDialog == NULL)
        return;

    if (g_pProgressDlg == NULL)
        return;

    g_bAbortCompression = false;

    emit OnCompressionStart();

    // Handle local options that must be disabled while compressing
    if (actAnalyseMeshData)
        actAnalyseMeshData->setEnabled(false);
    if (actCompressProjectFiles)
        actCompressProjectFiles->setEnabled(false);
    if (actRemoveImage)
        actRemoveImage->setEnabled(false);

    CompressFiles(file, this);
    emit OnCompressionDone();

    if (actAnalyseMeshData)
        actAnalyseMeshData->setEnabled(true);
    if (actCompressProjectFiles)
        actCompressProjectFiles->setEnabled(true);
    if (actRemoveImage)
        actRemoveImage->setEnabled(true);
}
