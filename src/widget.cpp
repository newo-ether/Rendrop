// widget.cpp

#include <QString>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <vector>

#include "widget.h"
#include "ui_widget.h"

#include "file_bar.h"
#include "add_file_button.h"

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

#include "drop_file_tip.h"

#if DEBUG
#include <QDebug>
#endif

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::widget)
{
    ui->setupUi(this);

    dropShadowRenderer = new DropShadowRenderer();

    addFileButton = new AddFileButton(ui->ContentWidget, dropShadowRenderer);
    QObject::connect(addFileButton, &AddFileButton::clicked, this, &Widget::onAddFileButtonClicked);
    ui->scrollArea->installEventFilter(this);

    ui->scrollAreaSizeWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->scrollAreaSizeWidget->installEventFilter(this);

    createDropShadowWidget(ui->ContentWidget, ui->infoWidget, 12, 4, 4, 0.3f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->scrollArea, 12, 4, 4, 0.3f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->renderButton, 6, 4, 4, 0.3f, 10);

    dropFileTip = new DropFileTip(ui->ContentWidget);
    dropFileTip->setAttribute(Qt::WA_TransparentForMouseEvents);

    QGraphicsOpacityEffect *opacity = new QGraphicsOpacityEffect;
    opacity->setOpacity(0.5);
    dropFileTip->setGraphicsEffect(opacity);
}

Widget::~Widget()
{
    for (auto dropShadowWidget : dropShadowWidgets)
    {
        delete dropShadowWidget;
    }

    delete addFileButton;
    delete dropFileTip;

    for (auto fileBar: fileBars)
    {
        delete fileBar;
    }

    delete ui;
    delete dropShadowRenderer;
}

bool Widget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->scrollArea && event->type() == QEvent::Resize)
    {
        QWidget *scrollArea = ui->scrollArea;
        int x = scrollArea->x() + scrollArea->width();
        int y = scrollArea->y() + scrollArea->height();

        addFileButton->move(x - 100, y - 100);
        addFileButton->raise();

        dropFileTip->setGeometry(scrollArea->geometry());
        dropFileTip->stackUnder(addFileButton);
    }

    if (watched == ui->scrollAreaSizeWidget && event->type() == QEvent::Resize)
    {
        float borderRadius = 16;
        float offsetX = 2;
        float offsetY = 2;
        float alphaMax = 0.3f;
        float blurRadius = 10;

        int fileBarWidth = ui->scrollAreaSizeWidget->width();
        int fileBarHeight = 50;

        float marginX = std::abs(offsetX) + blurRadius * 0.5f;
        float marginY = std::abs(offsetY) + blurRadius * 0.5f;

        fileBarShadowPixmap = dropShadowRenderer->render(
            fileBarWidth + marginX * 2,
            fileBarHeight + marginY * 2,
            borderRadius,
            offsetX,
            offsetY,
            alphaMax,
            blurRadius
        );
    }

    return QWidget::eventFilter(watched, event);
}

void Widget::newFileBar(QString fileName)
{
    unsigned int id = static_cast<unsigned int>(fileBars.size());
    FileBar *fileBar = new FileBar(ui->scrollAreaWidgetContents, dropShadowRenderer, id);
    fileBar->setFileName(fileName);

    DropShadowWidget *fileBarShadow = fileBar->getDropShadowWidget();
    fileBarShadow->setShadowUpdateEnabled(false);
    fileBarShadow->setShadowPixmap(&fileBarShadowPixmap);

    QVBoxLayout *layout = ui->scrollAreaVerticalLayout;
    layout->insertWidget(id, fileBar);

    QObject::connect(fileBar, &FileBar::upButtonClicked, this, &Widget::onFileBarUp);
    QObject::connect(fileBar, &FileBar::downButtonClicked, this, &Widget::onFileBarDown);
    QObject::connect(fileBar, &FileBar::deleteButtonClicked, this, &Widget::onFileBarDelete);

    fileBars.push_back(fileBar);

    if (fileBars.empty())
    {
        dropFileTip->show();
    }
    else
    {
        dropFileTip->hide();
    }
}

void Widget::createDropShadowWidget(
    QWidget *parent,
    QWidget *target,
    float borderRadius,
    float offsetX,
    float offsetY,
    float alphaMax,
    float blurRadius
) {
    DropShadowWidget *dropShadowWidget = new DropShadowWidget(parent, target, dropShadowRenderer);
    dropShadowWidget->setBorderRadius(borderRadius);
    dropShadowWidget->setOffsetX(offsetX);
    dropShadowWidget->setOffsetY(offsetY);
    dropShadowWidget->setAlphaMax(alphaMax);
    dropShadowWidget->setBlurRadius(blurRadius);

    dropShadowWidgets.push_back(dropShadowWidget);
}

#if DEBUG

void Widget::printItems()
{
    for (auto item : fileBars)
    {
        qDebug()
            << item->getID()
            << ": "
            << item->getFileName();
    }
    qDebug() << "\n";
}

#endif

void Widget::onFileBarUp(FileBar *fileBar)
{
    unsigned int id = fileBar->getID();
    if (id > 0)
    {
        FileBar *lastFileBar = fileBars[id - 1];
        lastFileBar->setID(id);
        fileBar->setID(id - 1);

        QVBoxLayout *layout = ui->scrollAreaVerticalLayout;
        layout->removeWidget(fileBar);
        layout->insertWidget(id - 1, fileBar);

        // Refresh File Bars
        lastFileBar->hide();
        lastFileBar->show();
        fileBar->hide();
        fileBar->show();

        std::swap(fileBars[id - 1], fileBars[id]);
    }

#if DEBUG
    printItems();
#endif

}

void Widget::onFileBarDown(FileBar *fileBar)
{
    unsigned int id = fileBar->getID();
    if (id < fileBars.size() - 1)
    {
        FileBar *nextFileBar = fileBars[id + 1];
        nextFileBar->setID(id);
        fileBar->setID(id + 1);

        QVBoxLayout *layout = ui->scrollAreaVerticalLayout;
        layout->removeWidget(fileBar);
        layout->insertWidget(id + 1, fileBar);

        // Refresh File Bars
        fileBar->hide();
        fileBar->show();
        nextFileBar->hide();
        nextFileBar->show();

        std::swap(fileBars[id + 1], fileBars[id]);
    }

#if DEBUG
    printItems();
#endif

}

void Widget::onFileBarDelete(FileBar *fileBar)
{
    unsigned int id = fileBar->getID();

    for (auto iter = fileBars.begin() + id + 1; iter != fileBars.end(); iter++)
    {
        FileBar *ptr = *iter;
        ptr->setID(ptr->getID() - 1);
    }

    fileBar->deleteLater();
    fileBars.erase(fileBars.begin() + id);

    if (fileBars.empty())
    {
        dropFileTip->show();
    }
    else
    {
        dropFileTip->hide();
    }

#if DEBUG
    printItems();
#endif

}

void Widget::onAddFileButtonClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open File"),
        "",
        tr("All Files (*)")
    );

    if (!filePath.isEmpty()) {
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();
        newFileBar(fileName);
    }

#if DEBUG
    printItems();
#endif

}
