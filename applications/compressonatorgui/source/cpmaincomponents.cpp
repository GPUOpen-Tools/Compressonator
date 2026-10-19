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
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "cpmaincomponents.h"

#include "compressonator.h"
#include "common.h"
#include "version.h"
#include "cmp_fileio.h"

#include <gpu_decode.h>

#include <gltf/tiny_gltf2_utils.h>

#include <QtCore/qdir.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>

#include <chrono>
#include <thread>

namespace
{
// Resolves a directory verified writable for storing per-user settings.  The
// per-user AppLocalDataLocation is tried first; if it cannot be created or is not
// actually writable, a verified-writable temp directory is used instead of
// silently falling back to the process working directory, which may be the
// (read-only for standard users) install directory -- reintroducing the exact
// failure this per-user location was meant to avoid.
//
// Writability is probed via the shared CMP_IsDirWritable() (cmp_fileio.h), which creates
// its probe file with a unique, PID/tick-based name and CREATE_NEW so it can never collide
// with -- and truncate -- a pre-existing file in the target directory.
QString ResolveWritableSettingsDir()
{
    const QString appDataPath = QString::fromStdString(CMP_GetUserDataDir());
    if (!appDataPath.isEmpty() && CMP_IsDirWritable(appDataPath.toStdString()))
        return appDataPath;

    const QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (!tempPath.isEmpty() && QDir().mkpath(tempPath) && CMP_IsDirWritable(tempPath.toStdString()))
    {
        qWarning(
            "CompressonatorGUI: per-user settings directory '%s' is unavailable or not writable; "
            "falling back to verified-writable temp directory '%s'.",
            qPrintable(appDataPath),
            qPrintable(tempPath));
        return tempPath;
    }

    const QString cwd = QDir::currentPath();
    qWarning(
        "CompressonatorGUI: no writable per-user or temp directory was found; settings will be "
        "attempted at the current working directory '%s' and may not persist.",
        qPrintable(cwd));
    return cwd;
}
}  // namespace

static signalMsgHandler static_msghandler;
int                     g_OpenGLMajorVersion = 0;
int                     g_OpenGLMinorVersion = 0;

#define STR_WELCOME_PAGE "Welcome Page"

C_Application_Options g_Application_Options;
acProgressDlg*        g_pProgressDlg = NULL;
bool                  g_bCompressing = false;  // Set true when we are compressing project items
extern CMIPS*         g_GUI_CMIPS;

// Hooked onto Progress Dialog.
void OnCancel()
{
    g_bAbortCompression = true;
}

// Hooked into g_GUI_CMIPS
void Print_onProgressDialog(char* str)
{
    if (g_pProgressDlg)
    {
        if (!g_pProgressDlg->isVisible())
            g_pProgressDlg->show();

        g_pProgressDlg->SetLabelText(QString(str));
    }
    QCoreApplication::processEvents();
}

void Set_onProgressValue(unsigned int value, bool* canceled)
{
    if (g_pProgressDlg)
    {
        if (!g_pProgressDlg->isVisible())
            g_pProgressDlg->show();
        g_pProgressDlg->SetValue(value);
        if (g_pProgressDlg->WasCanceled())
        {
            g_pProgressDlg->reset();
            *canceled = true;
        }
    }
    QCoreApplication::processEvents();
}

QString getFileExt(QString filePathName)
{
    QFileInfo fileInfo(filePathName);
    QString   EXT = fileInfo.completeSuffix();
    return EXT;
}

//=========================================================
cpMainComponents::cpMainComponents(QDockWidget* root_dock, QMainWindow* parent)
    : QMainWindow(parent)
{
    if (parent == NULL)
        m_parent = this;
    else
        m_parent = parent;

    //============================================
    fileMenu           = NULL;
    helpMenu           = NULL;
    fileToolBar        = NULL;
    CompressionToolBar = NULL;
    newProjectAct      = NULL;
    saveToBatchFileAct = NULL;
    openAct            = NULL;
    saveAct            = NULL;
    saveAsAct          = NULL;
    saveImageAct       = NULL;
    openImageFileAct   = NULL;
    exportAct          = NULL;
    exitAct            = NULL;
    userGuideAct       = NULL;
    gettingStartedAct  = NULL;
    aboutAct           = NULL;
    settingsAct        = NULL;
    aboutQtAct         = NULL;
    compressAct        = NULL;
    imagediffAct       = NULL;
    MIPGenAct          = NULL;

    deleteImageAct       = NULL;
    closeAllDocuments    = NULL;
    showOutputAct        = NULL;
    showWelcomePageAct   = NULL;
    windowMenu           = NULL;
    isCompressInProgress = false;
    m_ForceImageRefresh  = false;

    // To test crash dump!
    // char *p = 0;
    // *p = 5;

#ifdef ENABLE_AGS_SUPPORT
    onHDRButton = NULL;
    // AGS specific
    m_bIsHDRAvailableOnPrimary = false;
    m_bIsFullScreenModeOn      = false;
    m_agsContext               = nullptr;
    m_DeviceIndex              = 0;
    m_DisplayIndex             = 0;
#endif

#ifdef USE_MAINA_IMAVEVIEW_TOOLBAR
    ImageViewToolBar       = NULL;
    imageview_zoomInAct    = NULL;
    imageview_zoomOutAct   = NULL;
    imageview_RedAct       = NULL;
    imageview_GreenAct     = NULL;
    imageview_BlueAct      = NULL;
    imageview_AlphaAct     = NULL;
    imageview_FitScreenAct = NULL;
#endif

#ifdef USE_3DCONVERT
    m_modelConvert  = NULL;
    ConvertModelAct = NULL;
#endif

    m_welcomePage        = NULL;
    m_imagePropertyView  = NULL;
    m_genmips            = NULL;
    m_setcompressoptions = NULL;
    m_projectview        = NULL;
    m_imageCompare       = NULL;
    m_activeImageTab     = NULL;
    app_welcomepage      = NULL;
    m_frame              = NULL;

    m_numRecentFiles = 0;
    m_projectsRecentFiles.clear();

    m_showAppSettingsDialog = true;
    m_viewDiff              = false;

    // Settings must be written to a per user writable location: the install
    // directory is read only for standard users.  ResolveWritableSettingsDir()
    // verifies actual writability rather than assuming the working directory
    // is usable if the per-user directory lookup or creation fails.
    {
        QString appDataPath = ResolveWritableSettingsDir();
        m_sSettingsFile     = QDir(appDataPath).absoluteFilePath("CompressSettings.ini");
    }

    if (m_showAppSettingsDialog)
    {
        m_setapplicationoptions = new CSetApplicationOptions("Application Settings", this);
        m_setapplicationoptions->resize(600, 500);
        connect(m_setapplicationoptions, SIGNAL(OnAppSettingHide()), this, SLOT(onWriteSettings()));
        m_setapplicationoptions->hide();
    }

    //============================================
    app_welcomepage = root_dock;

    m_parent->setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    //
    m_blankpage = new acCustomDockWidget("", this);
    m_blankpage->setTitleBarWidget(NULL);
    m_blankpage->m_fileName = "";
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    m_blankpage->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_blankpage->setMinimumHeight(400);
    m_parent->addDockWidget(Qt::RightDockWidgetArea, m_blankpage);
    if (m_blankpage->custTitleBar)
    {
        m_blankpage->custTitleBar->setButtonCloseEnabled(false);
        m_blankpage->custTitleBar->setButtonToolBarShow(false);
        m_blankpage->lower();
        //m_blankpage->setStyleSheet("QTabWidget::tab:disabled { width: 0; height: 0; margin: 0; padding: 0; border: none; }");
        // setTabEnabled(false);
    }

    // Status View when compressing
    m_CompressStatusDialog = new CompressStatusDialog("Output", this);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
    m_CompressStatusDialog->resize(300, 90);
    m_parent->addDockWidget(Qt::BottomDockWidgetArea, m_CompressStatusDialog);
    m_CompressStatusDialog->hideOutput();

    connect(&static_msghandler, SIGNAL(signalMessage(const char*)), this, SLOT(browserMsg(const char*)));

    m_projectview = new ProjectView("Project Explorer", m_CompressStatusDialog, this);
    m_projectview->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_projectview->resize(300, 500);
    m_projectview->setMaximumWidth(300);
    m_projectview->setMinimumWidth(200);
    m_parent->addDockWidget(Qt::LeftDockWidgetArea, m_projectview);

    //
    m_imagePropertyView = new CImagePropertyView("  Property View", this);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    m_imagePropertyView->resize(300, 290);
    m_parent->addDockWidget(Qt::LeftDockWidgetArea, m_imagePropertyView);

    m_welcomePage             = new CWelcomePage(STR_WELCOME_PAGE, this);
    m_welcomePage->m_fileName = "";
    m_parent->tabifyDockWidget(m_blankpage, m_welcomePage);
    m_welcomePage->resize(600, 400);
    m_welcomePage->setMinimumHeight(400);

    if (m_welcomePage->custTitleBar)
        m_welcomePage->custTitleBar->setTitle(STR_WELCOME_PAGE);
    m_welcomePage->setAllowedAreas(Qt::RightDockWidgetArea);
    connect(m_welcomePage, SIGNAL(WebPageButtonClick(QString&, QString&)), this, SLOT(OnWelcomePageButtonClick(QString&, QString&)));

    // Get the blank page tab and set disable property to hide it
    QTabBar* tabBar = this->findChild<QTabBar*>();
    if (tabBar)
    {
        tabBar->setStyleSheet("QTabBar::tab:disabled { width: 0; height: 0; margin: 0; padding: 0; border: none; }");
        tabBar->setTabEnabled(0, false);
    }

    m_setcompressoptions = new CSetCompressOptions("Destination Setting", this);
    m_setcompressoptions->hide();

    connect(m_projectview, SIGNAL(ViewImageFile(QString&, QTreeWidgetItem*)), this, SLOT(AddImageView(QString&, QTreeWidgetItem*)));
    connect(m_projectview,
            SIGNAL(ViewImageFileDiff(C_Destination_Options*, QString&, QString&)),
            this,
            SLOT(AddImageDiff(C_Destination_Options*, QString&, QString&)));
    connect(
        m_projectview, SIGNAL(View3DModelFileDiff(C_3DSubModel_Info*, QString&, QString&)), this, SLOT(Add3DModelDiff(C_3DSubModel_Info*, QString&, QString&)));
    connect(m_projectview, SIGNAL(DeleteImageView(QString&)), this, SLOT(OnDeleteImageView(QString&)));
    connect(m_projectview, SIGNAL(UpdateData(QObject*)), m_imagePropertyView, SLOT(OnUpdateData(QObject*)));
    connect(m_projectview, SIGNAL(AddCompressSettings(QTreeWidgetItem*)), this, SLOT(OnAddCompressSettings(QTreeWidgetItem*)));
    connect(m_projectview, SIGNAL(EditCompressSettings(QTreeWidgetItem*)), this, SLOT(onEditCompressSettings(QTreeWidgetItem*)));

    connect(m_projectview, SIGNAL(OnAddedCompressSettingNode()), this, SLOT(onAddedCompressSettingNode()));
    connect(m_projectview, SIGNAL(OnAddedImageSourceNode()), this, SLOT(onAddedImageSourceNode()));

    connect(m_projectview, SIGNAL(OnSetToolBarActions(int)), this, SLOT(onSetToolBarActions(int)));

    connect(m_projectview, SIGNAL(OnCompressionStart()), this, SLOT(onCompressionStart()));
    connect(m_projectview, SIGNAL(OnCompressionDone()), this, SLOT(onCompressionDone()));

    connect(m_projectview, SIGNAL(OnCompressionStart()), m_imagePropertyView, SLOT(onCompressionStart()));
    connect(m_projectview, SIGNAL(OnProcessing(QString&)), this, SLOT(onProcessing(QString&)));
    connect(m_projectview, SIGNAL(OnCompressionDone()), m_imagePropertyView, SLOT(onCompressionDone()));

    connect(m_projectview, SIGNAL(OnSourceImage(int)), this, SLOT(onSourceImage(int)));
    connect(m_projectview, SIGNAL(OnDecompressImage()), this, SLOT(onDecompressImage()));

    connect(m_projectview, SIGNAL(OnSourceImage(int)), m_imagePropertyView, SLOT(onSourceImage(int)));
    connect(m_projectview, SIGNAL(OnProjectLoaded(int)), this, SLOT(onProjectLoaded(int)));

    connect(this, SIGNAL(OnImageLoadStart()), m_projectview, SLOT(onImageLoadStart()));
    connect(this, SIGNAL(OnImageLoadDone()), m_projectview, SLOT(onImageLoadDone()));
    connect(this, SIGNAL(OnImageLoadStart()), m_imagePropertyView, SLOT(onImageLoadStart()));
    connect(this, SIGNAL(OnImageLoadDone()), m_imagePropertyView, SLOT(onImageLoadDone()));

#ifdef USE_MSGHANDLER
    connect(&static_msghandler, SIGNAL(signalMessage(const char*)), m_projectview, SLOT(OnGlobalMessage(const char*)));
#endif

    connect(this, SIGNAL(SetCurrentItem(QString&)), m_projectview, SLOT(onSetCurrentItem(QString&)));
    connect(m_setcompressoptions,
            SIGNAL(SaveCompressSettings(QTreeWidgetItem*, C_Destination_Options&)),
            this,
            SLOT(AddImageCompSettings(QTreeWidgetItem*, C_Destination_Options&)));

    // QRect scr = QApplication::desktop()->screenGeometry();
    //
    m_genmips = new CGenMips("Generate MIP Maps", NULL);
    m_genmips->hide();
    connect(m_genmips,
            SIGNAL(signalGenerateMipmaps(CMP_CFilterParams, const std::vector<QTreeWidgetItem*>&)),
            this,
            SLOT(generateMipmaps(CMP_CFilterParams, const std::vector<QTreeWidgetItem*>&)));

#ifdef USE_3DCONVERT
    m_modelConvert = new CModelConvert("Convert 3D Models", NULL);
    m_modelConvert->hide();
    connect(m_modelConvert, SIGNAL(convertModel(int, QTreeWidgetItem*)), this, SLOT(onConvertModel(int, QTreeWidgetItem*)));
#endif

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    readSettings();

    // Set some global setting
    //#ifdef ENABLED_USER_GPUVIEW
    g_useCPUDecode = (g_Application_Options.m_ImageViewDecode == C_Application_Options::ImageDecodeWith::CPU);
    //#else
    //    g_useCPUDecode = true;
    //#endif
    g_useCPUEncode = g_Application_Options.m_ImageEncode == C_Application_Options::ImageEncodeWith::CPU;
    setUnifiedTitleAndToolBarOnMac(true);

    if (m_showAppSettingsDialog)
    {
        // Act on read settings for application startup
        if (g_Application_Options.m_loadRecentFile)
        {
            if (m_numRecentFiles > 0)
            {
                recentFileActs[0]->trigger();
            }
        }
    }

    m_welcomePage->GoToAMDHomePage(m_projectsRecentFiles);

    //=============================
    // Help About
    //=============================
    m_pacHelpAboutDialog = new CHelpAboutDialog(this);
    // Sett current project name on app title bar
    SetProjectWindowTitle();

    // Get the product version:
    m_apptitle = "Compressonator";

    QString ver =
        QString("%1.%2.%3.%4")
            .arg(QString::number(VERSION_MAJOR_MAJOR), QString::number(VERSION_MAJOR_MINOR), QString::number(VERSION_MINOR_MAJOR), QString::number(0));

    // Compression Connections

    connect(m_imagePropertyView, SIGNAL(saveSetting(QString*)), this, SLOT(onPropertyViewSaveSetting(QString*)));
    connect(m_imagePropertyView, SIGNAL(compressImage(QString*)), this, SLOT(onPropertyViewCompressImage(QString*)));

#ifdef ENABLE_AGS_SUPPORT
    // Get AGS Settings
    AGSGetDisplayInfo(&m_settings);

    if (m_bIsHDRAvailableOnPrimary)
        onHDRButton->setText("Full Screen with HDR");
    else
        onHDRButton->setText("Full Screen");
#endif

    // Adding a global Progress Dialog,
    // This replaces the one defined in ProjectView
    // Progress Dialog During Compression
    g_pProgressDlg = new acProgressDlg(this);
    if (g_pProgressDlg)
    {
        g_pProgressDlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        g_pProgressDlg->ShowCancelButton(true, &OnCancel);
        g_pProgressDlg->SetHeader("");
        g_pProgressDlg->SetLabelText("");
        g_pProgressDlg->SetRange(0, 100);
        g_pProgressDlg->hide();

        // Configure printline to global shared CMIPS
        if (g_GUI_CMIPS)
        {
            g_GUI_CMIPS->PrintLine        = Print_onProgressDialog;
            g_GUI_CMIPS->SetProgressValue = Set_onProgressValue;
        }
    }
}

