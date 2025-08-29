// blender_renderer.cpp

#include <QString>
#include <QObject>
#include <QTimer>
#include <QRegularExpression>

#include "blender_renderer.h"
#include "process.h"

BlenderRenderer::BlenderRenderer(QObject *parent):
    QThread(parent),
    isParameterSet(false),
    stopped(false) {}

BlenderRenderer::~BlenderRenderer() {}

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
    this->currentFrameFinished = false;
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
    currentFrameFinished = false;

    if (!isParameterSet)
    {
        emit finishedRendering(-1);
        return;
    }

    Process process;

    QStringList args;
    args << "-b"
         << filePath
         << "-s"
         << QString::number(currentFrame)
         << "-e"
         << QString::number(frameEnd)
         << "-j"
         << QString::number(frameStep)
         << "-a";

    if (process.start(blenderPath, args) != 0)
    {
        emit finishedRendering(-1);
        return;
    }

    emit outputTextUpdate(tr("Starting Blender..."));

    while (process.isRunning())
    {
        if (stopped)
        {
            process.kill();
            return;
        }

        QString output = process.readStandardOutput();
        if (!output.isEmpty())
        {
            parseOutput(output);
        }

        process.waitForFinished(50);
    }

    currentFrameFinished = true;
    emit progressChanged();
    emit finishedRendering(0);
}

int BlenderRenderer::getFinishedFrame() const
{
    return currentFrame - frameStart + (currentFrameFinished ? 1 : 0);
}

int BlenderRenderer::getTotalFrame() const
{
    return frameStep == 0 ? 0 : (frameEnd - frameStart) / frameStep + 1;
}

void BlenderRenderer::parseOutput(QString line)
{
    static QRegularExpression re("^Fra:\\s*(\\d+).*(\\r?\\n)?$");
    auto match = re.match(line);
    if (match.hasMatch()) {
        currentFrame = match.captured(1).toInt();
        emit progressChanged();
        emit outputTextUpdate(match.captured().trimmed());
    }
}
