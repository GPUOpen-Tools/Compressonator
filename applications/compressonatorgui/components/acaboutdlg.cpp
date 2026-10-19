#include "acaboutdlg.h"
#include "version.h"

#define AC_STR_HelpAboutWebPage "https://gpuopen.com/compressonator/"
#define AC_STR_HelpAboutWebPageText "gpuopen.com"

CHelpAboutDialog::CHelpAboutDialog(QWidget* parent)
    : QDialog(parent)
{
    Init("About Compressonator", "Compressonator");
}

void CHelpAboutDialog::Init(const QString& title, const QString& productName)
{
    Q_UNUSED(productName);

    // Set the dialog title:
    setWindowTitle(title);
    setWindowIcon(QIcon(":/compressonatorgui/images/acompress-256.png"));

    // Set window flags (minimize / maximize / close buttons):
    Qt::WindowFlags flags = windowFlags();
    flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);

    // layout components:
    QHBoxLayout* pMainLayout = new QHBoxLayout;
    pMainLayout->setSizeConstraint(QLayout::SetFixedSize);

    QVBoxLayout* pVerticalLayoutLeft = new QVBoxLayout;
    QString      version             = QString("%1.%2.%3")
                          .arg(QString::number(VERSION_MAJOR_MAJOR), QString::number(VERSION_MAJOR_MINOR), QString::number(VERSION_MINOR_MAJOR)
                               //QString::number(0)
                          );
    // Set the Help about string:
    QString helpAboutString = "Compressonator<br>";
    helpAboutString.append(version);

    QString copyRightCaption = "<p> &copy; 2024 Advanced Micro Devices, Inc. All Rights Reserved.</p>";
    // Create the QLabels from the strings:
    QLabel* pHelpAboutStringAsStaticText  = new QLabel(helpAboutString);
    QLabel* pCopyRightCaptionAsStaticText = new QLabel(copyRightCaption);

    // Set QLabels Alignments
    pHelpAboutStringAsStaticText->setAlignment(Qt::AlignCenter);
    pCopyRightCaptionAsStaticText->setAlignment(Qt::AlignCenter);
    // Add the web page link:
    QString websiteURL  = "<a href=\"" AC_STR_HelpAboutWebPage "\">" AC_STR_HelpAboutWebPageText "</a>";
    QLabel* pWebsiteURL = new QLabel(websiteURL);

    pWebsiteURL->setOpenExternalLinks(true);
    pWebsiteURL->setToolTip(AC_STR_HelpAboutWebPageText);

    // Add Horizontal Line:
    QFrame* pLine = new QFrame();

    pLine->setFrameShape(QFrame::HLine);
    pLine->setFrameShadow(QFrame::Sunken);

    // Add the OK button:
    QDialogButtonBox* pOKButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Vertical);

    connect(pOKButtonBox, SIGNAL(accepted()), this, SLOT(accept()));

    // Left Vertical Layout:
    pVerticalLayoutLeft->addStretch(1);
    pVerticalLayoutLeft->addWidget(pHelpAboutStringAsStaticText, 0, Qt::AlignCenter | Qt::AlignBottom);
    pVerticalLayoutLeft->addSpacing(5);
    pVerticalLayoutLeft->addWidget(pWebsiteURL, 0, Qt::AlignCenter | Qt::AlignBottom);
    pVerticalLayoutLeft->addStretch(1);
    pVerticalLayoutLeft->addWidget(pLine);
    pVerticalLayoutLeft->addWidget(pCopyRightCaptionAsStaticText, 0, Qt::AlignCenter);
    pVerticalLayoutLeft->addSpacing(5);
    pVerticalLayoutLeft->addWidget(pOKButtonBox, 0, Qt::AlignCenter | Qt::AlignBottom);
    pVerticalLayoutLeft->setMargin(10);

    // Main Horizontal Layout:
    //pMainLayout->addLayout(pVerticalLayoutRight);
    pMainLayout->addLayout(pVerticalLayoutLeft);
    pMainLayout->setMargin(0);

    // Activate:
    setLayout(pMainLayout);
}

CHelpAboutDialog::~CHelpAboutDialog()
{
}
