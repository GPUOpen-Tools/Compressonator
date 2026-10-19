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

#ifndef _WELCOMEPAGE_H
#define _WELCOMEPAGE_H

#include <QtWidgets>

#ifdef USE_QTWEBENGINE
#include <QtWebEngineWidgets/qwebengineview.h>
#endif

#include "cpstartuppage.h"
#include "accustomdockwidget.h"

class CWelcomePage : public acCustomDockWidget
{
    Q_OBJECT

public:
    CWelcomePage(const QString title, QWidget* parent);
    ~CWelcomePage();

    void GoToAMDHomePage(QVector<QString>& projectsNames);

    cpStartupPage*        m_startUpPage;
    acDockWidgetTitlebar* custTitleBar;

signals:

    void WebPageButtonClick(QString& Request, QString& Msg);

public slots:
    void onWebPageButtonClick(QString& Request, QString& Msg);

private:
    QString m_homePage;

    // Common for all
    QWidget*      m_newWidget;
    QGridLayout*  m_layout;
    const QString m_title;
    QWidget*      m_parent;
};

#define COMPRESSONATOR_HOME "https://gpuopen.com/compressonator/"
#define URL_FILE "file:///"
#define COMPRESSONATOR_GETTING_STARTED "Documents/gui_tool/getting_started/gettingstarted.html"
#define COMPRESSONATOR_USER_GUIDE "Documents/gui_tool/user_guide/index.html"
#define COMPRESSONATOR_NEWFEATURES_GUIDE "Documents/revisions.html"

#endif
