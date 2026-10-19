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
/// \file acImageView.cpp
/// \version 2.21
//
//=====================================================================

#include "acimageview.h"
#include "format_conversion.h"

#define CURSOR_SIZE 12  // pixel per cross hair fin

acImageView::~acImageView()
{
    if (m_imageloader)
        delete m_imageloader;
}

// ---------------------------------------------------------------------------
// Name:        acImageView::acImageView
// Description: Constructor
//              builds a QGraphics based image viewer widget that contains
//              a navigation view default (lower right corner)
// Date:        13/8/2015
// ---------------------------------------------------------------------------
acImageView::acImageView(const QString filePathName, QWidget* parent, CMipImages* OriginalMipImages, CMipImages* MipImages)
{
    Q_UNUSED(parent);

    m_MipImages         = MipImages;
    m_OriginalMipImages = OriginalMipImages;

    m_imageloader = new CImageLoader();

    m_layout = new QGridLayout(this);
    m_layout->setSpacing(0);
    m_layout->setMargin(0);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_imageGraphicsView   = NULL;
    m_graphicsScene       = NULL;
    m_imageItem_Processed = NULL;
    m_imageItemNav        = NULL;
    m_errMessage          = NULL;
    //Reserved: GPUDecode

    m_navVisible      = false;
    m_isDiffView      = false;
    m_currentMiplevel = 0;
    m_depthIndex      = 0;
#ifdef _DEBUG
    m_debugMode   = false;
    m_debugFormat = "";
#endif

    m_imageOrientation = 0;
    m_ImageScale       = 100;
    m_MouseHandDown    = false;
    m_appBusy          = false;  // Set to true when ImageView is processing data from a prior event

    for (int ml = 0; ml < MAX_MIPLEVEL_SUPPORTED; ml++)
        for (int dl = 0; dl < 6; dl++)
            m_PSNR[ml][dl] = 0.0;

    // Display if we have images
    if (m_MipImages)
    {
        if (m_MipImages->QImage_list[0][0] != NULL)
            for (int ii = 0; ii < CMP_MIPSET_MAX_DEPTHS; ii++)
            {
                if (m_MipImages->QImage_list[ii].size() > 0)
                {
                    // The scene is at 0,0 and set to the size of this display widget
                    m_graphicsScene = new acCustomGraphicsScene(this);

                    QSize size = this->size();

                    m_graphicsScene->setSceneRect(0, 0, size.width(), size.height());
                    m_graphicsScene->setBackgroundBrush(QBrush(Qt::black, Qt::SolidPattern));

                    //==========================
                    // Add a QImage to the scene
                    //
                    // An image item this is been used for the main view
                    // Note if a compressed image is loaded ie BCn DDS file
                    // We will still have a default image file that is used as a temp
                    // The temp file is loaded via resource file ie: ":/compressonatorgui/images/compressedimageerror.png"
                    //==========================
                    m_ImageIndex           = 0;
                    QImage* image_original = NULL;
                    QPixmap pixmap_original;

                    if (m_OriginalMipImages)
                    {
                        image_original           = m_OriginalMipImages->QImage_list[m_depthIndex][m_ImageIndex];
                        pixmap_original          = QPixmap::fromImage(*image_original, Qt::NoFormatConversion);
                        m_imageItem_Original     = new acCustomGraphicsImageItem(pixmap_original, NULL);
                        m_imageItem_Original->ID = m_graphicsScene->ID;
                        m_imageItem_Original->setFlags(QGraphicsItem::ItemIsSelectable);
                        m_graphicsScene->addItem(m_imageItem_Original);
                        m_imageItem_Original->setVisible(false);
                    }
                    else
                        m_imageItem_Original = NULL;

                    QImage* image_processed = m_MipImages->QImage_list[m_depthIndex][m_ImageIndex];

                    if (image_processed != NULL)
                    {
                        QPixmap pixmap_processed = QPixmap::fromImage(*image_processed, Qt::NoFormatConversion);

                        m_imageItem_Processed     = new acCustomGraphicsImageItem(pixmap_processed, image_original);
                        m_imageItem_Processed->ID = m_graphicsScene->ID;
                        m_imageItem_Processed->setFlags(QGraphicsItem::ItemIsSelectable);
                        m_graphicsScene->addItem(m_imageItem_Processed);
                    }

                    m_imageItem_Processed->setVisible(true);

#ifdef ENABLE_NAVIGATION
                    // Copy of the image view item (ToDo->Scale down to fit a smalled size)
                    // it will be positioned to bottom left corner of the graphics view
                    // and hidden by default, the view toggles on or off based on when
                    // a navigateButton is clicked
                    m_imageItemNav     = new acCustomGraphicsNavImageItem(QPixmap::fromImage((*image)));
                    m_imageItemNav->ID = m_graphicsScene->ID;
                    m_imageItemNav->setVisible(false);
                    m_imageItemNav->setFlags(QGraphicsItem::ItemIsSelectable);
                    m_graphicsScene->addItem(m_imageItemNav);

                    m_navigateButton = new QPushButton();
                    if (m_MipImages->MIPS2QtFailed)
                    {
                        QString PushButtonStyle(
                            "QPushButton {background: red; border:none; margin: 0px; padding: 0px } QPushButton:hover {border:1px solid black}");
                        m_navigateButton->setStyleSheet(PushButtonStyle);
                    }

                    QPixmap pixmap(qt_navigate_png);

                    m_graphicsScene->addWidget(m_navigateButton);

                    // a scalled transparent window that is a child of the navigation view
                    // ToDo(s)
                    //   1.  when its moved the view of the main image will pan to match the box area
                    //   2.  scale the size of the box to that of acImageView widget size
                    //   3.  Keep the bounds of the move within the navigation window
                    acCustomGraphicsNavWindow* m_navWindow = new acCustomGraphicsNavWindow(QRectF(0, 0, 50, 50), m_imageItemNav);
                    m_navWindow->ID                        = m_graphicsScene->ID;
                    m_navWindow->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
                    m_navWindow->setOpacity(0.5);
                    m_navWindow->setPen(QPen(Qt::black));
                    m_navWindow->setBrush(Qt::white);
                    m_navigateButton->hide();
#endif

                    //==========================
                    // Add Error message to the scene if there is error
                    //==========================
                    if (m_MipImages->errMsg != "")
                    {
                        m_errMessage = new QLabel();
                        if (m_errMessage != NULL)
                        {
                            QFont font = m_errMessage->font();
                            font.setBold(true);
                            font.setItalic(true);
                            font.setPointSize(size.width() / 40);
                            m_errMessage->setWordWrap(true);
                            m_errMessage->setFont(font);
                            m_errMessage->setFixedSize(size.width(), size.height());
                            m_errMessage->setText(QString::fromStdString(m_MipImages->errMsg));
                            m_errMessage->setAlignment(Qt::AlignCenter);
                            m_graphicsScene->addWidget(m_errMessage);
                        }
                    }

                    // The widget viewer for all of the items
                    m_imageGraphicsView     = new acCustomGraphicsView();
                    m_imageGraphicsView->ID = m_graphicsScene->ID;
                    m_imageGraphicsView->setVisible(false);
                    m_imageGraphicsView->setScene(m_graphicsScene);
                    m_imageGraphicsView->setFrameShadow(QFrame::Raised);
                    m_imageGraphicsView->centerOn(0, 0);
                    m_layout->addWidget(m_imageGraphicsView, 0, 0);
                    m_imageGraphicsView->ensureVisible(m_imageItem_Processed);
                    if (m_imageItem_Original)
                        m_imageGraphicsView->ensureVisible(m_imageItem_Original);
                    m_imageGraphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                    m_imageGraphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

                    m_xPos = m_imageGraphicsView->horizontalScrollBar();
                    if (m_xPos)
                    {
                        // These values should be refreshed each time the acImageView is resized
                        // for now we are obtaining the constructors defaults
                        m_defaultXPos_minimum = m_xPos->minimum();
                        m_defaultXPos_maximum = m_xPos->maximum();
                    }

                    m_yPos = m_imageGraphicsView->verticalScrollBar();
                    if (m_yPos)
                    {
                        // These values should be refreshed each time the acImageView is resized
                        // for now we are obtaining the constructors defaults
                        m_defaultYPos_minimum = m_yPos->minimum();
                        m_defaultYPos_maximum = m_yPos->maximum();
                    }

                    // Public : Events from acCustomGraphics
                    connect(m_graphicsScene, SIGNAL(sceneMousePosition(QPointF*, int)), this, SLOT(onacImageViewMousePosition(QPointF*, int)));
                    connect(m_imageGraphicsView, SIGNAL(resetImageView()), this, SLOT(onResetImageView()));
                    connect(m_imageGraphicsView, SIGNAL(OnMouseHandDown()), this, SLOT(onMouseHandDown()));
                    connect(m_imageGraphicsView, SIGNAL(OnMouseHandD()), this, SLOT(onMouseHandD()));

                    connect(m_imageGraphicsView, SIGNAL(OnWheelScaleUp(QPointF&)), this, SLOT(onWheelScaleUp(QPointF&)));
                    connect(m_imageGraphicsView, SIGNAL(OnWheelScaleDown(QPointF&)), this, SLOT(onWheelScaleDown(QPointF&)));

#ifdef ENABLE_NAVIGATION
                    connect(m_navigateButton, SIGNAL(released()), this, SLOT(onNavigateClicked()));
#endif

                    //===============================
                    // Add a OpenGL Widget if needed
                    // to the GraphicsView
                    //===============================
                    if (m_MipImages && (m_MipImages->m_MipImageFormat == MIPIMAGE_FORMAT::Format_OpenGL))
                    {
                        if (m_MipImages->mipset)
                        {
                            if (m_MipImages->mipset->m_compressed)
                            {
                                //Reserved: GPUDecode
                            }
                        }
                    }

                    //==========================
                    // Shift the position of the
                    // added error message
                    //==========================
                    if (m_MipImages->errMsg != "")
                    {
                        QGraphicsItem* m_errItem = m_imageGraphicsView->itemAt(0, 0);
                        if (m_errItem)
                            m_errItem->moveBy(size.width() / 4, size.height() / 4);

                        if (m_errMessage)
                        {
                            m_imageItem_Processed->setVisible(false);
                            if (m_imageItem_Original)
                                m_imageItem_Original->setVisible(false);
                        }
                    }

                    //==========================
                    // Add a image data Table
                    // to the GraphicsView
                    //==========================

                    if ((m_MipImages->m_Error == MIPIMAGE_FORMAT_ERRORS::Format_NoErrors) && image_processed)
                    {
                        QRect ImageSize = image_processed->rect();
                        m_myModel       = new acTableImageDataModel(ImageSize.height(), ImageSize.width(), this);

                        m_tableView = new QTableView(this);
                        m_tableView->setAutoScroll(true);
                        m_tableView->setModel(m_myModel);

                        QWidget*     newWidget = new QWidget();
                        QHBoxLayout* layout    = new QHBoxLayout();
                        layout->addWidget(m_tableView);
                        newWidget->setLayout(layout);

                        m_graphicsScene->addWidget(newWidget);
                        m_tableViewitem = m_imageGraphicsView->itemAt(0, 0);
                        if (m_tableViewitem)
                        {
                            m_tableViewitem->setOpacity(0.75);
                            m_tableViewitem->hide();
                        }
                    }
                    else
                    {
                        m_tableViewitem = NULL;
                        m_tableView     = NULL;
                    }

                    //===============================
                    // TopMost Layer: Mouse cursor
                    // with color blocks
                    //===============================
                    m_linex = new QGraphicsLineItem();
                    m_graphicsScene->addItem(m_linex);
                    m_linex->setLine(0, 0, CURSOR_SIZE * 2, 0);

                    m_liney = new QGraphicsLineItem();
                    m_graphicsScene->addItem(m_liney);
                    m_liney->setLine(0, 0, 0, CURSOR_SIZE * 2);

                    QPen pen(Qt::white);
                    m_linex->setPen(pen);
                    m_liney->setPen(pen);

                    m_linex->hide();
                    m_liney->hide();

                    m_rectBlocks = new QGraphicsRectItem(0, 0, m_graphicsScene->cursorBlockX, m_graphicsScene->cursorBlockY);
                    m_graphicsScene->addItem(m_rectBlocks);
                    m_rectBlocks->setPen(pen);
                    m_rectBlocks->hide();

                    //=============================
                    // Set Visable items
                    //=============================
                    m_imageGraphicsView->setVisible(true);

                    ManageScrollBars();
                }
            }
    }

    setLayout(m_layout);
}

