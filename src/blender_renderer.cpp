// blender_renderer.cpp

#include <QString>
#include <QObject>
#include <QProcess>
#include <QByteArray>
#include <QTimer>

#include "blender_renderer.h"

BlenderRenderer::BlenderRenderer(QObject *parent):
    QThread(parent),
    isParameterSet(false),
    stopped(false)
{
    blenderProcess = new QProcess();
}

BlenderRenderer::~BlenderRenderer()
{
    delete blenderProcess;
}

void BlenderRenderer::setParameter(
    QString blenderPath,
    QString filePath,
    int frameStart,
    int frameEnd,
    int frameStep
) {
    this->blenderPath = blenderPath;
    this->filePath = filePath;
    this->frameStart = frameStart;
    this->frameEnd = frameEnd;
    this->frameStep = frameStep;
    this->currentFrame = frameStart;
    isParameterSet = true;
}

void BlenderRenderer::setFrame(int frameStart, int frameEnd, int frameStep)
{
    this->frameStart = frameStart;
    this->frameEnd = frameEnd;
    this->frameStep = frameStep;
}

void BlenderRenderer::setCurrentFrame(int currentFrame)
{
    this->currentFrame = currentFrame;
}

void BlenderRenderer::stop()
{
    stopped = true;
}

void BlenderRenderer::run()
{
    stopped = false;

    if (!isParameterSet)
    {
        emit finishedRendering(-1);
        return;
    }

    while (currentFrame < frameEnd)
    {
        if (stopped)
        {
            return;
        }

        QThread::msleep(200);
        currentFrame += frameStep;
        emit progressChanged();
    }

/*
    QStringList args;
    args << "-b"
         << filePath
         << "-a";

    blenderProcess->start(blenderPath, args);
    blenderProcess->waitForFinished(1000000000);
*/

    emit finishedRendering(0);
}

int BlenderRenderer::getFinishedFrame() const
{
    return currentFrame - frameStart + 1;
}

int BlenderRenderer::getTotalFrame() const
{
    return frameStep == 0 ? 0 : (frameEnd - frameStart) / frameStep + 1;
}