// Called when user clicks on a project view item
void cpMainComponents::onSetToolBarActions(int itemType)
{
    // Enable delete button for any project view item
    if ((itemType == TREETYPE_3DMODEL_DATA) || (itemType == TREETYPE_3DSUBMODEL_DATA) || (itemType == TREETYPE_COMPRESSION_DATA) ||
        (itemType == TREETYPE_MESH_DATA) || (itemType == TREETYPE_IMAGEFILE_DATA))
    {
        deleteImageAct->setEnabled(true);
    }
    else
    {
        deleteImageAct->setEnabled(false);
    }

    // Enable the Mip Level gen when on a project image item
    if (itemType == TREETYPE_IMAGEFILE_DATA)
        MIPGenAct->setEnabled(true);
    else
        MIPGenAct->setEnabled(false);
}

void cpMainComponents::SetProjectWindowTitle()
{
    if (m_projectview)
    {
        setWindowTitle(m_projectview->m_curProjectName);
    }
}

void cpMainComponents::OnWelcomePageButtonClick(QString& Request, QString& Msg)
{
#define PROJECT_DIR "/projects/"

    if (Request.compare("new_project") == 0)
    {
        openNewProject();
    }
    else if (Request.compare("open_project") == 0)
    {
        if (!m_projectview)
            return;
        if (!m_projectview->userSaveProjectAndContinue())
            return;

        bool found = false;
        if (Msg.indexOf(".cprj") == -1)
            Msg.append(PROJECT_EXTENSION);

        // Try the shared sample folder
        {
            const QString sampleDir = QString::fromStdString(CMP_GetSampleDataDir());

            QString ProjectFile = QDir::cleanPath(sampleDir + PROJECT_DIR + Msg);
            if (!sampleDir.isEmpty() && QFile::exists(ProjectFile))
            {
                m_projectview->loadProjectFile(QDir::toNativeSeparators(ProjectFile));
                found = true;
            }
        }

        // Try the Working Dir
        if (!found)
        {
            // This is windows specific!
            QString pwd("");
            char*   ENV;
            // Check if user set our envniornment var
            ENV = getenv(ENV_COMPRESSONATOR_ROOT);
            if (ENV)
                pwd.append(ENV);
            else
            {
                // check Pathname of the current working dir
                ENV = getenv("PWD");
            }
            if (ENV)
            {
                pwd.append(PROJECT_DIR);
                pwd.append(Msg);
                QFile Fout(pwd);
                if (Fout.exists())
                {
                    m_projectview->loadProjectFile(pwd);
                    found = true;
                }
            }
        }

        // Try Recent Files
        if (!found)
        {
            for (int i = 0; i < m_numRecentFiles; i++)
            {
                // The list is in &%1 %2 format so the first 3 char are removed
                // Our max recents is 5 which is 1 char size
                QString proj = recentFileActs[i]->text();
                proj.remove(0, 3);
                if (Msg.compare(proj) == 0)
                {
                    recentFileActs[i]->trigger();
                    found = true;
                    break;
                }
            }
        }

        if (found)
        {
            statusBar()->showMessage("Project Location: " + m_projectview->m_curProjectFilePathName);
            setCurrentFile(m_projectview->m_curProjectFilePathName);
        }
    }
    else if (Request.compare("show_quick_start") == 0)
    {
        OpenCHMFile(COMPRESSONATOR_GETTING_STARTED);
    }
    else if (Request.compare("show_help") == 0)
    {
        OpenCHMFile(COMPRESSONATOR_USER_GUIDE);
    }
    else if (Request.compare("show_newfeatures") == 0)
    {
        OpenCHMFile(COMPRESSONATOR_NEWFEATURES_GUIDE);
    }
}

void cpMainComponents::closeEvent(QCloseEvent* event)
{
    m_appclosing = true;

    if (g_bCompressing)
    {
        g_bAbortCompression = true;
        // loop until all compression codecs abort
        int maxwait = 3000;  // > 3 seconds
        while (g_bCompressing)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            maxwait--;
            if (maxwait == 0)
                break;
            QApplication::processEvents();
        }
    }

#ifdef _WIN32
    CMP_ShutdownDecompessLibrary();
#endif

    if (m_projectview)
    {
        if (!m_projectview->userSaveProjectAndContinue())
        {
            event->ignore();
            m_appclosing = false;
            return;
        }
        setCurrentFile(m_projectview->m_curProjectFilePathName);
        m_projectview->clearProjectTreeView();
    }

    onWriteSettings();

    qApp->quit();
    event->accept();
}

void cpMainComponents::openProjectFile()
{
    if (m_projectview)
    {
        m_projectview->openProjectFile();
        setCurrentFile(m_projectview->m_curProjectFilePathName);
    }
}

void cpMainComponents::openNewProject()
{
    if (m_projectview)
    {
        m_projectview->openNewProjectFile();
        setCurrentFile(m_projectview->m_curProjectFilePathName);
    }
}

bool cpMainComponents::saveProjectToBatchFile()
{
    if (m_projectview)
    {
        m_projectview->saveToBatchFile();
    }
    return true;
}

void cpMainComponents::openImageFile()
{
    if (m_projectview)
    {
        m_projectview->OpenImageFile();
    }
}

void cpMainComponents::imageDiff()
{
    if (m_projectview)
    {
        m_projectview->diffImageFiles();
    }
}

void cpMainComponents::deleteImageFile()
{
    if (m_projectview)
    {
        m_projectview->UserDeleteItems();
    }
}

bool cpMainComponents::saveProjectFile()
{
    if (m_projectview)
    {
        m_projectview->saveProjectFile();
        setCurrentFile(m_projectview->m_curProjectFilePathName);
    }
    return true;
}

bool cpMainComponents::saveAsProjectFile()
{
    if (m_projectview)
    {
        m_projectview->saveAsProjectFile();
        setCurrentFile(m_projectview->m_curProjectFilePathName);
    }
    return true;
}

bool cpMainComponents::saveImageFile()
{
    if (m_projectview)
    {
        m_projectview->saveImageAs();
        setCurrentFile(m_projectview->m_curProjectFilePathName);
    }
    return true;
}

void cpMainComponents::settings()
{
    if (m_showAppSettingsDialog)
    {
        if (m_setapplicationoptions)
        {
            m_setapplicationoptions->UpdateViewData();
            m_setapplicationoptions->show();
            m_setapplicationoptions->raise();
        }
    }
}

void cpMainComponents::about()
{
    if (m_pacHelpAboutDialog)
        m_pacHelpAboutDialog->show();
}

void cpMainComponents::onShowWelcomePage()
{
    if (m_welcomePage)
    {
        m_welcomePage->show();
        m_welcomePage->raise();
    }
}

void cpMainComponents::onShowOutput()
{
    if (m_CompressStatusDialog)
    {
        m_CompressStatusDialog->showOutput();
        m_CompressStatusDialog->raise();
    }
}

void cpMainComponents::DeleteDock(acCustomDockWidget** dock)
{
    if (!(*dock))
        return;
    (*dock)->close();
    delete (*dock);
    (*dock) = NULL;
}

// CLoses all Docked Views that have a file_name attached to them
void cpMainComponents::onCloseAllDocuments()
{
    QList<acCustomDockWidget*> dockWidgets = m_parent->findChildren<acCustomDockWidget*>();
    acCustomDockWidget*        dock;

    for (int i = 0; i < dockWidgets.size(); i++)
    {
        dock = dockWidgets[i];
        if (dock)
        {
            QString FileName = dock->m_fileName;
            if (FileName.size() > 0)
            {
                //skip diff subwindow which are already deleted
                if (dock->m_type == TREETYPE_DIFFVIEW)
                {
                    i += 3;
                }
                DeleteDock(&dock);
            }
        }
    }
}

void cpMainComponents::openRecentFile()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action)
    {
        if (m_projectview)
        {
            if (m_projectview->loadProjectFile(action->data().toString()))
            {
                curFile = m_projectview->m_curProjectFilePathName;
            }
        }
    }
}

void cpMainComponents::createActions()
{
    newProjectAct = new QAction(QIcon(":/compressonatorgui/images/filenew.png"), tr("&New Project..."), this);
    if (newProjectAct)
    {
        newProjectAct->setShortcuts(QKeySequence::New);
        newProjectAct->setStatusTip(tr("Create a new project file"));
        connect(newProjectAct, SIGNAL(triggered()), this, SLOT(openNewProject()));
    }

    openAct = new QAction(QIcon(":/compressonatorgui/images/open.png"), tr("&Open project..."), this);
    if (openAct)
    {
        openAct->setShortcuts(QKeySequence::Open);
        openAct->setStatusTip(tr("Open an existing project file"));
        connect(openAct, SIGNAL(triggered()), this, SLOT(openProjectFile()));
    }

    saveAct = new QAction(QIcon(":/compressonatorgui/images/save.png"), tr("&Save project"), this);
    if (saveAct)
    {
        saveAct->setShortcuts(QKeySequence::Save);
        saveAct->setStatusTip(tr("Save project file"));
        connect(saveAct, SIGNAL(triggered()), this, SLOT(saveProjectFile()));
    }

    saveAsAct = new QAction(QIcon(""), tr("&Save project as..."), this);
    if (saveAsAct)
    {
        saveAsAct->setShortcuts(QKeySequence::SaveAs);
        saveAsAct->setStatusTip(tr("Save project as ..."));
        connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAsProjectFile()));
    }

    saveImageAct = new QAction(QIcon(""), tr("&Save image as..."), this);
    if (saveImageAct)
    {
        saveImageAct->setStatusTip(tr("Save image as ..."));
        connect(saveImageAct, SIGNAL(triggered()), this, SLOT(saveImageFile()));
        saveImageAct->setEnabled(false);
    }

    for (int i = 0; i < MaxRecentFiles; ++i)
    {
        recentFileActs[i] = new QAction(this);
        recentFileActs[i]->setVisible(false);
        connect(recentFileActs[i], SIGNAL(triggered()), this, SLOT(openRecentFile()));
    }

    saveToBatchFileAct = new QAction(QIcon(""), tr("&Export to batch file..."), this);
    if (saveToBatchFileAct)
    {
        saveToBatchFileAct->setStatusTip(tr("Export the project file to a command line batch file"));
        connect(saveToBatchFileAct, SIGNAL(triggered()), this, SLOT(saveProjectToBatchFile()));
        saveToBatchFileAct->setEnabled(false);
    }

    openImageFileAct = new QAction(QIcon(":/compressonatorgui/images/file.png"), tr("&Open Image File..."), this);
    if (openImageFileAct)
    {
        //ToDo :: openImageFileAct->setShortcuts();
        openImageFileAct->setStatusTip(tr("Open an image file"));
        connect(openImageFileAct, SIGNAL(triggered()), this, SLOT(openImageFile()));
    }

    deleteImageAct = new QAction(QIcon(":/compressonatorgui/images/delete.png"), tr("&Delete current image"), this);
    if (deleteImageAct)
    {
        //ToDo :: deleteImageAct->setShortcuts();
        deleteImageAct->setStatusTip(tr("Delete selected image file"));
        connect(deleteImageAct, SIGNAL(triggered()), this, SLOT(deleteImageFile()));
        deleteImageAct->setEnabled(false);
    }

    compressAct = new QAction(QIcon(":/compressonatorgui/images/compress.png"), tr("&Process selected images"), this);
    if (compressAct)
    {
        compressAct->setStatusTip(tr("Compress all selected items"));
        connect(compressAct, SIGNAL(triggered()), m_projectview, SLOT(onMenuBarCompression()));
        compressAct->setEnabled(false);
    }

    imagediffAct = new QAction(QIcon(":/compressonatorgui/images/imagediff.png"), tr("&View Image Diff"), this);
    if (imagediffAct)
    {
        imagediffAct->setStatusTip(tr("View Image Diff"));
        connect(imagediffAct, SIGNAL(triggered()), this, SLOT(imageDiff()));
        imagediffAct->setEnabled(true);
    }

    MIPGenAct = new QAction(QIcon(":/compressonatorgui/images/mip.png"), tr("&Generate MIP maps on selected source images"), this);
    if (MIPGenAct)
    {
        MIPGenAct->setStatusTip(tr("Generate MIP maps on selected source images"));
        connect(MIPGenAct, SIGNAL(triggered()), this, SLOT(onGenerateMipmapsMenuClicked()));
        MIPGenAct->setEnabled(false);
    }