int acImageView::getBrightnessLevel()
{
    return m_imageItem_Processed->m_iBrightness;
}

void acImageView::setBrightnessLevel(int brightness)
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    m_imageItem_Processed->m_iBrightness = brightness;

    if (m_appBusy)
        return;
    m_appBusy                                  = true;
    m_imageItem_Processed->m_UseProcessedImage = true;
    m_imageItem_Processed->m_ImageBrightness   = true;
    m_imageItem_Processed->UpdateImage();
    m_imageItem_Processed->m_UseProcessedImage = false;
    m_appBusy                                  = false;
}

void acImageView::enableNavigation(bool enable)
{
    Q_UNUSED(enable);

#ifdef ENABLE_NAVIGATION
    if (enable)
    {
        m_navigateButton->show();
    }
    else
    {
        m_navigateButton->hide();
    }
#endif
}

// ---------------------------------------------------------------------------
// Name:        acImageView::onMouseDoubleClickEvent
// Description: Captures mouse double click events from acGraphicsView.
// Arguments:   QMouseEvent * e
// Return Val:  void
// Date:        4/9/2015
// ---------------------------------------------------------------------------
void acImageView::onMouseDoubleClickEvent(QMouseEvent* e)
{
    //qDebug() << __FUNCTION__ << " MouseEvent ";
    Q_UNUSED(e);
}

