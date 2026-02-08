// blender_renderer.cpp

#include <QString>
#include <QObject>
#include <QTimer>
#include <QRegularExpression>

#include "blender_renderer.h"
#include "simple_process.h"

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

    SimpleProcess process;

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
            emit finishedRendering(-1);
            return;
        }

        if (getFinishedFrame() == getTotalFrame())
        {
            process.kill();
            break;
        }

        QString output = process.readStandardOutput();
        if (!output.isEmpty())
        {
            parseOutput(output);
        }

        process.waitForFinished(50);
    }

    QString remaining = process.readRemaining();
    if (!remaining.isEmpty())
    {
        parseOutput(remaining);
    }

    if (getFinishedFrame() < getTotalFrame())
    {
        emit finishedRendering(-1);
        return;
    }

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

void BlenderRenderer::parseOutput(QString output)
{
    static QRegularExpression reFrameOld("^Fra:\\s*(\\d+).+$");
    static QRegularExpression reFrameNew("^\\d+:\\d+\\.\\d+\\s+render\\s+\\|\\s+(Fra:\\s*(\\d+).+)$");
    static QRegularExpression reSaveOld("^Saved:.+$");
    static QRegularExpression reSaveNew("^\\d+:\\d+\\.\\d+\\s+render\\s+\\|\\s+(Saved:.+)$");

    QStringList lines = output.split('\n');
    QString outputText;
    bool hasMatch = false;

    for (QString &line: lines)
    {
        auto frameMatch = reFrameOld.match(line);
        if (!frameMatch.hasMatch()) {
            frameMatch = reFrameNew.match(line);
        }

        if (frameMatch.hasMatch()) {
            hasMatch = true;
            if (frameMatch.lastCapturedIndex() == 2)
            {
                currentFrame = frameMatch.captured(2).toInt();
                outputText = frameMatch.captured(1).trimmed();
            }
            else
            {
                currentFrame = frameMatch.captured(1).toInt();
                outputText = frameMatch.captured(0).trimmed();
            }
            currentFrameFinished = false;
        }

        auto saveMatch = reSaveOld.match(line);
        if (!saveMatch.hasMatch()) {
            saveMatch = reSaveNew.match(line);
        }

        if (saveMatch.hasMatch()) {
            hasMatch = true;
            if (saveMatch.lastCapturedIndex() == 1)
            {
                outputText = saveMatch.captured(1).trimmed();
            }
            else
            {
                outputText = saveMatch.captured(0).trimmed();
            }
            currentFrameFinished = true;

        }
    }

    if (hasMatch)
    {
        emit progressChanged();
        emit outputTextUpdate(outputText);
    }
}