#ifdef USE_3DCONVERT
    ConvertModelAct = new QAction(QIcon(":/compressonatorgui/images/3dmodelconvert.png"), tr("&Convert 3D Models to another format"), this);
    if (ConvertModelAct)
    {
        ConvertModelAct->setStatusTip(tr("Convert Model to another file format"));
        connect(ConvertModelAct, SIGNAL(triggered()), this, SLOT(convertModels()));
        ConvertModelAct->setEnabled(true);
    }
#endif
#ifdef ENABLE_AGS_SUPPORT
    onHDRButton = new QPushButton("Full Screen", this);
    if (onHDRButton)
    {
        onHDRButton->setStatusTip(tr("Sets Full screen on or off , If available HDR is turned on in Full Screen"));
        connect(onHDRButton, SIGNAL(released()), this, SLOT(handleHDRon()));
    }
#endif

#ifdef USE_MAIN_IMAVEVIEW_TOOLBAR
    imageview_zoomInAct    = new QAction(QIcon(":/compressonatorgui/images/zoomin.png"), tr("&Zoom into Image "), this);
    imageview_zoomOutAct   = new QAction(QIcon(":/compressonatorgui/images/zoomout.png"), tr("&Zoom out of Image"), this);
    imageview_RedAct       = new QAction(QIcon(":/compressonatorgui/images/redstone.png"), tr("Show or Hide  Red channel"), this);
    imageview_GreenAct     = new QAction(QIcon(":/compressonatorgui/images/greenstone.png"), tr("Show or Hide Green channel"), this);
    imageview_BlueAct      = new QAction(QIcon(":/compressonatorgui/images/bluestone.png"), tr("Show or Hide Blue channel"), this);
    imageview_AlphaAct     = new QAction(QIcon(":/compressonatorgui/images/circle.png"), tr("Show or Hide Alpha channel"), this);
    imageview_FitScreenAct = new QAction(QIcon(":/compressonatorgui/images/expand.png"), tr("&Fit in Window"), this);
#endif

    exitAct = new QAction(QIcon(""), tr("&Exit"), this);
    if (exitAct)
    {
        exitAct->setStatusTip(tr("Exit Application"));
        connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));
    }

    if (m_showAppSettingsDialog)
    {
        settingsAct = new QAction(QIcon(":/compressonatorgui/images/gear.png"), tr("&Set Application Options"), this);
        if (settingsAct)
        {
            settingsAct->setStatusTip(tr("Set Application Options"));
            connect(settingsAct, SIGNAL(triggered()), this, SLOT(settings()));
        }
    }

    showWelcomePageAct = new QAction(tr("Welcome Page"), this);
    if (showWelcomePageAct)
    {
        showWelcomePageAct->setStatusTip(tr("View Welcome Page"));
        connect(showWelcomePageAct, SIGNAL(triggered()), this, SLOT(onShowWelcomePage()));
    }

    showOutputAct = new QAction(tr("Output"), this);
    if (showOutputAct)
    {
        showOutputAct->setStatusTip(tr("View Output Window"));
        connect(showOutputAct, SIGNAL(triggered()), this, SLOT(onShowOutput()));
    }

    closeAllDocuments = new QAction(tr("Close all Image Views"), this);
    if (closeAllDocuments)
    {
        closeAllDocuments->setStatusTip(tr("Close all opened image views"));
        connect(closeAllDocuments, SIGNAL(triggered()), this, SLOT(onCloseAllDocuments()));
    }

    gettingStartedAct = new QAction(tr("Getting Started ..."), this);
    if (gettingStartedAct)
    {
        gettingStartedAct->setStatusTip(tr("Getting Started"));
        connect(gettingStartedAct, SIGNAL(triggered()), this, SLOT(gettingStarted()));
    }

    userGuideAct = new QAction(tr("User Guide ..."), this);
    if (userGuideAct)
    {
        userGuideAct->setStatusTip(tr("User Guide"));
        connect(userGuideAct, SIGNAL(triggered()), this, SLOT(userGuide()));
    }

    aboutAct = new QAction(tr("About Compressonator"), this);
    if (aboutAct)
    {
        aboutAct->setStatusTip(tr("About Compressonator"));
        connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));
    }

    newFeaturesAct = new QAction(tr("New Features ..."), this);
    if (newFeaturesAct)
    {
        newFeaturesAct->setStatusTip(tr("New Features Guide"));
        connect(newFeaturesAct, SIGNAL(triggered()), this, SLOT(newFeaturesGuide()));
    }
}

void cpMainComponents::generateMipmaps(CMP_CFilterParams CFilterParams, const std::vector<QTreeWidgetItem*>& images)
{
    if (!m_projectview || images.size() <= 0)
        return;

    if (m_CompressStatusDialog)
    {
        m_CompressStatusDialog->onClearText();
        m_CompressStatusDialog->showOutput();
    }

    if (CFilterParams.nMinSize <= 0)
        CFilterParams.nMinSize = 1;

    for (auto& image : images)
    {
        CMP_CFilterParams currImageParams = CFilterParams;

        QVariant       variant = image->data(TREE_SourceInfo, Qt::UserRole);
        C_Source_Info* data    = variant.value<C_Source_Info*>();

        // initial checks before creating the mipmaps

        if (!data || !data->m_MipImages || !data->m_MipImages->mipset)
            continue;

        if (data->m_MipImages->mipset->m_compressed)
        {
            if (m_CompressStatusDialog)
                m_CompressStatusDialog->appendText("Skipping image: '" + data->m_Name + "', mipmap generation is not supported for compressed images.\n");
            continue;
        }

        if (m_CompressStatusDialog)
            m_CompressStatusDialog->appendText("Processing image \"" + data->m_Name + "\"...");

        // make sure that the minimum mipmap size is not larger than the current image size
        // NOTE: nMinSize is set using the width value
        if (currImageParams.nMinSize >= data->m_Width || currImageParams.nMinSize >= data->m_Height)
        {
            if (m_CompressStatusDialog)
                m_CompressStatusDialog->appendText("No mipmaps created for " + data->m_Name + ", minimum size is not smaller than image size.\n");
            continue;
        }

        // reset mipmaps that might already be present

        if (data->m_MipImages->mipset->m_nMipLevels > 1 || data->m_MipImages->QImage_list[0].size() > 1)
        {
            for (int depth = 0; depth < data->m_MipImages->mipset->m_nDepth; ++depth)
            {
                // need to cache value so that it doesn't change as we change the list
                int numMipLevels = data->m_MipImages->QImage_list[depth].size();
                for (int mipLevel = 1; mipLevel < numMipLevels; mipLevel++)
                {
                    delete data->m_MipImages->QImage_list[depth][data->m_MipImages->QImage_list[depth].size() - 1];
                    data->m_MipImages->QImage_list[depth].pop_back();
                }
            }

            data->m_MipImages->mipset->m_nMipLevels = 1;
        }

        // make sure the requested mipmap level size is supported for the current image
        if (g_Application_Options.isGPUEncode())
        {
            int minValidSize = data->m_Width;

            int currWidth  = data->m_Width;
            int currHeight = data->m_Height;

            while (currWidth > currImageParams.nMinSize && currHeight > currImageParams.nMinSize)
            {
                currWidth  = currWidth >> 1;
                currHeight = currHeight >> 1;

                if (currWidth <= 1 || currHeight <= 1 || currWidth % 4 != 0 || currHeight % 4 != 0)
                    break;

                minValidSize = currWidth;
            }

            currImageParams.nMinSize = minValidSize;
        }

        if (currImageParams.nMinSize != CFilterParams.nMinSize)
        {
            if (m_CompressStatusDialog)
                m_CompressStatusDialog->appendText("Minimum valid size for " + data->m_Name + " set to " + QString::number(currImageParams.nMinSize) + "\n");
        }

        // generate new mipmaps

        // Always reload the original source image prior to processing
        // this will clear any prior mipmap processing that was performed on the source image
        // like Gamma settings, and some filters been applied on the source image
        // It also takes care of freeing any old mip levels that the user has not requested to be kept
        // ie moving from a lower mip level to a higher one
        CMP_FreeMipSet((CMP_MipSet*)data->m_MipImages->mipset);
        if (AMDLoadMIPSTextureImage(data->m_Full_Path.toStdString().c_str(), (CMP_MipSet*)data->m_MipImages->mipset, false, &g_pluginManager) != CMP_OK)
        {
            // Something went wrong on reload, exit!
            if (m_CompressStatusDialog)
            {
                m_CompressStatusDialog->appendText("Error in reloading the image for mipmap processing!");
                m_CompressStatusDialog->appendText("Please restart the application to restore the source image");
            }
            continue;
        }

        // Check if a plugin is avaliable for MipMap Generation
        PluginInterface_Filters* g_plugin_MipMapFilter = reinterpret_cast<PluginInterface_Filters*>(g_pluginManager.GetPlugin("FILTERS", "MIPMAP"));

        CMP_INT result;
        if (g_plugin_MipMapFilter)
        {
            // Init Codec info IO
            if ((g_CMIPS->PrintLine == NULL) && (PrintStatusLine != NULL))
            {
                g_CMIPS->PrintLine = PrintStatusLine;
            }

            if (g_plugin_MipMapFilter->TC_PluginSetSharedIO(g_CMIPS) == CMP_OK)
            {
                result = g_plugin_MipMapFilter->TC_CFilter((CMP_MipSet*)data->m_MipImages->mipset, NULL, &currImageParams);
            }
            else
                result = CMP_ERR_GENERIC;

            delete g_plugin_MipMapFilter;
        }
        else
            // Generate the MIP levels using Compressonator SDK
            result = CMP_GenerateMIPLevelsEx((CMP_MipSet*)data->m_MipImages->mipset, &currImageParams);

        // Create Image views for the levels
        if (result == CMP_OK)
        {
            CImageLoader imageLoader;
            imageLoader.UpdateMIPMapImages(data->m_MipImages);

            if (m_CompressStatusDialog)
            {
                QString msg = "<b>Generated : ";
                msg.append(QString::number(data->m_MipImages->mipset->m_nMipLevels));
                msg.append(" MIP level(s)</b>");
                msg.append(" with a minimum size set to ");
                msg.append(QString::number(currImageParams.nMinSize));
                msg.append(" px");
                m_CompressStatusDialog->appendText(msg);
                m_ForceImageRefresh = true;
            }
        }
        else if (m_CompressStatusDialog)
        {
            m_CompressStatusDialog->appendText("Error in processing MipMap!");
        }

        m_projectview->SignalUpdateData(image, TREETYPE_IMAGEFILE_DATA);
        m_projectview->m_clicked_onIcon = true;
        m_projectview->onTree_ItemClicked(image, 0);

        if (m_CompressStatusDialog)
            m_CompressStatusDialog->appendText("");
    }
}

void cpMainComponents::onGenerateMipmapsMenuClicked()
{
    if (!m_projectview)
        return;

    std::vector<QTreeWidgetItem*> images = m_projectview->GetAllSelectedImages();

    if (images.size() == 0)
    {
        m_projectview->m_CompressStatusDialog->appendText("Please select the image file(s) to use for generating mipmaps.");
        m_projectview->m_CompressStatusDialog->show();
        return;
    }

    int mipWidth  = 0;
    int mipHeight = 0;

    if (images.size() == 1)
    {
        // directly use width and height if there is only one image being processed

        QVariant       v    = images[0]->data(TREE_SourceInfo, Qt::UserRole);
        C_Source_Info* data = v.value<C_Source_Info*>();

        mipWidth  = data->m_Width;
        mipHeight = data->m_Height;
    }
    else if (images.size() > 1)
    {
        // when processing more than one image, we use the largest "standardized" size as the basis for generating the mip levels

        int mipPower = 0;

        // out of all the images we want to pick the one that could generate the most mipmap levels
        // to do this we find the image that can fit the largest power of 2 sized square
        // we use the dimensions of that square as our mipmap level 0 size
        for (const auto& image : images)
        {
            QVariant       v    = image->data(TREE_SourceInfo, Qt::UserRole);
            C_Source_Info* data = v.value<C_Source_Info*>();

            // width and height as a power of 2, rounded up to provide the full range
            int widthPower  = (int)ceil(log2(data->m_Width));
            int heightPower = (int)ceil(log2(data->m_Height));

            int smallerPower = widthPower < heightPower ? widthPower : heightPower;

            if (smallerPower > mipPower)
                mipPower = smallerPower;
        }

        int mipSize = (int)pow(2, mipPower);

        mipWidth  = mipSize;
        mipHeight = mipSize;
    }

    m_genmips->m_imageItems = std::move(images);

    m_genmips->setMipLevelDisplay(mipWidth, mipHeight, g_Application_Options.isGPUEncode());
    QString title = "Generate Mipmaps for ";

    if (g_Application_Options.isGPUEncode())
        title.append("GPU");
    else
        title.append("CPU");

    m_genmips->setWindowTitle(title);
    m_genmips->show();
}