void acImageView::showVirtualCursor()
{
    m_linex->show();
    m_liney->show();
    m_rectBlocks->show();
#ifdef ENABLE_NAVIGATION
    m_navigateButton->hide();
#endif
}

void acImageView::hideVirtualCursor()
{
    m_linex->hide();
    m_liney->hide();
    m_rectBlocks->hide();
#ifdef ENABLE_NAVIGATION
    m_navigateButton->show();
#endif
}

// ToDo check for m_imageItem_Original
void acImageView::showTableView(bool display)
{
    if (display)
    {
        m_imageItem_Processed->hide();
        m_tableViewitem->show();
    }
    else
    {
        m_tableViewitem->hide();
        m_imageItem_Processed->show();
    }
}

#ifdef _DEBUG
void acImageView::onToggleDebugChanged(int index)
{
    switch (index)
    {
    case 0:
        m_debugMode   = false;
        m_debugFormat = "";
        if (m_graphicsScene)
        {
            m_graphicsScene->isDebug = false;
        }
        break;
    case 1:  //BC6H
        m_debugMode   = true;
        m_debugFormat = "BC6H";
        if (m_graphicsScene)
        {
            m_graphicsScene->isDebug = true;
        }
        break;
    case 2:  //BC6H_SF
        m_debugMode   = true;
        m_debugFormat = "BC6H_SF";
        if (m_graphicsScene)
        {
            m_graphicsScene->isDebug = true;
        }
        break;
    case 3:  //BC7
        m_debugMode   = true;
        m_debugFormat = "BC7";
        if (m_graphicsScene)
        {
            m_graphicsScene->isDebug = true;
        }
        break;
    default:
        m_debugMode   = false;
        m_debugFormat = "";
        if (m_graphicsScene)
        {
            m_graphicsScene->isDebug = false;
        }
        break;
    }
}
#endif

void acImageView::onToggleImageViews(int index)
{
    // View Processed
    if (index == 0)
    {
        // Use Current position of Original image to set position of Processed image view
        MatchImagePosition(1);
        // if there is error, view is not visible
        if (m_errMessage)
        {
            m_errMessage->setVisible(true);
            m_imageItem_Processed->setVisible(false);
            if (m_imageItem_Original)
                m_imageItem_Original->setVisible(false);
        }
        else
        {
            // Make Visible the Processed Image and Hide the Original
            m_imageItem_Processed->setVisible(true);
            if (m_imageItem_Original)
                m_imageItem_Original->setVisible(false);
        }
    }
    else
    {
        // Use Current position of Processed image to set position of Original image view
        MatchImagePosition(0);

        // Hide processed view error when view original
        if (m_errMessage)
        {
            m_errMessage->setVisible(false);
        }

        // Make Visible the Original Image and Hide the Processed
        m_imageItem_Processed->setVisible(false);
        if (m_imageItem_Original)
            m_imageItem_Original->setVisible(true);
    }
}

void acImageView::MatchImagePosition(int activeIndex)
{
    // Check!
    if (!m_graphicsScene)
        return;
    if (!m_imageItem_Processed)
        return;
    if (!m_imageItem_Original)
        return;

    if (activeIndex == 0)
    {                                                // Processed Image
        QPointF pos = m_imageItem_Processed->pos();  // Current image size and position user is viewing
        m_imageItem_Original->setPos(pos);
    }
    else
    {                                               // We are viewing the Original Image
        QPointF pos = m_imageItem_Original->pos();  // Current image size and position user is viewing
        m_imageItem_Processed->setPos(pos);
    }
}

void acImageView::UpdatePixmapImage()
{
    QImage srcImage((m_imageItem_Processed->pixmap()).toImage());

    // ensure that we are always working on an RGBA8888 image, since that is what our conversion function generates
    QImage workingImage = srcImage.convertToFormat(QImage::Format_RGBA8888);

    MipSet* mipset = 0;

    if (m_MipImages->mipset->m_compressed)
        mipset = m_MipImages->decompressedMipSet;
    else
        mipset = m_MipImages->mipset;

    MipLevel* mipLevel = m_imageloader->getCMips()->GetMipLevel(mipset, m_currentMiplevel, m_depthIndex);
    if (!mipLevel)
        return;

    CMP_ChannelFormat channelFormat = mipset->m_ChannelFormat;
    FloatParams       params(&m_imageloader->m_options);

    // Leave the currently displayed pixmap untouched if the source cannot be converted.
    if (FloatToByte(workingImage.bits(),
                    static_cast<CMP_DWORD>(workingImage.byteCount()),
                    mipLevel->m_pfData,
                    mipLevel->m_dwLinearSize,
                    channelFormat,
                    static_cast<CMP_DWORD>(mipLevel->m_nWidth),
                    static_cast<CMP_DWORD>(mipLevel->m_nHeight),
                    &params) != CMP_OK)
        return;

    m_imageItem_Processed->setPixmap(QPixmap::fromImage(workingImage));
}

