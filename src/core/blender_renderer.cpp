// blender_renderer.cpp

#include <QRegularExpression>

#include "core/blender_renderer.h"
#include "core/simple_process.h"

BlenderRenderer::BlenderRenderer(QObject *parent):
    QThread(parent),
    frameStart(0),
    frameEnd(0),
    frameStep(0),
    currentFrame(0),
    currentFrameFinished(false),
    isParameterSet(false),
    stopped(false)
{
}

BlenderRenderer::~BlenderRenderer()
{
    if (isRunning())
    {
        stop();
        wait();
    }
}

void BlenderRenderer::setParameter(
    QString blenderPath,
    QString filePath,
    int frameStart,
    int frameEnd,
    int frameStep,
    bool resetCurrentFrame
)
{
    this->blenderPath = blenderPath;
    this->filePath = filePath;
    this->frameStart = frameStart;
    this->frameEnd = frameEnd;
    this->frameStep = frameStep;
    if (resetCurrentFrame)
    {
        currentFrame = frameStart;
    }
    currentFrameFinished = false;
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

void BlenderRenderer::setOutputPath(const QString &outputPath)
{
    this->outputPath = outputPath;
}

void BlenderRenderer::prepareStart()
{
    stopped = false;
}

void BlenderRenderer::stop()
{
    stopped = true;
}

void BlenderRenderer::run()
{
    currentFrameFinished = false;
    if (!isParameterSet)
    {
        emit finishedRendering(-1);
        return;
    }

    SimpleProcess process;
    QStringList arguments;
    arguments << "-b"
              << filePath;
    if (!outputPath.isEmpty())
    {
        arguments << "-o" << outputPath;
    }
    arguments << "-s"
              << QString::number(currentFrame)
              << "-e"
              << QString::number(frameEnd)
              << "-j"
              << QString::number(frameStep)
              << "-a";
    if (process.start(blenderPath, arguments) != 0)
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
            emit finishedRendering(1);
            return;
        }
        if (getFinishedFrame() == getTotalFrame())
        {
            process.kill();
            break;
        }
        const QString output = process.readStandardOutput();
        if (!output.isEmpty())
        {
            parseOutput(output);
        }
        process.waitForFinished(50);
    }

    const QString output = process.readStandardOutput();
    if (!output.isEmpty())
    {
        parseOutput(output);
    }
    if (stopped)
    {
        emit finishedRendering(1);
        return;
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
    if (frameStep <= 0)
    {
        return 0;
    }
    return (currentFrame - frameStart) / frameStep + (currentFrameFinished ? 1 : 0);
}

int BlenderRenderer::getTotalFrame() const
{
    if (frameStep <= 0)
    {
        return 0;
    }
    return (frameEnd - frameStart) / frameStep + 1;
}

void BlenderRenderer::parseOutput(QString output)
{
    // Cycles and EEVEE print "Fra:N ..." status lines. Blender 5 logs a
    // "Rendering frame N" line per frame instead, and Workbench prints only
    // that one, so either starts a frame.
    static const QRegularExpression framePattern("^.*(Fra:\\s*(\\d+).+)$");
    static const QRegularExpression frameStartPattern("^.*\\b(Rendering frame\\s+(\\d+))\\s*$");
    static const QRegularExpression savePattern("^.*(Saved:.+)$");
    const QStringList lines = output.split('\n');
    QString outputText;
    bool matched = false;
    for (const QString &line : lines)
    {
        QRegularExpressionMatch frameMatch = framePattern.match(line);
        if (!frameMatch.hasMatch())
        {
            frameMatch = frameStartPattern.match(line);
        }
        if (frameMatch.hasMatch())
        {
            matched = true;
            currentFrame = frameMatch.captured(2).toInt();
            outputText = frameMatch.captured(1).trimmed();
            currentFrameFinished = false;
        }
        const QRegularExpressionMatch saveMatch = savePattern.match(line);
        if (saveMatch.hasMatch())
        {
            matched = true;
            outputText = saveMatch.captured(1).trimmed();
            currentFrameFinished = true;
        }
    }
    if (matched)
    {
        emit progressChanged();
        emit outputTextUpdate(outputText);
    }
}