void cpMainComponents::convertModels()
{
    if (m_projectview)
    {
        QString          sourceFileName = "";
        QTreeWidgetItem* item           = m_projectview->GetCurrentItem();
        if (item)
        {
            QVariant v         = item->data(TREE_LevelType, Qt::UserRole);
            int      levelType = v.toInt();
            v                  = item->data(TREE_SourceInfo, Qt::UserRole);
            switch (levelType)
            {
            case TREETYPE_3DMODEL_DATA: {
                C_3DModel_Info* m_data = v.value<C_3DModel_Info*>();
                if (m_data)
                {
                    sourceFileName = m_data->m_Full_Path;
                }
            }
            break;
            case TREETYPE_3DSUBMODEL_DATA: {
                C_3DSubModel_Info* m_data = v.value<C_3DSubModel_Info*>();
                if (m_data)
                {
                    sourceFileName = m_data->m_Full_Path;
                }
            }
            break;
            }
        }
#ifdef USE_3DCONVERT
        m_modelConvert->setSourceFile(sourceFileName);
        m_modelConvert->clearStatus();
        m_modelConvert->show();
#endif
    }
}

#ifdef ENABLE_AGS_SUPPORT
//--------------------------------------------------------------------------------------
void cpMainComponents::AGSGetDisplayInfo(AGSDisplaySettings* settings)
{
    int             displayIndex = 0;
    DISPLAY_DEVICEA displayDevice;
    displayDevice.cb = sizeof(displayDevice);
    while (EnumDisplayDevicesA(0, displayIndex, &displayDevice, 0))
    {
        displayIndex++;
    }

    AGSGPUInfo gpuInfo;

    AGSConfiguration config = {};
    config.crossfireMode    = AGS_CROSSFIRE_MODE_EXPLICIT_AFR;

    if (agsInit(&m_agsContext, &config, &gpuInfo) == AGS_SUCCESS)
    {
        for (int gpuIndex = 0; gpuIndex < gpuInfo.numDevices; gpuIndex++)
        {
            const AGSDeviceInfo& device = gpuInfo.devices[gpuIndex];
            for (int i = 0; i < device.numDisplays; i++)
            {
                const AGSDisplayInfo& display = device.displays[i];
                if (display.displayFlags & AGS_DISPLAYFLAG_PRIMARY_DISPLAY && display.displayFlags & AGS_DISPLAYFLAG_HDR10)
                {
                    settings->chromaticityRedX = display.chromaticityRedX;  ///< Red display primary X coord
                    settings->chromaticityRedY = display.chromaticityRedY;  ///< Red display primary Y coord

                    settings->chromaticityGreenX = display.chromaticityGreenX;  ///< Green display primary X coord
                    settings->chromaticityGreenY = display.chromaticityGreenY;  ///< Green display primary Y coord

                    settings->chromaticityBlueX = display.chromaticityBlueX;  ///< Blue display primary X coord
                    settings->chromaticityBlueY = display.chromaticityBlueY;  ///< Blue display primary Y coord

                    settings->chromaticityWhitePointX = display.chromaticityWhitePointX;  ///< White point X coord
                    settings->chromaticityWhitePointY = display.chromaticityWhitePointY;  ///< White point Y coord

                    settings->minLuminance = display.minLuminance;  ///< The minimum scene luminance in nits
                    settings->maxLuminance = display.maxLuminance;  ///< The maximum scene luminance in nits

                    settings->maxContentLightLevel;  ///< The maximum content light level in nits (MaxCLL)
                    settings->maxFrameAverageLightLevel;
                    m_DeviceIndex              = gpuIndex;
                    m_DisplayIndex             = i;
                    m_bIsHDRAvailableOnPrimary = true;
                }
            }
        }
    }
}

bool cpMainComponents::AGSSetDisplay(AGSDisplaySettings* settings)
{
    if (AGS_SUCCESS == agsSetDisplayMode(m_agsContext, m_DeviceIndex, m_DisplayIndex, settings))
    {
        statusBar()->showMessage(tr("HDR enabled."));
        return true;
    }
    else
    {
        statusBar()->showMessage(tr("Set HDR fail."));
        return false;
    }
}
//---------------------------------------------------------------------------------

void cpMainComponents::handleHDRon()
{
    if (!m_bIsFullScreenModeOn)
    {
        this->showFullScreen();
        m_bIsFullScreenModeOn = true;
        onHDRButton->setText("Normal Screen");
    }
    else
    {
        this->showNormal();
        if (m_bIsHDRAvailableOnPrimary)
            onHDRButton->setText("Full Screen with HDR");
        else
            onHDRButton->setText("Full Screen");
        m_bIsFullScreenModeOn = false;
    }

    // This part of the code could also be another button!
    // that is enabled if HDR is available on the primary display
    if (m_bIsHDRAvailableOnPrimary)
    {
        if (m_bIsFullScreenModeOn)
        {
            // HDR On
            m_settings.mode = AGSDisplaySettings::Mode_scRGB;
            if (AGSSetDisplay(&m_settings))
            {
            }
        }
        else
        {
            // HDR Off
            m_settings.mode = AGSDisplaySettings::Mode_SDR;
            if (AGSSetDisplay(&m_settings))
            {
            }
        }
    }
}
#endif

void cpMainComponents::setCurrentFile(const QString& fileName)
{
    curFile = fileName;
    setWindowFilePath(curFile);

    QSettings   settings(m_sSettingsFile, QSettings::IniFormat);
    QStringList files = settings.value("recentFileList").toStringList();
    files.removeAll(fileName);
    files.prepend(fileName);
    while (files.size() > MaxRecentFiles)
        files.removeLast();

    settings.setValue("recentFileList", files);
    settings.sync();
    SetProjectWindowTitle();
}

void cpMainComponents::updateRecentFileActions()
{
    QSettings   settings(m_sSettingsFile, QSettings::IniFormat);
    QStringList files = settings.value("recentFileList").toStringList();
    QStringList UpdatedList;

    int numRecentFile       = 0;
    int scan_numRecentFiles = qMin(files.size(), (int)MaxRecentFiles);

    UpdatedList.clear();
    for (int i = 0; i < scan_numRecentFiles; ++i)
    {
        // Avoid adding duplicates
        // and fix up file paths to use "/" notation!
        files[i].replace("\\", "/");
        if (m_projectsRecentFiles.contains(files[i]))
        {
            continue;
        }

        m_projectsRecentFiles.push_back(files[i]);

        QFile file(files[i]);
        if (file.exists())
        {
            QString text = tr("&%1 %2").arg(numRecentFile + 1).arg(strippedName(files[i]));
            recentFileActs[numRecentFile]->setText(text);
            recentFileActs[numRecentFile]->setData(files[i]);
            recentFileActs[numRecentFile]->setStatusTip(files[i]);
            recentFileActs[numRecentFile]->setVisible(true);
            numRecentFile++;
            UpdatedList.append(files[i]);
        }
    }

    // Save back a cleaned up list of existing project files
    settings.setValue("recentFileList", UpdatedList);

    for (int j = numRecentFile; j < MaxRecentFiles; ++j)
        recentFileActs[j]->setVisible(false);

    m_numRecentFiles = numRecentFile;
    separatorAct->setVisible(m_numRecentFiles > 0);
}

QString cpMainComponents::strippedName(const QString& fullFileName)
{
    return QFileInfo(fullFileName).fileName();
}

void cpMainComponents::onAboutToShowFileMenu()
{
    if (m_projectview && saveToBatchFileAct)
    {
        if ((m_projectview->m_NumItems > 2) && m_projectview->AnySelectedItems())
        {
            saveToBatchFileAct->setEnabled(true);
        }
        else
            saveToBatchFileAct->setEnabled(false);
    }
}

void cpMainComponents::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    if (fileMenu)
    {
        if (newProjectAct)
            fileMenu->addAction(newProjectAct);
        if (openAct)
            fileMenu->addAction(openAct);
        if (saveAct)
            fileMenu->addAction(saveAct);
        if (saveAsAct)
            fileMenu->addAction(saveAsAct);
        if (saveImageAct)
            fileMenu->addAction(saveImageAct);
        fileMenu->addSeparator();
        if (openImageFileAct)
            fileMenu->addAction(openImageFileAct);
        if (saveToBatchFileAct)
            fileMenu->addAction(saveToBatchFileAct);
        fileMenu->addSeparator();
        if (exitAct)
            fileMenu->addAction(exitAct);
        separatorAct = menuBar()->addSeparator();
        for (int i = 0; i < MaxRecentFiles; ++i)
            fileMenu->addAction(recentFileActs[i]);
        updateRecentFileActions();
        fileMenu->addSeparator();
        fileMenu->addAction(exitAct);
        connect(fileMenu, SIGNAL(aboutToShow()), this, SLOT(onAboutToShowFileMenu()));
    }

    if (m_showAppSettingsDialog)
    {
        settingsMenu = menuBar()->addMenu(tr("&Settings"));
        if (settingsMenu)
        {
            if (settingsAct)
                settingsMenu->addAction(settingsAct);
        }
    }

    windowMenu = menuBar()->addMenu(tr("&Window"));
    if (windowMenu)
    {
        if (showWelcomePageAct)
            windowMenu->addAction(showWelcomePageAct);
        if (showOutputAct)
            windowMenu->addAction(showOutputAct);
        if (closeAllDocuments)
            windowMenu->addAction(closeAllDocuments);
    }

    helpMenu = menuBar()->addMenu(tr("&Help"));
    if (helpMenu)
    {
        if (gettingStartedAct)
            helpMenu->addAction(gettingStartedAct);
        if (userGuideAct)
            helpMenu->addAction(userGuideAct);
        if (aboutAct)
            helpMenu->addAction(aboutAct);
    }
}

void cpMainComponents::menuItemClicked(QAction* triggeredAction)
{
    // use either the action itself... or an offset
    int value = triggeredAction->data().toInt();
    Q_UNUSED(value);
}

void cpMainComponents::createToolBars()
{
    fileToolBar = addToolBar(tr("File"));
    if (fileToolBar)
    {
        if (newProjectAct)
            fileToolBar->addAction(newProjectAct);
        if (openAct)
            fileToolBar->addAction(openAct);
        if (saveAct)
            fileToolBar->addAction(saveAct);
        fileToolBar->addSeparator();
        if (openImageFileAct)
            fileToolBar->addAction(openImageFileAct);
        if (settingsAct)
            fileToolBar->addAction(settingsAct);
        fileToolBar->addSeparator();
        if (deleteImageAct)
            fileToolBar->addAction(deleteImageAct);
    }

    CompressionToolBar = addToolBar(tr("Compression"));
    if (CompressionToolBar)
    {
        if (compressAct)
            CompressionToolBar->addAction(compressAct);
        if (imagediffAct)
            CompressionToolBar->addAction(imagediffAct);
        if (MIPGenAct)
            CompressionToolBar->addAction(MIPGenAct);
#ifdef ENABLE_AGS_SUPPORT
        if (onHDRButton)
            CompressionToolBar->addWidget(onHDRButton);
#endif
    }

#ifdef USE_3DCONVERT
    CompressionToolBar = addToolBar(tr("3D Models"));
    if (CompressionToolBar)
    {
        if (ConvertModelAct)
            CompressionToolBar->addAction(ConvertModelAct);
    }
#endif

#ifdef USE_MAIN_IMAVEVIEW_TOOLBAR
    ImageViewToolBar = addToolBar(tr("Image View"));
    if (ImageViewToolBar)
    {
        if (imageview_zoomInAct)
            ImageViewToolBar->addAction(imageview_zoomInAct);
        if (imageview_zoomOutAct)
            ImageViewToolBar->addAction(imageview_zoomOutAct);
        if (imageview_RedAct)
            ImageViewToolBar->addAction(imageview_RedAct);
        if (imageview_GreenAct)
            ImageViewToolBar->addAction(imageview_GreenAct);
        if (imageview_BlueAct)
            ImageViewToolBar->addAction(imageview_BlueAct);
        if (imageview_AlphaAct)
            ImageViewToolBar->addAction(imageview_AlphaAct);
        if (imageview_FitScreenAct)
            ImageViewToolBar->addAction(imageview_FitScreenAct);
    }

    ImageViewToolBar->setEnabled(false);
#endif
}

void cpMainComponents::createStatusBar()
{
    statusBar()->setStyleSheet("QStatusBar{border-top: 1px outset grey;}");
    statusBar()->showMessage(tr("Ready"));
}

void cpMainComponents::showProgressBusy(QString Message)
{
    if (m_appclosing)
        return;

    statusBar()->showMessage(Message);
    if (qApp)
        qApp->setOverrideCursor(Qt::BusyCursor);
    if (m_projectview)
        m_projectview->m_processBusy = true;
}

void cpMainComponents::hideProgressBusy(QString Message)
{
    if (m_appclosing)
        return;

    statusBar()->showMessage(Message);
    if (qApp)
        qApp->restoreOverrideCursor();
    if (m_projectview)
        m_projectview->m_processBusy = false;
}

void cpMainComponents::readSettings()
{
    QSettings settings(m_sSettingsFile, QSettings::IniFormat);
    QPoint    pos  = settings.value("pos", QPoint(100, 100)).toPoint();
    QSize     size = settings.value("size", QSize(600, 400)).toSize();
    resize(size);
    move(pos);
    if (m_showAppSettingsDialog)
    {
        m_setapplicationoptions->LoadSettings(m_sSettingsFile, QSettings::IniFormat);
    }
    m_projectview->m_curProjectFilePathName = settings.value("ProjectPath").toString();
}

void cpMainComponents::onWriteSettings()
{
    QSettings settings(m_sSettingsFile, QSettings::IniFormat);
    settings.setValue("pos", pos());
    settings.setValue("size", size());
    if (m_showAppSettingsDialog)
    {
        m_setapplicationoptions->SaveSettings(m_sSettingsFile, QSettings::IniFormat);
    }
    settings.setValue("ProjectPath", m_projectview->m_curProjectFilePathName);
}