void acImageView::onVirtualMouseMoveEvent(QPointF* scenePos, QPointF* localPos, int onID)
{
    if (m_MouseHandDown)
        return;
    if (m_MipImages == NULL)
        return;

    if (scenePos)
    {
        //qDebug() << "*** acImageView::onVirtualMouseMoveEvent  ID   " << m_graphicsScene->ID << " onID " << onID << " sx : " << scenePos->rx() << " sy: " << scenePos->ry();
        // Tracking cursor - maps mouse pos from multiple acImageView items
        // Is the real mouse on the actual image or off the image and out of bounds
        if ((onID == m_graphicsScene->ID) || (localPos == NULL))
        {
            hideVirtualCursor();
        }
        else
        {
            QPoint imageloc   = localPos->toPoint();
            QRectF boundImage = m_imageItem_Processed->boundingRect();
            QRectF boundScene = m_imageItem_Processed->sceneBoundingRect();

            qreal scaleX = 0;
            if (boundImage.width() > 0)
                scaleX = boundScene.width() / boundImage.width();

            qreal scaleY = 0;
            if (boundImage.height() > 0)
                scaleY = boundScene.height() / boundImage.height();

            QPointF imagePt = boundScene.topLeft();
            QPoint  point   = imagePt.toPoint();

            int X = point.x() + (imageloc.x() * scaleX);
            int Y = point.y() + (imageloc.y() * scaleY);

            // Draw the Virtual Cursor on screen co-ordinates;
            showVirtualCursor();
            m_linex->setPos(X - CURSOR_SIZE, Y);
            m_liney->setPos(X, Y - CURSOR_SIZE);

            if (m_tableViewitem)
            {
                m_tableView->rowAt(imageloc.y());
                m_tableView->columnAt(imageloc.x());
            }

            emit acImageViewVirtualMousePosition(scenePos, localPos, onID);
        }

#ifdef _DEBUG
        if (m_debugMode)
        {
            QPoint imageloc   = localPos->toPoint();
            QRectF boundImage = m_imageItem_Processed->boundingRect();
            QRectF boundScene = m_imageItem_Processed->sceneBoundingRect();

            qreal scaleX = 0;
            if (boundImage.width() > 0)
                scaleX = boundScene.width() / boundImage.width();

            qreal scaleY = 0;
            if (boundImage.height() > 0)
                scaleY = boundScene.height() / boundImage.height();

            QPointF imagePt = boundScene.topLeft();
            QPoint  point   = imagePt.toPoint();

            int X = point.x() + (imageloc.x() * scaleX);
            int Y = point.y() + (imageloc.y() * scaleY);

            qreal cursorXsize = 0;
            qreal cursorYsize = 0;

            cursorXsize = m_graphicsScene->cursorBlockX * scaleX;
            cursorYsize = m_graphicsScene->cursorBlockY * scaleY;

            // Draw the Virtual Cursor on screen co-ordinates;
            showVirtualCursor();
            m_linex->setPos(X - CURSOR_SIZE, Y);
            m_liney->setPos(X, Y - CURSOR_SIZE);

            m_rectBlocks->setRect(X - (cursorXsize / 2), Y - (cursorYsize / 2), cursorXsize, cursorYsize);
        }
#endif
    }
}

bool acImageView::IsImageBoundedToView(QPointF* mousePos)
{
    UNREFERENCED_PARAMETER(mousePos);

    QPointF DeltaPos{0, 0};

    // if (mousePos)
    // {
    //     if ((m_lastMousePos.x() > 0) && (m_lastMousePos.y() > 0))
    //     DeltaPos = *mousePos - m_lastMousePos;
    //     m_lastMousePos = *mousePos;
    // }

    // Check Image is in bound of View
    QRectF bounds      = m_graphicsScene->sceneRect();                // Size of our Scene view
    QRectF imageBounds = m_imageItem_Processed->sceneBoundingRect();  // Current image size and position user is viewing

    bool bounded = false;

    // if ((imageBounds.x() + DeltaPos.x()) > 1)
    // {
    //     bounded = true;
    // }
    //
    // if ((imageBounds.y() + DeltaPos.y()) > 1)
    // {
    //     bounded = true;
    // }

    return bounded;
}

