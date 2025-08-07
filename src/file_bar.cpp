// file_bar.cpp

#include <QTimer>
#include <QElapsedTimer>

#include "file_bar.h"
#include "ui_file_bar.h"

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

FileBar::FileBar(
    QWidget *parent,
    DropShadowRenderer *dropShadowRenderer,
    unsigned int id
):
    QWidget(parent),
    ui(new Ui::fileBar),
    value(0),
    targetValue(0),
    velocity(0),
    stiffness(300),
    damping(50),
    id(id)
{
    ui->setupUi(this);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &FileBar::updateProgressBar);
    timer->start(8);

    elapsedTimer = new QElapsedTimer();
    elapsedTimer->start();
    lastElapsed = 0;

    QObject::connect(ui->upButton, &QPushButton::clicked, this, &FileBar::onUpButtonClicked);
    QObject::connect(ui->downButton, &QPushButton::clicked, this, &FileBar::onDownButtonClicked);
    QObject::connect(ui->deleteButton, &QPushButton::clicked, this, &FileBar::onDeleteButtonClicked);

    dropShadowWidget = new DropShadowWidget(
        parent,
        this,
        dropShadowRenderer
    );
    dropShadowWidget->setBorderRadius(16);
    dropShadowWidget->setOffsetX(2);
    dropShadowWidget->setOffsetY(2);
    dropShadowWidget->setAlphaMax(0.3f);
    dropShadowWidget->setBlurRadius(10);

    show();
    dropShadowWidget->show();
}

FileBar::~FileBar()
{
    delete dropShadowWidget;
    delete ui;
}

void FileBar::setFileName(QString fileName)
{
    ui->fileNameLabel->setText(fileName);
}

QString FileBar::getFileName() const
{
    return ui->fileNameLabel->text();
}

DropShadowWidget * FileBar::getDropShadowWidget() const
{
    return dropShadowWidget;
}

void FileBar::setID(unsigned int id)
{
    this->id = id;
}

unsigned int FileBar::getID() const
{
    return id;
}

void FileBar::setProgressBar(float value)
{
    this->targetValue = value;
}

void FileBar::onUpButtonClicked()
{
    emit upButtonClicked(this);
}

void FileBar::onDownButtonClicked()
{
    emit downButtonClicked(this);
}

void FileBar::onDeleteButtonClicked()
{
    emit deleteButtonClicked(this);
}

void FileBar::updateProgressBar()
{
    qint64 elapsed = elapsedTimer->elapsed();
    float dt = static_cast<float>(elapsed - lastElapsed) / 1000;
    lastElapsed = elapsed;

    int iteration = std::max(1, (int)(dt / 0.01f));
    dt /= iteration;

    for (int i = 0; i < iteration; i++)
    {
        float delta = targetValue - value;
        float accel = stiffness * delta - damping * velocity;

        velocity += accel * dt;
        value += velocity * dt;
        value = std::clamp(value, 0.0f, 100.0f);

        if (std::abs(delta) < 0.1f && std::abs(velocity) < 0.1f) {
            velocity = 0;
            value = targetValue;
        }
    }

    ui->progressBar->setValue(std::clamp(value, 0.0f, 100.0f) * 100.0f);
    update();
}