acCustomDockWidget* cpMainComponents::FindImageView(QString& file, bool findDiffs)
{
    QList<acCustomDockWidget*> dockWidgets = m_parent->findChildren<acCustomDockWidget*>();

    if (dockWidgets.size() == 0)
        return NULL;
    acCustomDockWidget* dock;

    /**********************
    //Used for testing
    QListIterator<acCustomDockWidget *> iter2(dockWidgets);
    printf("FindImage [%s]\n", file.toStdString().c_str());
    while (iter2.hasNext())
    {
        dock = iter2.next();
        QString dock_fileName = dock->m_fileName;
        printf("Dock filename [%s]\n", dock_fileName.toStdString().c_str());
    }
    ***********************/

    QListIterator<acCustomDockWidget*> iter(dockWidgets);
    while (iter.hasNext())
    {
        dock                  = iter.next();
        QString dock_fileName = dock->m_fileName;
        int     res           = file.compare(dock_fileName);
        if ((res == 0) && (!findDiffs))
        {
            return dock;
        }
        else
        {
            if (findDiffs)
            {
                if (dock_fileName.contains(DIFFERENCE_IMAGE_VS_TXT))
                {
                    return dock;
                }
            }
        }
    }

    return NULL;
}

// Active when user selects any Docked Tabbed Item
void cpMainComponents::onDockImageVisibilityChanged(bool visible)
{
    Q_UNUSED(visible);

    QTabBar* tabBar = this->findChild<QTabBar*>();
    if (tabBar)
    {
        int     currentIndex = tabBar->currentIndex();
        QString tabText      = tabBar->tabText(currentIndex);
        int     numTab       = tabBar->count();

        for (int i = 0; i < numTab; i++)
        {
            if (tabBar->tabText(i).compare("") == 0)
            {
                tabBar->setStyleSheet("QTabBar::tab:disabled { width: 0; height: 0; margin: 0; padding: 0; border: none; }");
                tabBar->setTabEnabled(i, false);
            }
        }

        const QList<QDockWidget*> tabedWidgets = tabifiedDockWidgets(m_blankpage);
        QDockWidget*              item;
        foreach (item, tabedWidgets)
        {
            // Get our custom Dock Widget
            if (item->titleBarWidget())
            {
                acCustomDockWidget* imageItem = reinterpret_cast<acCustomDockWidget*>(item);
                if (imageItem && !m_viewDiff)
                {
                    if (imageItem->m_tabName.compare("") == 0)
                    {
                        imageItem->lower();
                    }

                    if (imageItem->m_tabName.compare(tabText) == 0)
                    {
                        emit SetCurrentItem(imageItem->m_fileName);
                    }
                }
            }
        }
    }
}

void cpMainComponents::AddImageCompSettings(QTreeWidgetItem* item, C_Destination_Options& data)
{
    if (!item)
        return;

    try
    {
        if (data.m_editing)
        {
            QVariant               v      = item->data(TREE_SourceInfo, Qt::UserRole);
            C_Destination_Options* m_data = v.value<C_Destination_Options*>();
            *m_data << data;
            if (!m_data)
                return;
            item->setText(0, m_data->m_compname);

            QFileInfo fileinfo(m_data->m_destFileNamePath);
            QFile     file(m_data->m_destFileNamePath);
            m_data->m_FileSize = file.size();
            if (m_data->m_FileSize > 1024000)
                m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024000, 'f', 2) + " MB";
            else if (m_data->m_FileSize > 1024)
                m_data->m_FileSizeStr = QString().number((double)m_data->m_FileSize / 1024, 'f', 1) + " KB";
            else
                m_data->m_FileSizeStr = QString().number(m_data->m_FileSize) + " Bytes";
            if (file.exists() && (fileinfo.suffix().length() > 0))
                item->setIcon(0, QIcon(":/compressonatorgui/images/smallgraystone.png"));
            else
                item->setIcon(0, QIcon(":/compressonatorgui/images/smallwhiteblank.png"));

            // refresh the current Image property view (It may or maynot be pointing to The compression data
            // That was edited.
            m_imagePropertyView->refreshView();

            // Refresh the image
            m_projectview->m_clicked_onIcon = true;
            m_projectview->onTree_ItemClicked(item, 0);
        }
        else
        {
            // Check who called the [+] add setting
            QString itemName;
            itemName           = item->text(0);
            QVariant v         = item->data(TREE_LevelType, Qt::UserRole);
            int      levelType = v.toInt();

            switch (levelType)
            {
            case TREETYPE_Add_destination_setting: {
                if (data.m_isModelData)
                {  // Adding a Mesh Buffer Data Node
                    C_Destination_Options* m_data = new C_Destination_Options;
                    // copy the new data from compress settings dialog data
                    *m_data << data;
                    if (!m_data)
                        return;
                    m_projectview->Tree_AddCompressFile(item, m_data->m_compname, true, true, TREETYPE_MESH_DATA, m_data);
                }
                else
                {  // Adding a Image Data Node to either transcode or compress
                    C_Destination_Options* m_data = new C_Destination_Options;
                    // copy the new data from compress settings dialog data
                    *m_data << data;
                    if (!m_data)
                        return;
                    m_projectview->Tree_AddCompressFile(item, m_data->m_compname, true, true, TREETYPE_COMPRESSION_DATA, m_data);
                }
                break;
            }
            case TREETYPE_Add_Model_destination_settings: {
                C_Destination_Options* m_data = new C_Destination_Options;
                // copythe new data from comsettings dialog data
                *m_data << data;
                if (!m_data)
                    return;
                QString DestfilePathName = m_data->m_destFileNamePath;

                if (QFile::exists(m_data->m_destFileNamePath))
                {
                    QFile::remove(m_data->m_destFileNamePath);
                }

                bool isCopy = QFile::copy(m_data->m_sourceFileNamePath, m_data->m_destFileNamePath);

                if (!isCopy)
                {
                    QString error =
                        "Error: Create " + (m_data->m_destFileNamePath) + " failed. Please make sure you have write permission to the destination path.\n";
                    PrintInfo(error.toStdString().c_str());
                    if (m_CompressStatusDialog)
                    {
                        m_CompressStatusDialog->showOutput();
                    }
                    return;
                }
#ifdef _WIN32
                if (m_data->m_destFileNamePath.contains(".obj") || m_data->m_destFileNamePath.contains(".OBJ"))
                {
                    //write to indicate the state of the file
                    writeObjFileState(m_data->m_destFileNamePath.toStdString(), CMP_COPY);
                }
#endif
                QTreeWidgetItem* ParentItem = item->parent();
                if (ParentItem)
                {
                    QString itemName = ParentItem->text(0);
                    m_projectview->Tree_Add3DSubModelFile(ParentItem, DestfilePathName, NULL);
                }
                break;
            }
            case TREETYPE_IMAGEFILE_DATA: {  //for case no image item selected,only work for imagefile item
                if (data.m_isModelData)
                {
                    return;
                }
                else
                {  // Adding a Image Data Node for compress
                    C_Destination_Options* m_data = new C_Destination_Options;
                    // copy the new data from compress settings dialog data
                    *m_data << data;
                    if (!m_data)
                        return;
                    m_projectview->Tree_AddCompressFile(item, m_data->m_compname, true, true, TREETYPE_COMPRESSION_DATA, m_data);
                }
                break;
            }
            default:
                break;
            }
        }
    }
    catch (...)
    {
        DisplayException("");
    }
}

void cpMainComponents::DisplayException(QString msgTitle)
{
    if (m_CompressStatusDialog)
    {
        m_CompressStatusDialog->showOutput();
        m_CompressStatusDialog->raise();
        //QTimer::singleShot(10, this, SLOT(SetRaised()));
    }
    // do some message
    try
    {
        throw;
    }
    catch (const std::exception& e)
    {
        PrintInfo("%s Error: %s (feature may not be supported!)", msgTitle.toStdString().c_str(), e.what());
    }
    catch (...)
    {
        PrintInfo("Error: Occured while processing image or model (feature may not be supported!)");
    }
}