// ---------------------------------------------------------------------------
// Name:        acImageView::onMouseDoubleClickEvent
// Description: Captures mouse position from acGraphicsScene and passed it out as a signal
// Arguments:   QPointF *scenePos and unique ID of the GraphicsView instance
// Return Val:  void
// Date:        4/9/2015
// ---------------------------------------------------------------------------
void acImageView::onacImageViewMousePosition(QPointF* scenePos, int ID)
{
    if (m_MouseHandDown)
        return;

    //qDebug() << " =============== START OF MOUSE MOVE EVENT =================" <<  " rx : " << scenePos->rx() << " ry: " << scenePos->ry();

    QGraphicsItem* itemPicked;
    QPointF        localPt;

    itemPicked = m_graphicsScene->itemAt(scenePos->rx(), scenePos->ry(), QTransform());

    // is mouse inside image view
    if (itemPicked)
    {
        localPt = itemPicked->mapFromScene((const QPointF&)*scenePos);
        //qDebug() << "item at rx : " << localPt.rx() << " ry: " << localPt.ry();

        if (m_graphicsScene->isDebug)
        {
            // napatel
            int x = qRound(localPt.rx() - 0.5f);
            if (x < 0)
                x = 0;

            int y = qRound(localPt.ry() - 0.5f);
            if (y < 0)
                y = 0;

            x = (x / m_graphicsScene->cursorBlockX);
            y = (y / m_graphicsScene->cursorBlockY);

            localPt.setX(qreal(x));
            localPt.setY(qreal(y));
        }
        emit acImageViewMousePosition(scenePos, &localPt, ID);
    }
    else
    {
        // mouse is outside of an image view
        emit acImageViewMousePosition(scenePos, NULL, ID);
    }

    if (m_MouseHandDown)
    {
        if (m_imageItem_Processed)
        {
            if (!IsImageBoundedToView(scenePos))
            {
                m_imageItem_Processed->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
                if (m_imageItem_Original)
                    m_imageItem_Original->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
            }
            else
            {
                m_imageItem_Processed->setFlags(QGraphicsItem::ItemIsSelectable);
                if (m_imageItem_Original)
                    m_imageItem_Original->setFlags(QGraphicsItem::ItemIsSelectable);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Name:        acImageView::resizeEvent
// Description: Adjust component pos & sizes inside the QGraphicsView to match
//              current wiget resize
// Arguments:   QResizeEvent *e
// Return Val:  void
// Date:        4/9/2015
// ---------------------------------------------------------------------------
void acImageView::resizeEvent(QResizeEvent* e)
{
    Q_UNUSED(e);
    //qDebug() << __FUNCTION__;

    QSize size = this->size();
    //qDebug() << "Size H:" << size.height() << " W:" << size.width();

    // Update the scene to fit window
    if (m_graphicsScene)
    {
        QRectF bounds = m_graphicsScene->itemsBoundingRect();
        m_graphicsScene->setSceneRect(0, 0, size.width(), size.height());
    }

    // get the image bounding size and use it to center our Image to widget windows
    if (m_imageItem_Processed)
    {
        centerImage();
    }

    //Reserved: GPUDecode

#ifdef ENABLE_NAVIGATION
    // Move the navigate button
    if (m_navigateButton)
    {
        // Note we are clipping the button to the bottom left corner of the view
        // and not showing all of it: only the top left corner of the button
        // it has a nice visual effect of a small corner box. We can change this to
        // be a icon view and fit it properly according to the icon size. instead of the
        // 10 pixels we use here
        m_navigateButton->move(size.width() - 10, size.height() - 10);

        // If the Navigation image is been viewed then also
        // move it tp fix the corner
        if (m_imageItemNav)
        {
            QSizeF sizeF = m_imageItemNav->boundingRect().size();
            m_imageItemNav->setPos(size.width() - sizeF.width(), size.height() - sizeF.width());
        }
    }
#endif

    if (m_tableViewitem)
    {
        //m_tableViewitem->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
        if (m_graphicsScene)
        {
            // Scaling an item to fit view
            QRectF bound = m_graphicsScene->itemsBoundingRect();
            //    // QRect itemSize = cpView.m_imageGraphicsView->mapFromScene(itemPicked->sceneBoundingRect()).boundingRect();
            //    // double ratiox = 1;
            //    // double ratioy = 1;
            //    // if (itemSize.width() > 0)
            //    //     ratiox = bound.width() / itemSize.width();
            //    // if (itemSize.height() > 0)
            //    //     ratioy = bound.height() / itemSize.height();
            //    // itemPicked->setTransform(QTransform::fromScale(ratiox, ratioy), true);
            QRect rec = bound.toRect();
            m_tableView->setMinimumWidth(rec.width());
            m_tableView->setMinimumHeight(rec.height());
        }
    }
}

// ---------------------------------------------------------------------------
// Name:        acImageView::onNavigateClicked
// Description: called when user selected navigation button, it will either
//              show the navigation view or hide it if its already visible
// Arguments:
// Return Val:  void
// Date:        4/9/2015
// ---------------------------------------------------------------------------
void acImageView::onNavigateClicked()
{
#ifdef ENABLE_NAVIGATION
    if (m_navVisible)
    {
        if (m_imageItemNav)
        {
            m_imageItemNav->setVisible(false);
            m_navVisible = false;
        }
    }
    else
    {
        if (m_imageItemNav)
        {
            m_imageItemNav->setVisible(true);
            m_navVisible = true;
        }
    }
#endif
}

// ---------------------------------------------------------------------------
// Name:        void acImageView::onResetImageView
// Description: called when user pressed the shift key with a left mouse click
//              it will reset all views back to default state
// Arguments:
// Return Val:  void
// Date:        4/9/2015
// ---------------------------------------------------------------------------
void acImageView::onResetImageView()
{
    if (m_MipImages)
        if (m_MipImages->m_Error == MIPIMAGE_FORMAT_ERRORS::Format_InvalidFile)
            return;

    QSize size = this->size();

    // Update the scene to fit window
    if (m_graphicsScene)
    {
        QRectF bounds = m_graphicsScene->itemsBoundingRect();
        m_graphicsScene->setSceneRect(0, 0, size.width(), size.height());
    }

    // get the image use it to fit it to widget windows
    if (m_imageItem_Processed)
    {
        // Transformation point
        m_imageItem_Processed->setTransformOriginPoint(0, 0);
        if (m_imageItem_Original)
            m_imageItem_Original->setTransformOriginPoint(0, 0);

        // Set image properties back to defaults
        m_imageItem_Processed->setDefaults();
        m_imageItem_Processed->m_UseProcessedImage = true;
        m_appBusy                                  = true;
        m_imageItem_Processed->UpdateImage();
        m_appBusy = false;

        if (m_imageItem_Original)
        {
            m_imageItem_Original->setDefaults();
            m_imageItem_Original->m_UseProcessedImage = true;
            m_appBusy                                 = true;
            m_imageItem_Original->UpdateImage();
            m_appBusy = false;
        }

        onViewImageOriginalSize();

        m_imageOrientation = 0;
        m_imageItem_Processed->setRotation(0);

        if (m_imageItem_Original)
            m_imageItem_Original->setRotation(0);

        onFitInWindow();
    }

    // ToDo Reset all action checked ToolButtons back to default state
}

void acImageView::onExrExposureChanged(double value)
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;
    if (!m_imageloader)
        return;

    m_imageloader->m_options.fInputExposure = float(value);

    UpdatePixmapImage();
}

void acImageView::onExrDefogChanged(double value)
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;
    if (!m_imageloader)
        return;

    m_imageloader->m_options.fInputDefog = float(value);

    UpdatePixmapImage();
}

void acImageView::onExrKneeLowChanged(double value)
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    if (!m_imageloader)
        return;

    m_imageloader->m_options.fInputKneeLow = float(value);

    UpdatePixmapImage();
}

void acImageView::onExrKneeHighChanged(double value)
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;
    if (!m_imageloader)
        return;

    m_imageloader->m_options.fInputKneeHigh = float(value);

    UpdatePixmapImage();
}

void acImageView::onExrGammaChanged(double value)
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    if (!m_imageloader)
        return;

    m_imageloader->m_options.fInputGamma = float(value);

    UpdatePixmapImage();
}

void acImageView::onToggleChannelR()
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    if (m_appBusy)
        return;
    m_appBusy = true;

    m_imageItem_Processed->m_UseProcessedImage = true;
    m_imageItem_Processed->m_ChannelR          = !m_imageItem_Processed->m_ChannelR;
    m_imageItem_Processed->UpdateImage();
    m_imageItem_Processed->m_UseProcessedImage = false;

    m_appBusy = false;
}

void acImageView::onToggleChannelG()
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    if (m_appBusy)
        return;
    m_appBusy = true;

    m_imageItem_Processed->m_UseProcessedImage = true;
    m_imageItem_Processed->m_ChannelG          = !m_imageItem_Processed->m_ChannelG;
    m_imageItem_Processed->UpdateImage();
    m_imageItem_Processed->m_UseProcessedImage = false;

    m_appBusy = false;
}

void acImageView::onToggleChannelB()
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    if (m_appBusy)
        return;
    m_appBusy = true;

    m_imageItem_Processed->m_UseProcessedImage = true;
    m_imageItem_Processed->m_ChannelB          = !m_imageItem_Processed->m_ChannelB;
    m_imageItem_Processed->UpdateImage();
    m_imageItem_Processed->m_UseProcessedImage = false;

    m_appBusy = false;
}

void acImageView::onToggleChannelA()
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    if (m_appBusy)
        return;
    m_appBusy = true;

    m_imageItem_Processed->m_UseProcessedImage = true;
    m_imageItem_Processed->m_ChannelA          = !m_imageItem_Processed->m_ChannelA;
    m_imageItem_Processed->UpdateImage();
    m_imageItem_Processed->m_UseProcessedImage = false;

    m_appBusy = false;
}

void acImageView::onToggleGrayScale()
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;
    if (m_appBusy)
        return;
    m_appBusy = true;

    m_imageItem_Processed->m_UseProcessedImage = true;
    m_imageItem_Processed->m_GrayScale         = !m_imageItem_Processed->m_GrayScale;
    m_imageItem_Processed->UpdateImage();
    m_imageItem_Processed->m_UseProcessedImage = false;
    m_appBusy                                  = false;
}

void acImageView::onInvertImage()
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    if (m_appBusy)
        return;
    m_appBusy = true;

    m_imageItem_Processed->m_InvertImage = true;
    m_imageItem_Processed->UpdateImage();
    m_imageItem_Processed->m_InvertImage = false;

    m_appBusy = false;
}

