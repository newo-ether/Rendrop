// widget.cpp

#include <QString>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QMessageBox>
#include <QFont>
#include <QProcess>
#include <QTemporaryFile>
#include <QThread>
#include <QThreadPool>
#include <QCloseEvent>

#include <vector>

#include "widget.h"
#include "ui_widget.h"

#include "file_bar.h"
#include "add_file_button.h"

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

#include "drop_file_tip.h"

Widget::Widget(int languageIndex, QWidget *parent):
    QWidget(parent),
    ui(new Ui::widget)
{
    ui->setupUi(this);

    ui->languageComboBox->setCurrentIndex(languageIndex);
    QObject::connect(ui->languageComboBox, &QComboBox::currentIndexChanged, this, &Widget::onLanguageChanged);

    blenderVersionManager = new BlenderVersionManager("config/blender_versions.cfg");
    updateBlenderVersions();

    qRegisterMetaType<BlenderFileInfo>("BlenderFileInfo");

    dropShadowRenderer = new DropShadowRenderer();

    handle = dropShadowRenderer->createWidgetBuffer([this]() { this->updateAllFileBarShadow(); });

    addFileButton = new AddFileButton(ui->ContentWidget, dropShadowRenderer);
    QObject::connect(addFileButton, &AddFileButton::clicked, this, &Widget::onAddFileButtonClicked);
    ui->scrollAreaContainer->installEventFilter(this);

    ui->scrollAreaSizeWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->scrollAreaSizeWidget->installEventFilter(this);

    createDropShadowWidget(ui->ContentWidget, ui->selectorContainer, 12, 4, 4, 0.4f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->infoWidgetContainer, 12, 4, 4, 0.4f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->scrollAreaContainer, 12, 4, 4, 0.4f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->renderButton, 15, 4, 4, 0.3f, 10);
    createDropShadowWidget(ui->ContentWidget, ui->outputContainer, 15, 4, 4, 0.3f, 10);

    dropFileTip = new DropFileTip(ui->ContentWidget);
    dropFileTip->setAttribute(Qt::WA_TransparentForMouseEvents);

    QGraphicsOpacityEffect *opacity = new QGraphicsOpacityEffect;
    opacity->setOpacity(0.5);
    dropFileTip->setGraphicsEffect(opacity);

    ui->renderButton->setText(tr("Start Render"));
    isRendering = false;
    clearOutputText();

    QObject::connect(ui->addButton, &QPushButton::clicked, this, &Widget::onAddBlenderVersionButtonClicked);
    QObject::connect(ui->deleteButton, &QPushButton::clicked, this, &Widget::onDeleteBlenderVersionButtonClicked);
    QObject::connect(ui->renderButton, &QPushButton::clicked, this, &Widget::onRenderButtonClicked);

    updateStatisticInfo();
    updateButtonStatus();
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
        FileBar::State state = fileBar->getState();
        if (state == FileBar::State::Loading)
        {
            fileBar->stopReading();
        }
        else if (state == FileBar::State::Rendering)
        {
            fileBar->stopRender();
        }
    }

    for (auto fileBar: fileBars)
    {
        delete fileBar;
    }

    delete ui;

    dropShadowRenderer->deleteWidgetBuffer(handle);
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

        dropShadowRenderer->setWidgetBuffer(
            handle,
            fileBarWidth + marginX * 2,
            fileBarHeight + marginY * 2,
            borderRadius,
            offsetX,
            offsetY,
            alphaMax,
            blurRadius
        );
        if (!fileBarShadowPixmap.isNull())
        {
            fileBarShadowPixmap = fileBarShadowPixmap.scaled(
                fileBarWidth + marginX * 2,
                fileBarHeight + marginY * 2,
                Qt::IgnoreAspectRatio,
                Qt::FastTransformation
            );
        }
    }

    return QWidget::eventFilter(watched, event);
}