void cpMainComponents::AddImageView(QString& fileName, QTreeWidgetItem* item)
{
    if (!(QFile::exists(fileName)))
    {
        if (m_CompressStatusDialog)
        {
            m_CompressStatusDialog->showOutput();
        }
        QString error = "Error: File " + fileName + " does not exist. Please try remove from Project and add again";
        PrintInfo(error.toStdString().c_str());
        return;
    }

    // expand any files that are relative paths to full path
    fileName = QFileInfo(fileName).canonicalFilePath();

    bool           isDel                 = true;
    bool           doRefreshCompressView = false;
    cp3DModelView* m_3Dmodelview         = NULL;
    cpImageView*   m_imageview           = NULL;
    QString        ImageType             = "";

    try
    {
        if (isCompressInProgress)
            return;

        if (!item)
            return;

        emit OnImageLoadStart();

        if (deleteImageAct)
        {
            isDel = deleteImageAct->isEnabled();
            deleteImageAct->setEnabled(false);
        }

        // Determin File Type
        QVariant v         = item->data(TREE_LevelType, Qt::UserRole);
        int      levelType = v.toInt();

        acCustomDockWidget*    dock           = NULL;
        C_Destination_Options* m_compressdata = NULL;

        bool DockItemDeleted = false;

        if (levelType == TREETYPE_COMPRESSION_DATA)
        {
            QVariant v     = item->data(TREE_SourceInfo, Qt::UserRole);
            m_compressdata = v.value<C_Destination_Options*>();
            if (m_compressdata == NULL)
                return;

            if (m_compressdata->m_data_has_been_changed)
            {
                // Find the old image and remove it
                OnDeleteImageView(fileName);
                DockItemDeleted                         = true;
                m_compressdata->m_data_has_been_changed = false;
                doRefreshCompressView                   = true;
            }
        }

        if (((g_Application_Options.m_useNewImageViews || doRefreshCompressView || g_Application_Options.m_refreshCurrentView || m_ForceImageRefresh)) &&
            (levelType != TREETYPE_3DMODEL_DATA) && (levelType != TREETYPE_3DSUBMODEL_DATA))
        {
            // Find the old image and remove it
            // flag (DockItemDeleted) is used make sure a Compressed dock item is not scanned twice for delete
            if (!DockItemDeleted)
                OnDeleteImageView(fileName);

            if (doRefreshCompressView)
            {
                if (m_compressdata)
                    m_compressdata->m_data_has_been_changed = false;
            }

            m_ForceImageRefresh = false;
        }
        else
        {
            // Make sure we are not not already viewing this image file
            dock = FindImageView(fileName, false);
        }

        if (dock)
        {
            // We found the image in our list of existing views
            m_activeImageTab = dock;
        }

        showProgressBusy("Loading ... Please wait");

        if (!g_Application_Options.m_useNewImageViews)
        {
            if (dock)
            {
                // check if the view is uptodate
                // or needs to be refreshed
                m_activeImageTab = dock;

                if (levelType == TREETYPE_COMPRESSION_DATA)
                {
                    if (doRefreshCompressView)
                    {
                        // Flag project we have new project settings to save on exit!
                        if (m_projectview)
                            m_projectview->m_saveProjectChanges = true;

                        // Remove the old tab
                        if (dock)
                        {
                            DeleteDock(&dock);
                        }
                    }
                }
            }
        }

        if (dock == NULL)
        {
            Setting* setting      = new Setting();
            setting->onBrightness = false;

            if (levelType == TREETYPE_COMPRESSION_DATA)
            {
                // Get ImageFile Data
                QVariant               v          = item->data(TREE_SourceInfo, Qt::UserRole);
                C_Destination_Options* m_filedata = v.value<C_Destination_Options*>();

                if (m_filedata)
                {
                    QFile file(fileName);
                    m_filedata->m_FileSize = file.size();
                    if (m_filedata->m_FileSize > 1024000)
                        m_filedata->m_FileSizeStr = QString().number((double)m_filedata->m_FileSize / 1024000, 'f', 2) + " MB";
                    else if (m_filedata->m_FileSize > 1024)
                        m_filedata->m_FileSizeStr = QString().number((double)m_filedata->m_FileSize / 1024, 'f', 1) + " KB";
                    else
                        m_filedata->m_FileSizeStr = QString().number(m_filedata->m_FileSize) + " Bytes";

                    // Try to get the root node
                    // for this compressed image view
                    CMipImages*      OrigImages = NULL;
                    QTreeWidgetItem* currItem   = item->parent();
                    if (currItem)
                    {
                        v                   = (currItem)->data(TREE_LevelType, Qt::UserRole);
                        int ParentlevelType = v.toInt();
                        // we need to move up a branch to models parent to get the original images
                        if (ParentlevelType == TREETYPE_3DSUBMODEL_DATA)
                        {
                            QString srcImage = m_filedata->m_sourceFileNamePath;
                            currItem         = currItem->parent();
                            if (currItem)
                            {
                                v               = (currItem)->data(TREE_LevelType, Qt::UserRole);
                                ParentlevelType = v.toInt();
                                if (ParentlevelType == TREETYPE_3DMODEL_DATA)
                                {
                                    v                    = currItem->data(TREE_SourceInfo, Qt::UserRole);
                                    C_3DModel_Info* data = v.value<C_3DModel_Info*>();
                                    if (data)
                                    {
                                        for (int i = 0; i < data->m_Model_Images.size(); i++)
                                        {
                                            if (data->m_Model_Images[i].m_isImage && (data->m_Model_Images[i].m_FilePathName.compare(srcImage) == 0))
                                            {
                                                currItem = data->m_Model_Images[i].child;
                                                break;
                                            }
                                        }
                                    }
                                    else
                                        currItem = NULL;
                                }
                                else
                                    currItem = NULL;
                            }

                            if (currItem)
                            {
                                // check its type
                                ParentlevelType = currItem->data(TREE_LevelType, Qt::UserRole).toInt();
                                if (ParentlevelType == TREETYPE_VIEWIMAGE_ONLY_NODE)
                                {
                                    v                        = currItem->data(TREE_SourceInfo, Qt::UserRole);
                                    C_Source_Info* imagedata = v.value<C_Source_Info*>();
                                    if (imagedata)
                                        OrigImages = imagedata->m_MipImages;
                                }
                            }
                        }
                        else
                        {
                            QVariant       v         = currItem->data(TREE_SourceInfo, Qt::UserRole);
                            C_Source_Info* imagedata = v.value<C_Source_Info*>();
                            if (imagedata)
                                OrigImages = imagedata->m_MipImages;
                        }
                    }

                    // Create a new view image
                    ImageType            = "Image file ";
                    setting->input_image = eImageViewState::isProcessed;
                    m_imageview          = new cpImageView(fileName, ImageType, m_parent, m_filedata->m_MipImages, setting, OrigImages);
                    if (!m_imageview)
                    {
                        QMessageBox::warning(this, "Image View", "Image is not compatible with the current decode view setting!", QMessageBox::Ok);
                        throw "";
                    }
                    else
                        m_imageview->m_type = TREETYPE_IMAGEFILE_DATA;
                }
            }
            else if ((levelType == TREETYPE_IMAGEFILE_DATA) || (levelType == TREETYPE_VIEWIMAGE_ONLY_NODE))
            {
                // Get ImageFile Data
                QVariant       v          = item->data(TREE_SourceInfo, Qt::UserRole);
                C_Source_Info* m_filedata = v.value<C_Source_Info*>();

                if (m_filedata)
                {
                    // Create a new view image
                    ImageType            = "Original Image file ";
                    setting->reloadImage = g_Application_Options.m_useNewImageViews;
                    if (m_filedata->m_MipImages->QImage_list[0].size() > 1)
                        setting->generateMips = true;
                    setting->input_image  = eImageViewState::isOriginal;
                    m_imageview           = new cpImageView(fileName, ImageType, m_parent, m_filedata->m_MipImages, setting, NULL);
                    m_imageview->m_type   = TREETYPE_IMAGEFILE_DATA;
                    setting->generateMips = false;
                }
            }
            else if ((levelType == TREETYPE_3DMODEL_DATA))
            {
                if (g_Application_Options.getGLTFRender() == C_Application_Options::RenderModelsWith::glTF_Vulkan)
                {
                    // V3.1 does not support models with no textures
                    if (!hasGLTFTextures(fileName.toStdString()))
                    {
                        QMessageBox::warning(this, "Vulkan Model Render", "Preview version : Support models with textures only", QMessageBox::Ok);
                        throw "";
                    }
                }

                m_imageview = NULL;
                // Get ImageFile Data
                QVariant        v          = item->data(TREE_SourceInfo, Qt::UserRole);
                C_3DModel_Info* m_filedata = v.value<C_3DModel_Info*>();

                if (m_filedata)
                {
                    // Create a new view image
                    ImageType             = "3D Model Render ";
                    setting->reloadImage  = g_Application_Options.m_useNewImageViews;
                    setting->generateMips = false;
                    m_3Dmodelview         = new cp3DModelView(fileName, "", ImageType, m_parent);
                    m_3Dmodelview->m_type = TREETYPE_3DMODEL_DATA;

                    setting->generateMips = false;
                    if (m_3Dmodelview->custTitleBar)
                    {
                        m_3Dmodelview->custTitleBar->m_close = true;
                        connect(m_3Dmodelview->custTitleBar, SIGNAL(OnAboutToClose(QString&)), this, SLOT(onAboutToClose(QString&)));
                    }
                }
            }
            else if (levelType == TREETYPE_3DSUBMODEL_DATA)
            {
                if (g_Application_Options.getGLTFRender() == C_Application_Options::RenderModelsWith::glTF_Vulkan)
                {
                    // V3.1 does not support models with no textures
                    if (!hasGLTFTextures(fileName.toStdString()))
                    {
                        QMessageBox::warning(this, "Vulkan Model Render", "Preview version : Support models with textures only", QMessageBox::Ok);
                        throw "";
                    }
                }

                m_imageview = NULL;
                // Get ImageFile Data
                QVariant           v          = item->data(TREE_SourceInfo, Qt::UserRole);
                C_3DSubModel_Info* m_filedata = v.value<C_3DSubModel_Info*>();

                if (m_filedata)
                {
                    // Create a new view image
                    ImageType             = "3D Model Render ";
                    setting->reloadImage  = g_Application_Options.m_useNewImageViews;
                    setting->generateMips = false;
                    m_3Dmodelview         = new cp3DModelView(fileName, "", ImageType, m_parent);
                    m_3Dmodelview->m_type = TREETYPE_3DSUBMODEL_DATA;

                    setting->generateMips = false;
                    if (m_3Dmodelview->custTitleBar)
                    {
                        m_3Dmodelview->custTitleBar->m_close = true;
                        connect(m_3Dmodelview->custTitleBar, SIGNAL(OnAboutToClose(QString&)), this, SLOT(onAboutToClose(QString&)));
                    }
                }
            }
            else if (levelType == TREETYPE_VIEWMESH_ONLY_NODE)
            {
                if (g_Application_Options.getGLTFRender() == C_Application_Options::RenderModelsWith::glTF_Vulkan)
                {
                    // V3.1 does not support models with no textures
                    if (!hasGLTFTextures(fileName.toStdString()))
                    {
                        QMessageBox::warning(this, "Vulkan Model Render", "Preview version : Support models with textures only", QMessageBox::Ok);
                        throw "";
                    }
                }

                m_imageview = NULL;
                // Get ImageFile Data
                QVariant            v          = item->data(TREE_SourceInfo, Qt::UserRole);
                C_Mesh_Buffer_Info* m_filedata = v.value<C_Mesh_Buffer_Info*>();

                if (m_filedata)
                {
                    // Create a new view image
                    ImageType             = "3D Mesh ";
                    setting->reloadImage  = g_Application_Options.m_useNewImageViews;
                    setting->generateMips = false;
                    m_3Dmodelview         = new cp3DModelView(fileName, m_filedata->m_glTF_filePath, ImageType, m_parent);
                    m_3Dmodelview->m_type = TREETYPE_MESH_DATA;

                    setting->generateMips = false;
                    if (m_3Dmodelview->custTitleBar)
                    {
                        m_3Dmodelview->custTitleBar->m_close = true;
                        connect(m_3Dmodelview->custTitleBar, SIGNAL(OnAboutToClose(QString&)), this, SLOT(onAboutToClose(QString&)));
                    }
                }
            }
            else if (levelType == TREETYPE_MESH_DATA)
            {
                if (g_Application_Options.getGLTFRender() == C_Application_Options::RenderModelsWith::glTF_Vulkan)
                {
                    // V3.1 does not support models with no textures
                    if (!hasGLTFTextures(fileName.toStdString()))
                    {
                        QMessageBox::warning(this, "Vulkan Model Render", "This version of the render support models with textures only", QMessageBox::Ok);
                        throw "";
                    }
                }

                m_imageview = NULL;
                // Get ImageFile Data
                QVariant               v              = item->data(TREE_SourceInfo, Qt::UserRole);
                C_Destination_Options* m_destmeshdata = v.value<C_Destination_Options*>();

                if (m_destmeshdata)
                {
                    // Create a new view image
                    ImageType             = "3D Mesh ";
                    setting->reloadImage  = g_Application_Options.m_useNewImageViews;
                    setting->generateMips = false;

                    QTreeWidgetItem* subModel_parent = item->parent();

                    if (subModel_parent)
                    {
                        QVariant v               = subModel_parent->data(TREE_LevelType, Qt::UserRole);
                        int      parentLevelType = v.toInt();
                        if (parentLevelType == TREETYPE_3DSUBMODEL_DATA)
                        {
                            QVariant           v             = subModel_parent->data(TREE_SourceInfo, Qt::UserRole);
                            C_3DSubModel_Info* subModel_data = v.value<C_3DSubModel_Info*>();
                            if (m_destmeshdata->m_destFileNamePath.contains(".bin") || m_destmeshdata->m_destFileNamePath.contains(".BIN"))
                            {
                                m_3Dmodelview = new cp3DModelView(fileName, subModel_data->m_Full_Path, ImageType, m_parent);
                            }
                            else
                            {
                                m_3Dmodelview = new cp3DModelView(fileName, "", ImageType, m_parent);
                            }

                            m_3Dmodelview->m_type = TREETYPE_MESH_DATA;

                            setting->generateMips = false;
                            if (m_3Dmodelview->custTitleBar)
                            {
                                m_3Dmodelview->custTitleBar->m_close = true;
                                connect(m_3Dmodelview->custTitleBar, SIGNAL(OnAboutToClose(QString&)), this, SLOT(onAboutToClose(QString&)));
                            }
                        }
                    }
                }
            }

            if (m_imageview)
            {
                m_imageview->showToobar(true);
                m_imageview->showToobarButton(true);
                m_imageview->setAllowedAreas(Qt::RightDockWidgetArea);
                m_parent->addDockWidget(Qt::RightDockWidgetArea, m_imageview);
                m_parent->tabifyDockWidget(m_blankpage, m_imageview);
                m_viewDiff = false;
                connect(m_imageview, SIGNAL(visibilityChanged(bool)), this, SLOT(onDockImageVisibilityChanged(bool)));
                connect(m_imageview, SIGNAL(UpdateData(QObject*)), m_imagePropertyView, SLOT(OnUpdateData(QObject*)));
                m_activeImageTab = m_imageview;
            }

            if (m_3Dmodelview)
            {
                m_3Dmodelview->setAllowedAreas(Qt::RightDockWidgetArea);
                m_parent->addDockWidget(Qt::RightDockWidgetArea, m_3Dmodelview);
                m_parent->tabifyDockWidget(m_blankpage, m_3Dmodelview);
                m_viewDiff = false;
                connect(m_3Dmodelview, SIGNAL(visibilityChanged(bool)), this, SLOT(onDockImageVisibilityChanged(bool)));
                connect(m_3Dmodelview, SIGNAL(UpdateData(QObject*)), m_imagePropertyView, SLOT(OnUpdateData(QObject*)));
                m_activeImageTab = m_3Dmodelview;
            }
        }

        // =================================
        // Place the TAB on view if hidden!
        // =================================
        if (m_activeImageTab)
        {
            // check its visability: User is requesting view
            if (!m_activeImageTab->isVisible())
            {
                m_activeImageTab->setVisible(true);
            }

            m_activeImageTab->raise();
            //QTimer::singleShot(10, this, SLOT(SetRaised()));
        }
    }
    catch (...)
    {
        if (m_imageview)
        {
            delete m_imageview;
            m_imageview = nullptr;
        }

        if (m_3Dmodelview)
        {
            delete m_3Dmodelview;
            m_3Dmodelview = nullptr;
        }

        DisplayException(ImageType);
    }

    emit OnImageLoadDone();

    if (deleteImageAct)
        deleteImageAct->setEnabled(isDel);
    if (saveImageAct)
        saveImageAct->setEnabled(true);

    if (g_pProgressDlg)
        g_pProgressDlg->hide();
    hideProgressBusy("Ready");
}

void cpMainComponents::AddImageDiff(C_Destination_Options* destination, QString& fileName1, QString& fileName2)
{
    try
    {
        if (isCompressInProgress)
            return;

        bool isComp = true, isDel = true;

        emit OnImageLoadStart();
        //if (compressAct)
        //{
        //    isComp = compressAct->isEnabled();
        //    compressAct->setEnabled(false);
        //}

        if (imagediffAct)
        {
            imagediffAct->setEnabled(false);
        }

        if (deleteImageAct)
        {
            isDel = deleteImageAct->isEnabled();
            deleteImageAct->setEnabled(false);
        }

        if (m_CompressStatusDialog)
        {
            m_CompressStatusDialog->onClearText();
            m_CompressStatusDialog->showOutput();
        }

        showProgressBusy("Loading Image Differance...Please wait");

        QString originalFileName = "";
        QString destFile         = "";
        QString title            = "";

        if (destination == NULL)
        {
            QFileInfo fileinfo1(fileName1);
            QFile     file1(fileName1);
            if (file1.exists() && (fileinfo1.suffix().length() > 0))
            {
                originalFileName = fileName1;
            }
            else
            {
                PrintInfo("Image Diff Error: Image File #1 cannot be found\n");
                onShowOutput();
                return;
            }

            QFileInfo fileinfo2(fileName2);
            QFile     file2(fileName2);
            if (file2.exists() && (fileinfo2.suffix().length() > 0))
            {
                destFile = fileName2;
            }
            else
            {
                PrintInfo("Image Diff Error: Image File #2 cannot be found\n");
                onShowOutput();
                return;
            }

            title = DIFFERENCE_IMAGE_TXT + originalFileName + DIFFERENCE_IMAGE_VS_TXT + destFile;
            OnDeleteImageDiffView(title);

            // Create a new view image
            m_imageCompare         = new CImageCompare(title, originalFileName, destFile, false, this);
            m_imageCompare->m_type = TREETYPE_DIFFVIEW;
        }
        else
        {
            originalFileName = destination->m_sourceFileNamePath;
            // Find the old image diff and remove it
            // User may have selected a newer Decompression Option
            // for the image diff view
            title = DIFFERENCE_IMAGE_TXT + destination->m_destFileNamePath;
            OnDeleteImageDiffView(destination->m_destFileNamePath);

            if (QDir(destination->m_decompressedFileNamePath).exists() && (QFile(destination->m_decompressedFileNamePath).size()) > 0)
            {
                destFile = destination->m_decompressedFileNamePath;
            }
            else
            {
                destFile = destination->m_destFileNamePath;
            }

            // Create a new view image
            m_imageCompare         = new CImageCompare(title, originalFileName, destFile, true, this);
            m_imageCompare->m_type = TREETYPE_DIFFVIEW;
        }

        CMipImages* m_diffMips = m_imageCompare->getMdiffMips();
        if (m_diffMips == NULL)
        {
            delete m_imageCompare;
            PrintInfo("Image Diff Error: Diff Image cannot be found\n");
            return;
        }
        else
        {
            m_imageCompare->setAllowedAreas(Qt::RightDockWidgetArea);
            m_parent->addDockWidget(Qt::RightDockWidgetArea, m_imageCompare);
            m_parent->tabifyDockWidget(m_blankpage, m_imageCompare);
            if (m_imageCompare->custTitleBar)
            {
                m_imageCompare->custTitleBar->m_close = true;
                connect(m_imageCompare->custTitleBar, SIGNAL(OnAboutToClose(QString&)), this, SLOT(onAboutToClose(QString&)));
            }
            m_viewDiff = true;
            connect(m_imageCompare, SIGNAL(visibilityChanged(bool)), this, SLOT(onDockImageVisibilityChanged(bool)));
            connect(m_imageCompare, SIGNAL(UpdateData(QObject*)), m_imagePropertyView, SLOT(OnUpdateData(QObject*)));
            m_activeImageTab = m_imageCompare;

            // check its visability: User is requesting view
            if (!m_activeImageTab->isVisible())
            {
                m_activeImageTab->setVisible(true);
            }

            m_activeImageTab->raise();
            QTimer::singleShot(30, this, SLOT(SetRaised()));
        }

        hideProgressBusy("Ready");

        emit OnImageLoadDone();

        if (imagediffAct)
            imagediffAct->setEnabled(isComp);
#ifdef USE_3DCONVERT
        if (ConvertModelAct)
            ConvertModelAct->setEnabled(isComp);
#endif
        if (deleteImageAct)
            deleteImageAct->setEnabled(isDel);
    }
    catch (...)
    {
        DisplayException("ImageDiff");
    }
}