void acImageView::onMirrorHorizontal()
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    if (m_appBusy)
        return;
    m_appBusy = true;

    // Transformation point
    m_imageItem_Processed->setTransformOriginPoint(0, 0);

    m_imageItem_Processed->m_Mirrored        = true;
    m_imageItem_Processed->m_Mirrored_h      = true;
    m_imageItem_Processed->m_Mirrored_v      = false;
    m_imageItem_Processed->m_ImageBrightness = false;
    m_imageItem_Processed->UpdateImage();
    m_imageItem_Processed->m_Mirrored = false;

    m_appBusy = false;

    // Transformation point
    if (m_imageItem_Original)
    {
        m_imageItem_Original->setTransformOriginPoint(0, 0);
        m_imageItem_Original->m_Mirrored        = true;
        m_imageItem_Original->m_Mirrored_h      = true;
        m_imageItem_Original->m_Mirrored_v      = false;
        m_imageItem_Original->m_ImageBrightness = false;
        m_appBusy                               = true;
        m_imageItem_Original->UpdateImage();
        m_appBusy                        = false;
        m_imageItem_Original->m_Mirrored = false;
    }
}

void acImageView::onMirrorVirtical()
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    if (m_appBusy)
        return;
    m_appBusy = true;

    // Transformation point
    m_imageItem_Processed->setTransformOriginPoint(0, 0);
    m_imageItem_Processed->m_Mirrored        = true;
    m_imageItem_Processed->m_Mirrored_h      = false;
    m_imageItem_Processed->m_Mirrored_v      = true;
    m_imageItem_Processed->m_ImageBrightness = false;
    m_imageItem_Processed->UpdateImage();
    m_imageItem_Processed->m_Mirrored = false;
    m_appBusy                         = false;

    if (m_imageItem_Original)
    {
        m_imageItem_Original->setTransformOriginPoint(0, 0);
        m_imageItem_Original->m_Mirrored        = true;
        m_imageItem_Original->m_Mirrored_h      = false;
        m_imageItem_Original->m_Mirrored_v      = true;
        m_imageItem_Original->m_ImageBrightness = false;
        m_appBusy                               = true;
        m_imageItem_Original->UpdateImage();
        m_appBusy                        = false;
        m_imageItem_Original->m_Mirrored = false;
    }
}

void acImageView::onRotateRight()
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    QRectF bounds = m_imageItem_Processed->boundingRect();  // Images Size bounded within a rectangle
    qreal  x      = bounds.width() / 2;
    qreal  y      = bounds.height() / 2;
    m_imageOrientation += 90;
    if (m_imageOrientation >= 360)
        m_imageOrientation = 0;

    m_imageItem_Processed->setTransformOriginPoint(x, y);
    m_imageItem_Processed->setRotation(m_imageOrientation);

    if (m_imageItem_Original)
    {
        m_imageItem_Original->setTransformOriginPoint(x, y);
        m_imageItem_Original->setRotation(m_imageOrientation);
    }

    centerImage();
}

void acImageView::onRotateLeft()
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    m_imageOrientation -= 90;
    if (m_imageOrientation < 0)
        m_imageOrientation = 270;

    QRectF bounds = m_imageItem_Processed->boundingRect();  // Images Size bounded within a rectangle
    qreal  x      = bounds.width() / 2;
    qreal  y      = bounds.height() / 2;
    m_imageItem_Processed->setTransformOriginPoint(x, y);
    m_imageItem_Processed->setRotation(m_imageOrientation);

    if (m_imageItem_Original)
    {
        m_imageItem_Original->setTransformOriginPoint(x, y);
        m_imageItem_Original->setRotation(m_imageOrientation);
    }

    centerImage();
}

void acImageView::onViewImageOriginalSize()
{
    // Check!
    if (!m_graphicsScene)
        return;
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    QSize size = this->size();

    m_imageItem_Processed->setTransformOriginPoint(0, 0);
    m_imageItem_Processed->m_UseProcessedImage = true;
    m_imageItem_Processed->setScale(1.0);

    if (m_imageItem_Original)
    {
        m_imageItem_Original->setTransformOriginPoint(0, 0);
        m_imageItem_Original->m_UseProcessedImage = true;
        m_imageItem_Original->setScale(1.0);
    }

    m_ImageScale = 100;
    emit acScaleChanged(m_ImageScale);

#ifdef ENABLE_NAVIGATION
    QSizeF sizeF = m_imageItemNav->boundingRect().size();
    m_imageItemNav->setPos(size.width() - sizeF.width(), size.height() - sizeF.width());
#endif

    centerImage();

    m_imageItem_Processed->m_UseProcessedImage = false;
    if (m_imageItem_Original)
        m_imageItem_Original->m_UseProcessedImage = false;
}

void acImageView::onSetPixelDiffView(bool OnOff)
{
    // Check!
    if (!m_graphicsScene)
        return;
    if (!m_imageItem_Processed)
        return;

    if (m_appBusy)
        return;

    m_appBusy                                  = true;
    m_imageItem_Processed->m_ShowPixelDiff     = OnOff;
    m_imageItem_Processed->m_UseProcessedImage = true;
    m_imageItem_Processed->UpdateImage();
    m_imageItem_Processed->m_UseProcessedImage = false;
    m_appBusy                                  = false;
}

void acImageView::centerImage()
{
    //#ifdef USE_MOVABLE_IMAGES
    // Check!
    if (!m_graphicsScene)
        return;
    if (!m_imageItem_Processed)
        return;

    QRectF boundsView = m_graphicsScene->sceneRect();      // Size of the view user sees the image in
    m_imageItem_Processed->setTransformOriginPoint(0, 0);  // Transformation point

    QRectF bounds = m_imageItem_Processed->boundingRect();       // Original images size
    QRectF offset = m_imageItem_Processed->sceneBoundingRect();  // Current image size and position user is viewing

    // if original item exist use original item for bound and offset
    if (m_imageItem_Original)
    {
        bounds = m_imageItem_Original->boundingRect();
        offset = m_imageItem_Original->sceneBoundingRect();
    }
    // Move the images top left  origin corner to screen center
    qreal dx = boundsView.center().x() - offset.x();
    qreal dy = boundsView.center().y() - offset.y();

    // now move the image so it is centered in the view
    dx = dx - (offset.width() / 2);
    dy = dy - (offset.height() / 2);

    // translate move the image from it current position by a delta x and delta y
    // so that is viewed in center
    QTransform translation;
    translation.translate(dx, dy);
    m_imageItem_Processed->setTransform(translation, true);
    //#endif

    if (m_imageItem_Original)
    {
        m_imageItem_Original->setTransformOriginPoint(0, 0);
        m_imageItem_Original->setTransform(translation, true);
    }
}

