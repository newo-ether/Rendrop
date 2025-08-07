// widget.cpp

#include <QString>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QMessageBox>
#include <QFontDatabase>
#include <QFont>

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
    QFontDatabase::addApplicationFont(":/font/BitcountSingle-Regular.ttf");
    ui->setupUi(this);

    blenderVersionManager = new BlenderVersionManager("config/blender_versions.txt");
    updateBlenderVersions();

    dropShadowRenderer = new DropShadowRenderer();

    addFileButton = new AddFileButton(ui->ContentWidget, dropShadowRenderer);
    QObject::connect(addFileButton, &AddFileButton::clicked, this, &Widget::onAddFileButtonClicked);
    ui->scrollAreaContainer->installEventFilter(this);

    ui->scrollAreaSizeWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->scrollAreaSizeWidget->installEventFilter(this);

    createDropShadowWidget(ui->ContentWidget, ui->selectorContainer, 12, 4, 4, 0.3f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->infoWidgetContainer, 12, 4, 4, 0.3f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->scrollAreaContainer, 12, 4, 4, 0.3f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->renderButton, 6, 4, 4, 0.3f, 10);

    dropFileTip = new DropFileTip(ui->ContentWidget);
    dropFileTip->setAttribute(Qt::WA_TransparentForMouseEvents);

    QGraphicsOpacityEffect *opacity = new QGraphicsOpacityEffect;
    opacity->setOpacity(0.5);
    dropFileTip->setGraphicsEffect(opacity);

    QObject::connect(ui->horizontalSlider, &QSlider::sliderPressed, this, &Widget::onSliderChanged);
    QObject::connect(ui->horizontalSlider, &QSlider::sliderMoved, this, &Widget::onSliderChanged);

    QObject::connect(ui->addButton, &QPushButton::clicked, this, &Widget::onAddBlenderVersionButtonClicked);
    QObject::connect(ui->deleteButton, &QPushButton::clicked, this, &Widget::onDeleteBlenderVersionButtonClicked);
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
    delete blenderVersionManager;
}

bool Widget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->scrollAreaContainer && event->type() == QEvent::Resize)
    {
        QWidget *scrollAreaContainer = ui->scrollAreaContainer;
        int x = scrollAreaContainer->x() + scrollAreaContainer->width();
        int y = scrollAreaContainer->y() + scrollAreaContainer->height();

        addFileButton->move(x - 110, y - 110);
        addFileButton->raise();

        dropFileTip->setGeometry(scrollAreaContainer->geometry());
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

void Widget::errorMessageBox(QString text)
{
    QMessageBox messageBox(this);
    QFont font("Bitcount Single", 12);
    messageBox.setIcon(QMessageBox::Critical);
    messageBox.setText(text);
    messageBox.setWindowTitle("Error");
    messageBox.setFont(font);
    messageBox.exec();
}

void Widget::warningMessageBox(QString text)
{
    QMessageBox messageBox(this);
    QFont font("Bitcount Single", 12);
    messageBox.setIcon(QMessageBox::Warning);
    messageBox.setText(text);
    messageBox.setWindowTitle("Error");
    messageBox.setFont(font);
    messageBox.exec();
}

void Widget::infoMessageBox(QString text)
{
    QMessageBox messageBox(this);
    QFont font("Bitcount Single", 12);
    messageBox.setIcon(QMessageBox::Information);
    messageBox.setText(text);
    messageBox.setWindowTitle("Error");
    messageBox.setFont(font);
    messageBox.exec();
}

void Widget::newFileBar(QString fileName)
{
    unsigned int id = static_cast<unsigned int>(fileBars.size());
    FileBar *fileBar = new FileBar(ui->scrollAreaContent, dropShadowRenderer, id);
    fileBar->setFileName(fileName);
    fileBar->setProgressBar(ui->horizontalSlider->sliderPosition() / 100.0f);

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
        tr("Blender Files (*.blend)")
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

void Widget::onSliderChanged()
{
    float position = ui->horizontalSlider->sliderPosition() / 100.0f;

    ui->currentProgressBar->setProgressBar(position);
    ui->totalProgressBar->setProgressBar(position);

    for (auto fileBar : fileBars)
    {
        fileBar->setProgressBar(position);
    }
}

void Widget::onAddBlenderVersionButtonClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select Blender Version"),
        "",
        tr("Blender Executable (blender.exe)")
    );

    if (!path.isEmpty()) {
        int result = blenderVersionManager->addBlenderVersion(path.toStdString());
        if (result != 0)
        {
            if (result == -1)
            {
                errorMessageBox(tr("Cannot open specified path."));
            }
            else if (result == -2)
            {
                errorMessageBox(tr("Failed to start Blender."));
            }
            else if (result == -3)
            {
                infoMessageBox(tr("Selected Blender version is already added."));
            }
            else
            {
                errorMessageBox(tr("Failed to save version configuration file."));
            }
        }
        else
        {
            updateBlenderVersions();
        }
    }
}

void Widget::onDeleteBlenderVersionButtonClicked()
{
    QString version = ui->comboBox->currentText();
    if (blenderVersionManager->deleteBlenderVersion(version.toStdString()) != 0)
    {
        errorMessageBox(tr("Failed to save version configuration file."));
    }
    updateBlenderVersions();
}

void Widget::updateBlenderVersions()
{
    ui->comboBox->clear();
    const std::vector<BlenderVersionManager::BlenderVersion> &versions = blenderVersionManager->getBlenderVersions();
    if (versions.empty())
    {
        ui->comboBox->addItem(tr("-- Select A Blender Version --"));
    }
    else
    {
        for (auto version : versions)
        {
            ui->comboBox->addItem(QString::fromStdString(version.version));
        }
    }
}