void cpMainComponents::Add3DModelDiff(C_3DSubModel_Info* destination, QString& fileName1, QString& fileName2)
{
    Q_UNUSED(destination);

    try
    {
        if (isCompressInProgress)
            return;

        bool isComp = true, isDel = true;

        emit OnImageLoadStart();

        if (imagediffAct)
        {
            imagediffAct->setEnabled(false);
        }
#ifdef USE_3DCONVERT
        if (ConvertModelAct)
        {
            ConvertModelAct->setEnabled(false);
        }
#endif
        if (deleteImageAct)
        {
            isDel = deleteImageAct->isEnabled();
            deleteImageAct->setEnabled(false);
        }

        if (m_CompressStatusDialog)
        {
            m_CompressStatusDialog->onClearText();
            m_CompressStatusDialog->showOutput();
        }

        showProgressBusy("Loading Image Differance...Please wait");

        QString originalFileName = "";
        QString destFile         = "";
        QString title            = "";

        QFileInfo fileinfo1(fileName1);
        if (QFile::exists(fileName1) && (fileinfo1.suffix().length() > 0))
        {
            originalFileName = fileName1;
        }
        else
        {
            PrintInfo("Image Diff Error: Image File #1 cannot be found\n");
            onShowOutput();
            return;
        }

        QFileInfo fileinfo2(fileName2);
        if (QFile::exists(fileName2) && (fileinfo2.suffix().length() > 0))
        {
            destFile = fileName2;
        }
        else
        {
            PrintInfo("Image Diff Error: Image File #2 cannot be found\n");
            onShowOutput();
            return;
        }

        title = DIFFERENCE_IMAGE_TXT + originalFileName + DIFFERENCE_IMAGE_VS_TXT + destFile;
        OnDeleteImageDiffView(title);  //destFile);

        // Create a new view image
        C3DModelCompare* m_3dModelCompare = new C3DModelCompare(title, fileName1, fileName2, true, this);
        m_3dModelCompare->m_type          = TREETYPE_DIFFVIEW;

        // CMipImages  *m_diffMips = m_3dModelCompare->getMdiffMips();
        // if (m_diffMips == NULL)
        // {
        //     delete m_3dModelCompare;
        //     PrintInfo("Image Diff Error: Diff Image cannot be found\n");
        // }
        // else
        {
            m_3dModelCompare->setAllowedAreas(Qt::RightDockWidgetArea);
            m_parent->addDockWidget(Qt::RightDockWidgetArea, m_3dModelCompare);
            m_parent->tabifyDockWidget(m_blankpage, m_3dModelCompare);
            if (m_3dModelCompare->custTitleBar)
            {
                m_3dModelCompare->custTitleBar->m_close = true;
                connect(m_3dModelCompare->custTitleBar, SIGNAL(OnAboutToClose(QString&)), this, SLOT(onAboutToClose(QString&)));
            }
            m_viewDiff = true;
            connect(m_3dModelCompare, SIGNAL(visibilityChanged(bool)), this, SLOT(onDockImageVisibilityChanged(bool)));
            connect(m_3dModelCompare, SIGNAL(UpdateData(QObject*)), m_imagePropertyView, SLOT(OnUpdateData(QObject*)));
            m_activeImageTab = m_3dModelCompare;

            // check its visability: User is requesting view
            if (!m_activeImageTab->isVisible())
            {
                m_activeImageTab->setVisible(true);
            }

            m_activeImageTab->raise();
            QTimer::singleShot(30, this, SLOT(SetRaised()));
        }

        hideProgressBusy("Ready");

        emit OnImageLoadDone();

        if (imagediffAct)
            imagediffAct->setEnabled(isComp);
#ifdef USE_3DCONVERT
        if (ConvertModelAct)
            ConvertModelAct->setEnabled(isComp);
#endif
        if (deleteImageAct)
            deleteImageAct->setEnabled(isDel);
    }
    catch (...)
    {
        DisplayException("3D Model ImageDiff");
    }
}

void cpMainComponents::OnDeleteImageView(QString& fileName)
{
    showProgressBusy("Removing Image view ... Please wait");
    // Make sure we are not not already viewing this image file
    acCustomDockWidget* dock = (acCustomDockWidget*)FindImageView(fileName, false);

    if (dock)
    {
        DeleteDock(&dock);
    }

    // Also Remove any Image Diff Tabs
    OnDeleteImageDiffView(fileName);

    // Check if we need to disable any buttons from the main app!
    if (m_projectview)
    {
        int itemsCount = m_projectview->GetNumItems();
        if (deleteImageAct)
        {
            deleteImageAct->setEnabled(itemsCount > 0);
        }
    }
    hideProgressBusy("Ready");
}

void cpMainComponents::OnDeleteImageDiffView(QString& fileName)
{
    showProgressBusy("Removing Image Differance view ... Please wait");

    // Make sure we are not not already viewing this image file
    acCustomDockWidget* dock = (acCustomDockWidget*)FindImageView(fileName, true);

    // Vaid pointer else NULL
    if (dock)
    {
        // Only delete a valid assigned DiffView type
        //printf("Delete Image Diff Dock %d\n",dock->m_type);
        if (dock->m_type == TREETYPE_DIFFVIEW)
            DeleteDock(&dock);
    }

    hideProgressBusy("Ready");
}

void cpMainComponents::SetRaised()
{
    if (m_activeImageTab)
        m_activeImageTab->raise();
}

cpMainComponents::~cpMainComponents()
{
    g_bAbortCompression = true;
#ifdef _WIN32
    CMP_ShutdownDecompessLibrary();
#endif
}

void cpMainComponents::OnAddCompressSettings(QTreeWidgetItem* item)
{
    if (!item)
        return;

    QVariant v         = item->data(TREE_LevelType, Qt::UserRole);
    int      levelType = v.toInt();

    //int setting = 1;
    QString CompProjectName = "New";

    // Obtain the Parent and its data
    QTreeWidgetItem* parent = item->parent();

    // if no parent verify item itself as parent
    if (!parent)
        parent = item;

    if (parent)
    {
        // Verify its root
        QVariant v             = parent->data(TREE_LevelType, Qt::UserRole);
        int      itemlevelType = v.toInt();
        m_setcompressoptions->m_DestinationFolder->setReadOnly(false);
        m_setcompressoptions->m_PBDestFileFolder->setEnabled(true);
        m_setcompressoptions->m_DestinationData.m_SourceType = itemlevelType;

        if (itemlevelType == TREETYPE_IMAGEFILE_DATA)
        {
            QVariant       v                 = parent->data(TREE_SourceInfo, Qt::UserRole);
            C_Source_Info* m_ImageSourceFile = v.value<C_Source_Info*>();

            QFileInfo fileinfo(m_ImageSourceFile->m_Name);
            CompProjectName = fileinfo.baseName();

            m_setcompressoptions->m_DestinationData.init();
            m_setcompressoptions->m_DestinationFolder->setReadOnly(false);
            m_setcompressoptions->m_PBDestFileFolder->setEnabled(true);
            m_setcompressoptions->m_DestinationData.m_sourceFileNamePath       = m_ImageSourceFile->m_Full_Path;
            m_setcompressoptions->m_DestinationData.m_SourceImageSize          = m_ImageSourceFile->m_ImageSize;
            m_setcompressoptions->m_DestinationData.m_SourceIscompressedFormat = CMP_IsCompressedFormat(m_ImageSourceFile->m_Format);
            m_setcompressoptions->m_DestinationData.m_SourceIsFloatFormat      = CMP_IsFloatFormat(m_ImageSourceFile->m_Format);

            // Used to append to name - for unique name
            // There is still chances of duplucate names, but it will not effect
            // compression unless target file is also of the same name as any other child
            // compression settings
            int parentcount = parent->childCount();

            if (m_ImageSourceFile->m_extnum <= parentcount)
                m_ImageSourceFile->m_extnum = parentcount;

            // Extension Counter Number incriment
            m_setcompressoptions->m_extnum = m_ImageSourceFile->m_extnum++;

            // Set image target size
            m_setcompressoptions->m_DestinationData.m_DstWidth  = m_ImageSourceFile->m_Width;
            m_setcompressoptions->m_DestinationData.m_DstHeight = m_ImageSourceFile->m_Height;

            // Set Compression Widgets to enable
            m_setcompressoptions->m_showDestinationEXTSetting = true;
            m_setcompressoptions->m_showTheControllerSetting  = true;
            m_setcompressoptions->m_showTheInfoTextSetting    = true;

            // List of source files - for am image file there is only one source file
            // clean up combo list
            m_setcompressoptions->m_CBSourceFile->clear();

            QFileInfo fi(m_setcompressoptions->m_DestinationData.m_sourceFileNamePath);
            QString   name = fi.fileName();
            m_setcompressoptions->m_CBSourceFile->addItem(name);
        }
        else if (itemlevelType == TREETYPE_3DMODEL_DATA)
        {
            m_setcompressoptions->m_DestinationData.m_SourceType = itemlevelType;
            QVariant        v                                    = parent->data(TREE_SourceInfo, Qt::UserRole);
            C_3DModel_Info* m_3DModelSourceFile                  = v.value<C_3DModel_Info*>();

            // Source File Name
            QFileInfo fileinfo(m_3DModelSourceFile->m_Name);
            CompProjectName = fileinfo.baseName();

            m_setcompressoptions->m_DestinationData.init();
            m_setcompressoptions->m_DestinationFolder->setReadOnly(true);
            m_setcompressoptions->m_PBDestFileFolder->setEnabled(false);
            m_setcompressoptions->m_DestinationData.m_sourceFileNamePath = m_3DModelSourceFile->m_Full_Path;

            m_3DModelSourceFile->m_extnum++;

            // Extension Counter Number incriment
            m_setcompressoptions->m_extnum = m_3DModelSourceFile->m_extnum++;

            // List of source files
            if (levelType == TREETYPE_Add_Model_destination_settings)
            {
                // Set Compression Widgets to enable
                m_setcompressoptions->m_showDestinationEXTSetting = false;
                m_setcompressoptions->m_showTheControllerSetting  = false;
                m_setcompressoptions->m_showTheInfoTextSetting    = false;
                m_setcompressoptions->m_CBSourceFile->clear();
                QFileInfo fi(m_setcompressoptions->m_DestinationData.m_sourceFileNamePath);
                QString   name = fi.fileName();
                m_setcompressoptions->m_CBSourceFile->addItem(name);
                m_setcompressoptions->m_DestinationData.m_modelSource = m_3DModelSourceFile->m_Full_Path;
            }
            else
            {
                m_setcompressoptions->m_CBSourceFile->clear();
            }
        }
        else if (itemlevelType == TREETYPE_3DSUBMODEL_DATA)
        {
            if (levelType != TREETYPE_Add_destination_setting)
                return;  // noting to do with compression settings!

            // copy 3d sub src data from 3d src models
            QVariant           v                      = parent->data(TREE_SourceInfo, Qt::UserRole);
            C_3DSubModel_Info* m_3DSubModelSourceFile = v.value<C_3DSubModel_Info*>();
            if (!m_3DSubModelSourceFile)
                return;

            if (m_3DSubModelSourceFile->m_Model_Images.isEmpty())
            {
                PrintInfo("Error: There is no texture found in the glTF file.");
                onShowOutput();
                return;
            }

            // Is 3d src del flag set to true
            bool canSetValues = false;
            int  defaultIndex = 0;
            while (defaultIndex < m_3DSubModelSourceFile->m_Model_Images.size())
            {
                if (m_3DSubModelSourceFile->m_SubModel_Images[defaultIndex].m_srcDelFlag == false)
                {
                    canSetValues = true;
                    break;
                }
                defaultIndex++;
            }

            if (canSetValues == false)
            {
                PrintInfo(
                    "Error: All setting for the file has been added. \n Note: To add/ modify setting, please add new setting or modify the current setting "
                    "through properties window.");
                onShowOutput();
                return;
            }

            m_setcompressoptions->m_DestinationFolder->setReadOnly(true);
            m_setcompressoptions->m_PBDestFileFolder->setEnabled(false);
            m_setcompressoptions->m_DestinationData.m_Model_Images    = m_3DSubModelSourceFile->m_Model_Images;
            m_setcompressoptions->m_DestinationData.m_SubModel_Images = m_3DSubModelSourceFile->m_SubModel_Images;

            // Let assume for now that we are using PBR images which all have the same sizes
            if (m_setcompressoptions->m_DestinationData.m_Model_Images.size() > 0)
            {
                m_setcompressoptions->m_DestinationData.m_DstWidth  = m_3DSubModelSourceFile->m_Model_Images[defaultIndex].m_Width;
                m_setcompressoptions->m_DestinationData.m_DstHeight = m_3DSubModelSourceFile->m_Model_Images[defaultIndex].m_Height;
            }

            // Set Compression Widgets to enable
            m_setcompressoptions->m_showDestinationEXTSetting = true;
            m_setcompressoptions->m_showTheControllerSetting  = true;
            m_setcompressoptions->m_showTheInfoTextSetting    = true;

            m_setcompressoptions->m_CBSourceFile->clear();
            for (int i = 0; i < m_setcompressoptions->m_DestinationData.m_Model_Images.size(); ++i)
            {
                // check for delete flags
                if (m_setcompressoptions->m_DestinationData.m_SubModel_Images[i].m_srcDelFlag == false)
                {
                    QFileInfo fi(m_setcompressoptions->m_DestinationData.m_Model_Images[i].m_FilePathName);
                    QString   name = fi.fileName();
                    m_setcompressoptions->m_CBSourceFile->addItem(name, QVariant(m_setcompressoptions->m_DestinationData.m_SubModel_Images[i].m_isImage));
                }
            }

            m_setcompressoptions->m_DestinationData.m_modelDest   = m_3DSubModelSourceFile->m_Full_Path;
            m_setcompressoptions->m_DestinationData.m_modelSource = m_3DSubModelSourceFile->m_ModelSource_gltf;

            // Default to first entry in the source combolist
            QString srcPath = "";
            if (m_setcompressoptions->m_DestinationData.m_Model_Images.isEmpty())
            {
                QFileInfo srcfileInfo(m_setcompressoptions->m_DestinationData.m_modelSource);
                srcPath = srcfileInfo.absolutePath();
            }
            else
            {
                QFileInfo srcfileInfo(m_setcompressoptions->m_DestinationData.m_Model_Images[defaultIndex].m_FilePathName);
                srcPath = srcfileInfo.absolutePath();
            }

            m_setcompressoptions->m_DestinationData.m_isModelData = !m_setcompressoptions->m_DestinationData.m_Model_Images[defaultIndex].m_isImage;

            m_setcompressoptions->m_DestinationData.m_sourceFileNamePath = srcPath + "/" + m_setcompressoptions->m_CBSourceFile->itemText(0);

            QFileInfo fileinfo(m_setcompressoptions->m_DestinationData.m_sourceFileNamePath);
            CompProjectName = fileinfo.baseName();

            int parentcount = parent->childCount();
            if (m_3DSubModelSourceFile->m_extnum <= parentcount)
                m_3DSubModelSourceFile->m_extnum = parentcount;

            // Extension Counter Number incriment
            m_setcompressoptions->m_extnum = m_3DSubModelSourceFile->m_extnum++;
            if ((m_3DSubModelSourceFile->ModelType == eModelType::OBJ) && (parent != item))
            {
                m_setcompressoptions->GBDestinationFile->setVisible(false);
            }
            else
            {
                // we are adding a OBJ or GLTF destination file
                m_setcompressoptions->GBDestinationFile->setVisible(true);
            }
        }
        m_setcompressoptions->m_DestinationData.m_SourceType = itemlevelType;
    }

    m_setcompressoptions->m_DestinationData.m_compname = CompProjectName;

    m_setcompressoptions->m_DestinationData.m_editing = false;
    m_setcompressoptions->m_item                      = item;

    emit m_setcompressoptions->m_DestinationData.compressionChanged((QVariant&)m_setcompressoptions->m_DestinationData.m_Compression);

    if (m_setcompressoptions->updateDisplayContent())
    {
        if (!m_setcompressoptions->isVisible())
        {
            QPoint pos = QCursor::pos();
            m_setcompressoptions->move(pos);
            m_setcompressoptions->show();
        }
    }
}