void acImageView::onFitInWindow()
{
    // Check!
    if (!m_graphicsScene)
        return;
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    // Scale the image to fit scene view
    QRectF bounds = m_graphicsScene->sceneRect();                  // Size of our Scene view
    QSizeF sizeF  = m_imageItem_Processed->boundingRect().size();  // Actual size of the image

    // if original exist, use original actual size
    if (m_imageItem_Original)
    {
        sizeF = m_imageItem_Original->boundingRect().size();  // Actual size of the image
    }

    qreal sceneW = bounds.width();
    qreal sceneH = bounds.height();
    qreal itemW  = sizeF.width();
    qreal itemH  = sizeF.height();
    qreal scale  = 1.0;

    qreal scaleW = 1.0;
    if (itemW > 0)
    {
        scaleW = sceneW / itemW;
    }

    qreal scaleH = 1.0;
    if (itemH > 0)
    {
        scaleH = sceneH / itemH;
    }

    if (scaleH > scaleW)
        scale = scaleW;
    else
        scale = scaleH;

    m_ImageScale = (int)(scale * 100);

    // Transformation point
    m_imageItem_Processed->setTransformOriginPoint(0, 0);
    m_imageItem_Processed->setScale(scale);

    if (m_imageItem_Original)
    {
        m_imageItem_Original->setTransformOriginPoint(0, 0);
        m_imageItem_Original->setScale(scale);
    }

    emit acScaleChanged(m_ImageScale);

    centerImage();
}

void acImageView::onGridBackground(int enableGrid)
{
    eCustomGraphicsScene_Grids enable = (eCustomGraphicsScene_Grids)enableGrid;

    if (m_graphicsScene->isGridEnabled())
        m_graphicsScene->gridEnabled(enable);
    else
        m_graphicsScene->gridEnabled(enable);
    m_graphicsScene->update();
}

// This can be called multiple time when used with more then 1 image view, like that used in Image DIff views
// the code should be optimized so that the PSNR calc can be turned on of off during construct or viewing
void acImageView::processPSNR()
{
    // Is the calculation for the first time then process the data
    if (m_PSNR[m_currentMiplevel][m_depthIndex] == 0.0)
    {
        if (m_OriginalMipImages == NULL)
            return;
        if (m_MipImages == NULL)
            return;
        if (m_MipImages->decompressedMipSet == NULL)
            return;
        if (m_OriginalMipImages->mipset == NULL)
            return;
        if (m_OriginalMipImages->mipset->m_compressed && m_OriginalMipImages->decompressedMipSet == NULL)
            return;

        //CMP_DOUBLE       outMSE;
        //CMP_DOUBLE       outPSNR;
        CMP_AnalysisData analysisData = {0};
        analysisData.channelBitMap    = CMP_getFormat_nChannels(m_MipImages->mipset->m_format);  // m_MipImages->decompressedMipSet->m_format);
        CMP_ERROR status;

        if (m_OriginalMipImages->mipset->m_compressed)
            status =
                CMP_MipSetAnlaysis(m_OriginalMipImages->decompressedMipSet, m_MipImages->decompressedMipSet, m_currentMiplevel, m_depthIndex, &analysisData);
        else
            status = CMP_MipSetAnlaysis(m_OriginalMipImages->mipset, m_MipImages->decompressedMipSet, m_currentMiplevel, m_depthIndex, &analysisData);

        if (status == CMP_OK)
        {
            m_MSE[m_currentMiplevel][m_depthIndex]  = analysisData.mse;
            m_PSNR[m_currentMiplevel][m_depthIndex] = analysisData.psnr;
        }
        else
            return;  // should post an error message to user!
    }

    // signal the data to all slots
    acPSNRUpdated(m_PSNR[m_currentMiplevel][m_depthIndex]);
}

void acImageView::onImageMipLevelChanged(int MipLevel)
{
    m_currentMiplevel = MipLevel;
    if (m_MipImages)
    {
        if (m_MipImages->QImage_list[m_depthIndex].size() > MipLevel)
        {
            m_ImageIndex  = MipLevel;
            QImage* image = m_MipImages->QImage_list[m_depthIndex][m_ImageIndex];
            if (image)
            {
                m_imageItem_Processed->changeImage(*image);
            }

            if (m_OriginalMipImages)
            {
                if (m_OriginalMipImages->QImage_list[m_depthIndex].size() > MipLevel)
                {
                    QImage* image_original = m_OriginalMipImages->QImage_list[m_depthIndex][m_ImageIndex];
                    if (image_original)
                    {
                        m_imageItem_Original->changeImage(*image_original);
                        m_imageItem_Processed->changeImageDiffRef(image_original);
                    }
                }
            }

            centerImage();
            onFitInWindow();

            processPSNR();
        }
    }
}

void acImageView::onImageDepthChanged(int DepthLevel)
{
    if (DepthLevel >= CMP_MIPSET_MAX_DEPTHS)
        DepthLevel = CMP_MIPSET_MAX_DEPTHS - 1;

    if (m_MipImages)
    {
        m_depthIndex  = DepthLevel;
        QImage* image = m_MipImages->QImage_list[DepthLevel][m_ImageIndex];
        if (image)
        {
            m_imageItem_Processed->changeImage(*image);
        }

        if (m_OriginalMipImages)
        {
            QImage* image_original = m_OriginalMipImages->QImage_list[DepthLevel][m_ImageIndex];
            if (image_original)
            {
                m_imageItem_Original->changeImage(*image_original);
                m_imageItem_Processed->changeImageDiffRef(image_original);
            }
        }

        centerImage();
        onFitInWindow();
    }
}

void acImageView::onMouseHandDown()
{
    m_MouseHandDown = true;
    m_lastMousePos.setX(0);
    m_lastMousePos.setY(0);
    if (m_imageItem_Processed)
    {
        if (!IsImageBoundedToView(NULL))
            m_imageItem_Processed->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
        else
            m_imageItem_Processed->setFlags(QGraphicsItem::ItemIsSelectable);
    }

    if (m_imageItem_Original)
    {
        if (!IsImageBoundedToView(NULL))
            m_imageItem_Original->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
        else
            m_imageItem_Original->setFlags(QGraphicsItem::ItemIsSelectable);
    }

    ManageScrollBars();
}

void acImageView::onMouseHandD()
{
    m_MouseHandDown = false;

    if (m_imageItem_Processed)
        m_imageItem_Processed->setFlags(QGraphicsItem::ItemIsSelectable);
    if (m_imageItem_Original)
        m_imageItem_Original->setFlags(QGraphicsItem::ItemIsSelectable);

    ManageScrollBars();
}