void Widget::closeEvent(QCloseEvent *event)
{
    bool isLoading = false;
    if (!isRendering)
    {
        for (auto fileBar : fileBars)
        {
            FileBar::State state = fileBar->getState();
            if (state == FileBar::State::Loading)
            {
                isLoading = true;
                break;
            }
        }
    }

    QMessageBox::StandardButton result;
    if (isRendering)
    {
        result = warningMessageBox(
            tr("Warning"),
            tr("Some files are still rendering. Are you sure to exit?"),
            QMessageBox::Yes | QMessageBox::No
        );
    }
    else if (isLoading)
    {
        result = warningMessageBox(
            tr("Warning"),
            tr("Some files are still loading. Are you sure to exit?"),
            QMessageBox::Yes | QMessageBox::No
        );
    }
    else
    {
        event->accept();
        return;
    }

    if (result == QMessageBox::Yes)
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

QMessageBox::StandardButton Widget::errorMessageBox(QString title, QString text, QMessageBox::StandardButtons buttons)
{
    QMessageBox messageBox(this);
    QFont font("Pixel Mixed", 12);
    messageBox.setIcon(QMessageBox::Critical);
    messageBox.setText(text);
    messageBox.setWindowTitle(title);
    messageBox.setFont(font);
    messageBox.setStandardButtons(buttons);
    return static_cast<QMessageBox::StandardButton>(messageBox.exec());
}

QMessageBox::StandardButton Widget::warningMessageBox(QString title, QString text, QMessageBox::StandardButtons buttons)
{
    QMessageBox messageBox(this);
    QFont font("Pixel Mixed", 12);
    messageBox.setIcon(QMessageBox::Warning);
    messageBox.setText(text);
    messageBox.setWindowTitle(title);
    messageBox.setFont(font);
    messageBox.setStandardButtons(buttons);
    return static_cast<QMessageBox::StandardButton>(messageBox.exec());
}

QMessageBox::StandardButton Widget::infoMessageBox(QString title, QString text, QMessageBox::StandardButtons buttons)
{
    QMessageBox messageBox(this);
    QFont font("Pixel Mixed", 12);
    messageBox.setIcon(QMessageBox::Information);
    messageBox.setText(text);
    messageBox.setWindowTitle(title);
    messageBox.setFont(font);
    messageBox.setStandardButtons(buttons);
    return static_cast<QMessageBox::StandardButton>(messageBox.exec());
}

QMessageBox::StandardButton Widget::questionMessageBox(QString title, QString text, QMessageBox::StandardButtons buttons)
{
    QMessageBox messageBox(this);
    QFont font("Pixel Mixed", 12);
    messageBox.setIcon(QMessageBox::Question);
    messageBox.setText(text);
    messageBox.setWindowTitle(title);
    messageBox.setFont(font);
    messageBox.setStandardButtons(buttons);
    return static_cast<QMessageBox::StandardButton>(messageBox.exec());
}

FileBar *Widget::newFileBar(QString fileName, QString filePath)
{
    QString blenderPath = blenderVersionManager->getBlenderPath(ui->comboBox->currentText());
    FileBar *fileBar = new FileBar(
        ui->scrollAreaContent,
        dropShadowRenderer,
        fileName,
        filePath,
        blenderPath
    );

    DropShadowWidget *fileBarShadow = fileBar->getDropShadowWidget();
    fileBarShadow->setShadowUpdateEnabled(false);
    fileBarShadow->setShadowPixmap(&fileBarShadowPixmap);

    unsigned int index = static_cast<unsigned int>(fileBars.size());
    QVBoxLayout *layout = ui->scrollAreaVerticalLayout;
    layout->insertWidget(index, fileBar);

    QObject::connect(fileBar, &FileBar::upButtonClicked, this, &Widget::onFileBarUp);
    QObject::connect(fileBar, &FileBar::downButtonClicked, this, &Widget::onFileBarDown);
    QObject::connect(fileBar, &FileBar::deleteButtonClicked, this, &Widget::onFileBarDelete);
    QObject::connect(fileBar, &FileBar::reloadButtonClicked, this, &Widget::onFileBarReload);
    QObject::connect(fileBar, &FileBar::finishedReading, this, &Widget::onFinishedReading);
    QObject::connect(fileBar, &FileBar::finishedRendering, this, &Widget::onFinishedRendering);
    QObject::connect(fileBar, &FileBar::progressChanged, this, &Widget::onProgressChanged);
    QObject::connect(fileBar, &FileBar::outputTextUpdate, this, &Widget::onOutputTextUpdate);

    fileBars.push_back(fileBar);
    dropFileTip->hide();

    return fileBar;
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

void Widget::onLanguageChanged(int index)
{
    QDir configDir = QFileInfo("config/language.cfg").absoluteDir();
    if (!configDir.exists())
    {
        configDir.mkpath(configDir.absolutePath());
    }

    QFile configFile("config/language.cfg");
    if (configFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        configFile.write(index == 0 ? "en_US\n" : "zh_CN\n");
        configFile.close();
    }

    auto result = warningMessageBox(
        tr("Warning"),
        tr("Language settings will take effect after restarting the program. Restart now?"),
        QMessageBox::Yes | QMessageBox::No
    );

    if (result == QMessageBox::Yes)
    {
        QString program = QCoreApplication::applicationFilePath();
        QStringList arguments = QCoreApplication::arguments();
        QProcess::startDetached(program, arguments);
        QCoreApplication::quit();
    }
}

void Widget::onFileBarUp(FileBar *fileBar)
{
    if (fileBars.empty())
    {
        return;
    }

    auto iter = std::find(fileBars.begin(), fileBars.end(), fileBar);
    if (iter > fileBars.begin() && iter < fileBars.end())
    {
        FileBar *lastFileBar = *(iter - 1);

        QVBoxLayout *layout = ui->scrollAreaVerticalLayout;
        layout->removeWidget(fileBar);
        layout->insertWidget(iter - fileBars.begin() - 1, fileBar);

        // Refresh File Bars
        lastFileBar->hide();
        fileBar->hide();
        lastFileBar->show();
        fileBar->show();

        std::swap(*iter, *(iter - 1));
    }
}

void Widget::onFileBarDown(FileBar *fileBar)
{
    if (fileBars.empty())
    {
        return;
    }

    auto iter = std::find(fileBars.begin(), fileBars.end(), fileBar);
    if (iter >= fileBars.begin() && iter < fileBars.end() - 1)
    {
        FileBar *nextFileBar = *(iter + 1);

        QVBoxLayout *layout = ui->scrollAreaVerticalLayout;
        layout->removeWidget(fileBar);
        layout->insertWidget(iter - fileBars.begin() + 1, fileBar);

        // Refresh File Bars
        fileBar->hide();
        nextFileBar->hide();
        fileBar->show();
        nextFileBar->show();

        std::swap(*iter, *(iter + 1));
    }
}

void Widget::onFileBarDelete(FileBar *fileBar)
{
    if (fileBar->getState() == FileBar::State::Loading)
    {
        auto result = warningMessageBox(
            tr("Warning"),
            fileBar->getFileName()
                + tr(" is still loading. Are you sure to delete?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (result == QMessageBox::No)
        {
            return;
        }
    }

    if (fileBar->getState() == FileBar::State::Rendering)
    {
        auto result = warningMessageBox(
            tr("Warning"),
            fileBar->getFileName()
                + tr(" is still rendering. Are you sure to delete?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (result == QMessageBox::No)
        {
            return;
        }
    }

    auto iter = std::find(fileBars.begin(), fileBars.end(), fileBar);
    if (iter == fileBars.end())
    {
        return;
    }

    QVBoxLayout *layout = ui->scrollAreaVerticalLayout;
    layout->removeWidget(fileBar);

    fileBar->deleteLater();
    fileBars.erase(iter);

    if (fileBars.empty())
    {
        dropFileTip->show();
    }
    else
    {
        dropFileTip->hide();
    }

    updateButtonStatus();
    updateStatisticInfo();
}

void Widget::onFileBarReload(FileBar *)
{
    updateButtonStatus();
    updateStatisticInfo();
}

void Widget::onFinishedReading()
{
    updateButtonStatus();
    updateStatisticInfo();
}

void Widget::onAddFileButtonClicked()
{
    QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        tr("Open File"),
        "",
        tr("Blender Files (*.blend)")
    );

    for (QString &filePath : filePaths)
    {
        if (!filePath.isEmpty())
        {
            QFileInfo fileInfo(filePath);
            QString fileName = fileInfo.completeBaseName();
            newFileBar(fileName, filePath);
        }
    }

    updateButtonStatus();
    updateStatisticInfo();
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
        int result = blenderVersionManager->addBlenderVersion(path);
        if (result != 0)
        {
            if (result == -1)
            {
                errorMessageBox(tr("Error"), tr("Cannot open specified path."));
            }
            else if (result == -2)
            {
                errorMessageBox(tr("Error"), tr("Failed to start Blender."));
            }
            else if (result == -3)
            {
                infoMessageBox(tr("Note"), tr("Selected Blender version is already added."));
            }
            else
            {
                errorMessageBox(tr("Error"), tr("Failed to save version configuration file."));
            }
        }
        else
        {
            updateBlenderVersions();
        }
    }

    updateButtonStatus();
}

void Widget::onDeleteBlenderVersionButtonClicked()
{
    if (blenderVersionManager->getBlenderVersionCount() > 0)
    {
        QString version = ui->comboBox->currentText();
        if (blenderVersionManager->deleteBlenderVersion(version) != 0)
        {
            errorMessageBox(tr("Error"), tr("Failed to save version configuration file."));
        }
        updateBlenderVersions();
    }

    updateButtonStatus();
}

void Widget::onRenderButtonClicked()
{
    if (!isRendering)
    {
        FileBar *firstRender = nullptr;
        for (auto fileBar: fileBars)
        {
            if (fileBar->getState() == FileBar::State::Queued)
            {
                firstRender = fileBar;
                break;
            }
        }

        if (firstRender)
        {
            firstRender->render();

            ui->renderButton->setText(tr("Stop Render"));
            isRendering = true;

            setSelectorEnabled(false);
        }
    }
    else
    {
        for (auto fileBar: fileBars)
        {
            if (fileBar->getState() == FileBar::State::Rendering)
            {
                fileBar->stopRender();
            }
        }

        ui->renderButton->setText(tr("Start Render"));
        isRendering = false;

        setSelectorEnabled(true);
    }

    clearOutputText();
}

void Widget::onFinishedRendering()
{
    FileBar *nextRender = nullptr;
    for (auto fileBar: fileBars)
    {
        if (fileBar->getState() == FileBar::State::Queued)
        {
            nextRender = fileBar;
            break;
        }
    }

    if (nextRender)
    {
        nextRender->render();
    }
    else
    {
        ui->renderButton->setText(tr("Start Render"));
        isRendering = false;

        setSelectorEnabled(true);
    }

    updateButtonStatus();
    updateStatisticInfo();
    clearOutputText();
}

void Widget::onProgressChanged()
{
    updateStatisticInfo();
}

void Widget::onOutputTextUpdate(QString text)
{
    ui->outputLabel->setText(text);
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
        for (auto &version : versions)
        {
            ui->comboBox->addItem(version.version);
        }
    }
}

void Widget::updateButtonStatus()
{
    bool blenderExists = blenderVersionManager->getBlenderVersionCount() > 0;
    addFileButton->setEnabled(blenderExists);

    bool isLoading = false;
    for (auto fileBar: fileBars)
    {
        if (fileBar->getState() == FileBar::State::Loading)
        {
            isLoading = true;
            break;
        }
    }

    bool isQueued = false;
    for (auto fileBar: fileBars)
    {
        if (fileBar->getState() == FileBar::State::Queued)
        {
            isQueued = true;
            break;
        }
    }

    if (isRendering)
    {
        ui->renderButton->setEnabled(true);
    }
    else
    {
        ui->renderButton->setEnabled(!fileBars.empty() && blenderExists && !isLoading && isQueued);
    }
}

void Widget::setSelectorEnabled(bool enable)
{
    ui->comboBox->setEnabled(enable);
    ui->addButton->setEnabled(enable);
    ui->deleteButton->setEnabled(enable);
}

void Widget::updateStatisticInfo()
{
    int frameFinished = 0, frameTotal = 0;
    int projectFinished = 0, projectTotal = 0;

    for (auto fileBar: fileBars)
    {
        FileBar::State state = fileBar->getState();
        if (state == FileBar::State::Finished)
        {
            projectFinished++;
        }

        frameFinished += fileBar->getFinishedFrame();
        frameTotal += fileBar->getTotalFrame();
    }
    projectTotal = static_cast<int>(fileBars.size());

    ui->framesLabel->setText(
        QString::number(frameFinished)
        + " / "
        + QString::number(frameTotal));

    ui->projectsLabel->setText(
        QString::number(projectFinished)
        + " / "
        + QString::number(projectTotal));

    float frameProgress = frameTotal == 0 ? 0.0f : static_cast<float>(frameFinished) / frameTotal * 100.0f;
    float projectProgress = projectTotal == 0 ? 0.0f : static_cast<float>(projectFinished) / projectTotal * 100.0f;

    ui->framesProgressBar->setProgressBar(frameProgress);
    ui->projectsProgressBar->setProgressBar(projectProgress);
}

void Widget::clearOutputText()
{
    ui->outputLabel->setText(tr("Ready to render."));
}

void Widget::updateAllFileBarShadow()
{
    fileBarShadowPixmap = dropShadowRenderer->getPixmap(handle);

    for (auto &fileBar : fileBars)
    {
        fileBar->getDropShadowWidget()->update();
    }
}