void cpMainComponents::onAddedImageSourceNode()
{
    if (compressAct)
        compressAct->setEnabled(true);
}

void cpMainComponents::onAddedCompressSettingNode()
{
    if (compressAct)
        compressAct->setEnabled(true);
    m_setcompressoptions->m_destFilePath = m_setcompressoptions->m_DestinationFolder->text();
}

void cpMainComponents::onEditCompressSettings(QTreeWidgetItem* item)
{
    QVariant               v      = item->data(TREE_SourceInfo, Qt::UserRole);
    C_Destination_Options* m_data = v.value<C_Destination_Options*>();
    if (m_data)
    {
        m_setcompressoptions->m_item = item;
        m_setcompressoptions->m_DestinationData << (const C_Destination_Options&)*m_data;

        if (m_setcompressoptions->updateDisplayContent())
        {
            if (!m_setcompressoptions->isVisible())
            {
                QPoint pos = QCursor::pos();
                m_setcompressoptions->move(pos);
                m_setcompressoptions->show();
            }
        }
    }
}

//==========================================
// Static Members used to redirect
// Messages to GUI
//==========================================
bool    isCompressMSG = false;
QString comError      = "Failed to initialize COM";

// ----------------------------------
// Messages from command line prints
// ----------------------------------

void cpMainComponents::PrintStatus(char* buff)
{
    //qDebug() << buff;
    isCompressMSG = true;
    QString msg   = buff;
    if (msg.contains(comError))
        return;

    emit static_msghandler.signalMessage(buff);
}

// ----------------------------------
// Messages from qDebug()
// ----------------------------------

void cpMainComponents::msgHandler(QtMsgType type, const char* msg)
{
    Q_UNUSED(type);
    Q_UNUSED(msg);
    //emit static_msghandler.signalMessage(msg);
}

void cpMainComponents::browserMsg(const char* msg)
{
    //statusBar()->showMessage(msg);
    if (m_CompressStatusDialog && isCompressMSG)
    {
        QString qmsg = msg;
        qmsg.remove(QRegExp("[\\n\\r]"));
        m_CompressStatusDialog->appendText(qmsg);
        isCompressMSG = false;
    }
}

//================================================

void cpMainComponents::removeItemTabs(QString* FilePathName)
{
    QTreeWidgetItem* item = m_projectview->GetCurrentItem(TREETYPE_COMPRESSION_DATA);
    if (item)
    {
        // qDebug() << "Delete this Tab: " << *FilePathName;
        // view image
        QVariant               v      = item->data(TREE_SourceInfo, Qt::UserRole);
        C_Destination_Options* m_data = v.value<C_Destination_Options*>();
        if (m_data)
        {
            if ((m_data->m_destFileNamePath.compare(*FilePathName) == 0) || (m_data->m_destFileNamePath.compare(DIFFERENCE_IMAGE_TXT + *FilePathName) == 0))
            {
                OnDeleteImageView(*FilePathName);
                OnDeleteImageDiffView(*FilePathName);
                m_activeImageTab = NULL;
                if (m_projectview)
                {
                    m_projectview->Tree_updateCompressIcon(item, *FilePathName, false);
                    m_projectview->m_saveProjectChanges = true;
                }
            }
        }
    }
}

void cpMainComponents::onPropertyViewSaveSetting(QString* FilePathName)
{
    if (m_projectview)
    {
        QFile::remove(*FilePathName);
        removeItemTabs(FilePathName);
        m_projectview->Tree_SetCurrentItem(*FilePathName);
    }
}

void cpMainComponents::onPropertyViewCompressImage(QString* FilePathName)
{
    if (m_projectview)
    {
        removeItemTabs(FilePathName);
        m_projectview->Tree_clearAllItemsSelected();
        QTreeWidgetItem* item = m_projectview->Tree_SetCurrentItem(*FilePathName);
        if (item != NULL)
        {
            QVariant               v      = item->data(TREE_SourceInfo, Qt::UserRole);
            C_Destination_Options* m_data = v.value<C_Destination_Options*>();
            if (m_data)
            {
                m_data->m_isselected = true;
                m_projectview->CompressSelectedItems();
            }
            else
            {
                C_3DSubModel_Info* m_modeldata = v.value<C_3DSubModel_Info*>();
                if (m_modeldata)
                {
                    QTreeWidgetItemIterator it(item);
                    while (*it)
                    {
                        QVariant vc        = (*it)->data(TREE_LevelType, Qt::UserRole);
                        int      levelType = vc.toInt();
                        if (levelType == TREETYPE_COMPRESSION_DATA || levelType == TREETYPE_MESH_DATA)
                        {
                            v                           = (*it)->data(TREE_SourceInfo, Qt::UserRole);
                            C_Destination_Options* data = v.value<C_Destination_Options*>();
                            if (data)
                            {
                                data->m_isselected = true;
                                (*it)->setSelected(true);
                                break;
                            }
                        }

                        it++;
                    }

                    m_projectview->CompressSelectedItems();
                }
            }
        }
    }
}

void cpMainComponents::onCompressionStart()
{
    // Disable relavent tool bar options
    if (compressAct)
        compressAct->setEnabled(false);
    if (imagediffAct)
        imagediffAct->setEnabled(false);
#ifdef USE_3DCONVERT
    if (ConvertModelAct)
        ConvertModelAct->setEnabled(false);
#endif
    if (deleteImageAct)
        deleteImageAct->setEnabled(false);
    if (MIPGenAct)
        MIPGenAct->setEnabled(false);

    // Free up as much memory as we can
    // prior to processing
    if (g_Application_Options.m_closeAllDocuments)
        onCloseAllDocuments();

    isCompressInProgress = true;
}

void cpMainComponents::onCompressionDone()
{
    isCompressInProgress = false;

    if (m_projectview)
    {
        // check if any items prior to compressing!
        int itemsCount = m_projectview->GetNumDestItems();
        if (itemsCount > 0)
        {
            // Called when compression from Project view is completed.
            QTreeWidgetItem* item = m_projectview->GetCurrentItem(TREETYPE_COMPRESSION_DATA);
            if (item)
            {
                // view image
                QVariant               v      = item->data(TREE_SourceInfo, Qt::UserRole);
                C_Destination_Options* m_data = v.value<C_Destination_Options*>();
                if (m_data)
                {
                    // Refresh any changes in current items data
                    // like compression time
                    if (m_imagePropertyView)
                        m_imagePropertyView->OnUpdateData(m_data);

                    // Add the image to the diff image list
                    if (m_projectview)
                    {
                        // Add the image to the diff image list if it is not in the list
                        if ((!(m_projectview->m_ImagesinProjectTrees.contains(m_data->m_destFileNamePath))) &&
                            (!(m_data->m_destFileNamePath.contains(".gltf"))))
                            m_projectview->m_ImagesinProjectTrees.append(m_data->m_destFileNamePath);
                    }

#ifdef SHOW_DECOMPRESS_IMAGE
                    // This can cause issue if image decompresion takes a long time!
                    AddImageView(m_data->m_destFileNamePath, item);
#endif
                }
            }
        }
    }

    // Re-enable relavent tool bar options
    if (compressAct)
        compressAct->setEnabled(true);
    if (imagediffAct)
        imagediffAct->setEnabled(true);
#ifdef USE_3DCONVERT
    if (ConvertModelAct)
        ConvertModelAct->setEnabled(true);
#endif
    if (deleteImageAct)
        deleteImageAct->setEnabled(true);
}

// in future revisions: calls to onSourceImage() should be replaced newer code onSetToolBarActions()
void cpMainComponents::onSourceImage(int childCount)
{
    if (MIPGenAct)
        MIPGenAct->setEnabled(true);
    if (deleteImageAct)
        deleteImageAct->setEnabled(true);
#ifdef USE_3DCONVERT
    if (ConvertModelAct)
        ConvertModelAct->setEnabled(false);
#endif
    if (saveToBatchFileAct)
    {
        if (!saveToBatchFileAct->isEnabled())
            saveToBatchFileAct->setEnabled(childCount > 1);
    }
}

void cpMainComponents::onProjectLoaded(int childCount)
{
    if (saveToBatchFileAct)
    {
        saveToBatchFileAct->setEnabled(childCount > 1);
    }

    if (m_imagePropertyView)
        m_imagePropertyView->OnUpdateData(NULL);
}

void cpMainComponents::onDecompressImage()
{
    if (imagediffAct)
        imagediffAct->setEnabled(true);
#ifdef USE_3DCONVERT
    if (ConvertModelAct)
        ConvertModelAct->setEnabled(true);
#endif
    if (MIPGenAct)
        MIPGenAct->setEnabled(false);
    if (deleteImageAct)
        deleteImageAct->setEnabled(true);

    if (saveToBatchFileAct)
    {
        if (!saveToBatchFileAct->isEnabled())
            saveToBatchFileAct->setEnabled(true);
    }
}

void cpMainComponents::OpenCHMFile(QString fileName)
{
    QString str;

    str.append(URL_FILE);
    str.append(qApp->applicationDirPath());
    str.append("/");
    str.append(fileName);
    if (!QDesktopServices::openUrl(QUrl(str)))
    {
        char* ENV;
        str = "";
        ENV = getenv("COMPRESSONATOR_ROOT");
        if (ENV)
        {
            str.append(URL_FILE);
            str.append(ENV);
            str.append("/");
            str.append(fileName);
            QDesktopServices::openUrl(QUrl(str));
        }
    }
}

void cpMainComponents::userGuide()
{
    OpenCHMFile(COMPRESSONATOR_USER_GUIDE);
}

void cpMainComponents::newFeaturesGuide()
{
    OpenCHMFile(COMPRESSONATOR_NEWFEATURES_GUIDE);
}

void cpMainComponents::gettingStarted()
{
    OpenCHMFile(COMPRESSONATOR_GETTING_STARTED);
}

void cpMainComponents::onProcessing(QString& FilePathName)
{
    Q_UNUSED(FilePathName);
    // Reserved for future use
    // to handle any action just before a file is processed
    // by command line.
}

// Removes all Docked Views that contains Title
void cpMainComponents::onAboutToClose(QString& Title)
{
    QList<acCustomDockWidget*> dockWidgets = m_parent->findChildren<acCustomDockWidget*>();
    acCustomDockWidget*        dock;

    for (int i = 0; i < dockWidgets.size(); i++)
    {
        dock = dockWidgets[i];
        if (dock)
        {
            QString DockTitle = dock->custTitleBar->getTitle();
            if (DockTitle.compare(Title) == 0)
            {
                //skip diff subwindow which are already deleted
                if (dock->m_type == TREETYPE_DIFFVIEW)
                {
                    i += 3;
                }
                DeleteDock(&dock);
            }
        }
    }
}