void acImageView::onWheelScaleUp(QPointF& pos)
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    Qt::KeyboardModifiers keyMod = QApplication::keyboardModifiers();
    m_ImageScale += keyMod.testFlag(Qt::ControlModifier) ? 100 : 10;

    if (m_ImageScale > AC_IMAGE_MAX_ZOOM)
        m_ImageScale = AC_IMAGE_MAX_ZOOM;

    QPointF localPt;

    localPt     = m_imageItem_Processed->mapFromScene(pos);
    qreal scale = m_ImageScale / 100.0;
    m_imageItem_Processed->setScale(scale);
    if (m_imageItem_Original)
        m_imageItem_Original->setScale(scale);

    QPointF localPtNew;
    localPtNew = m_imageItem_Processed->mapFromScene(pos);

    QPointF Delta = localPtNew - localPt;
    m_imageItem_Processed->moveBy(Delta.x() * scale, Delta.y() * scale);
    if (m_imageItem_Original)
        m_imageItem_Original->moveBy(Delta.x() * scale, Delta.y() * scale);
    emit acScaleChanged(m_ImageScale);

    ManageScrollBars();
}

// Zoom Out
void acImageView::onWheelScaleDown(QPointF& pos)
{
    // Check!
    if (!m_imageItem_Processed)
        return;
    if (m_MipImages)
        if (m_MipImages->m_Error != MIPIMAGE_FORMAT_ERRORS::Format_NoErrors)
            return;

    Qt::KeyboardModifiers keyMod = QApplication::keyboardModifiers();
    m_ImageScale += keyMod.testFlag(Qt::ControlModifier) ? -100 : -10;

    if (m_ImageScale < AC_IMAGE_MIN_ZOOM)
        m_ImageScale = AC_IMAGE_MIN_ZOOM;

    QPointF localPt;

    localPt     = m_imageItem_Processed->mapFromScene(pos);
    qreal scale = m_ImageScale / 100.0;
    m_imageItem_Processed->setScale(scale);

    if (m_imageItem_Original)
        m_imageItem_Original->setScale(scale);

    QPointF localPtNew;
    localPtNew = m_imageItem_Processed->mapFromScene(pos);

    QPointF Delta = localPtNew - localPt;
    m_imageItem_Processed->moveBy(Delta.x() * scale, Delta.y() * scale);
    if (m_imageItem_Original)
        m_imageItem_Original->moveBy(Delta.x() * scale, Delta.y() * scale);
    emit acScaleChanged(m_ImageScale);

    ManageScrollBars();
}

void acImageView::onSetScale(int value)
{
    if (!m_graphicsScene)
        return;
    if (!m_imageItem_Processed)
        return;

    // Size of our Scene view
    QRectF  bounds = m_graphicsScene->sceneRect();
    QPointF pos    = bounds.center();

    QGraphicsItem* itemPicked = m_graphicsScene->itemAt(pos, QTransform());
    // Found an item under the cursor
    if (itemPicked)
    {
        // Is the item our custome image
        if (itemPicked->type() == (itemPicked->UserType + acCustomGraphicsTypes::IMAGE))
        {
            //========================================================
            // Zoom from a image position that is on the scene Center
            //========================================================

            QPointF localPt;
            localPt = m_imageItem_Processed->mapFromScene(pos);

            m_ImageScale = value;
            qreal scale  = m_ImageScale / 100.0;
            m_imageItem_Processed->setScale(scale);

            if (m_imageItem_Original)
                m_imageItem_Original->setScale(scale);

            QPointF localPtNew;
            localPtNew = m_imageItem_Processed->mapFromScene(pos);

            QPointF Delta = localPtNew - localPt;
            m_imageItem_Processed->moveBy(Delta.x() * scale, Delta.y() * scale);
            if (m_imageItem_Original)
                m_imageItem_Original->moveBy(Delta.x() * scale, Delta.y() * scale);
        }
    }
    else
    {
        //========================================================
        // Zoom from image Center
        //========================================================

        QRectF offset1 = m_imageItem_Processed->sceneBoundingRect();  // Current image size and position user is viewing

        m_ImageScale = value;
        m_imageItem_Processed->setScale(m_ImageScale / 100.0);
        if (m_imageItem_Original)
            m_imageItem_Original->setScale(m_ImageScale / 100.0);

        QRectF  offset2 = m_imageItem_Processed->sceneBoundingRect();  // New image size and position user is viewing
        QPointF Delta   = offset1.center() - offset2.center();
        m_imageItem_Processed->moveBy(Delta.x(), Delta.y());
        if (m_imageItem_Original)
            m_imageItem_Original->moveBy(Delta.x(), Delta.y());
    }

    ManageScrollBars();
}

void acImageView::ManageScrollBars()
{
    if (!m_imageGraphicsView)
        return;
    if (!m_graphicsScene)
        return;
    if (!m_imageItem_Processed)
        return;

    if (!m_xPos)
        return;
    if (!m_yPos)
        return;

    // Size of our viewing area
    QRectF Viewbounds = m_imageGraphicsView->contentsRect();

    // Size of our Scene view
    QRectF Scenebounds = m_graphicsScene->sceneRect();

    // Get the Current location of the Image
    QRectF imageoffset = m_imageItem_Processed->sceneBoundingRect();

    int value;

    value = m_xPos->value();
    value = m_xPos->pageStep();
    value = m_xPos->singleStep();
    value = m_xPos->minimumHeight();
    value = m_xPos->maximumHeight();
    value = m_xPos->minimumWidth();
    value = m_xPos->maximumWidth();
    value = m_xPos->minimum();
    value = m_xPos->maximum();

    bool ShowScrollX = false;
    bool ShowScrollY = false;
    // Horizontal scroll setting
    if (imageoffset.x() < 0)
    {
        int diff = abs(imageoffset.x());
        m_xPos->setMinimum(-diff);
        ShowScrollX = true;
    }

    if ((imageoffset.x() + imageoffset.width()) > Viewbounds.width())
    {
        int diff = abs((imageoffset.x() + imageoffset.width()) - Viewbounds.width());
        m_xPos->setMaximum(diff);
        ShowScrollX = true;
    }

    // Verical scroll setting
    if (imageoffset.y() < 0)
    {
        int diff = abs(imageoffset.y() + 200);
        m_yPos->setMinimum(-diff);
        ShowScrollY = true;
    }

    if ((imageoffset.y() + imageoffset.height()) > Viewbounds.height())
    {
        int diff = abs((imageoffset.y() + imageoffset.height()) - Viewbounds.height());
        m_yPos->setMaximum(diff + 200);
        ShowScrollY = true;
    }

    if (ShowScrollX)
        m_xPos->show();
    else
        m_xPos->hide();

    if (ShowScrollY)
        m_yPos->show();
    else
        m_yPos->hide();
}
