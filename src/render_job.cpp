// render_job.cpp

#include <QThread>

#include "render_job.h"
#include "blender_file_reader.h"
#include "blender_renderer.h"

RenderJob::RenderJob(
    const QUuid &id,
    const QString &name,
    const QString &filePath,
    const QString &blenderPath,
    QObject *parent
):
    RenderJob(
        ProjectInfo(
            id,
            name,
            filePath,
            QString(),
            ProjectState::Loading,
            0,
            0,
            0,
            0,
            0,
            0,
            QString(),
            0,
            0
        ),
        blenderPath,
        parent
    )
{
}

RenderJob::RenderJob(
    const ProjectInfo &project,
    const QString &blenderPath,
    QObject *parent
):
    QObject(parent),
    project(project),
    blenderPath(blenderPath),
    fileReader(new BlenderFileReader()),
    renderer(new BlenderRenderer())
{
    fileReader->setParameter(project.path, blenderPath);
    configureRendererFromProject();
    connectWorkers();
}

void RenderJob::connectWorkers()
{
    QObject::connect(fileReader, &BlenderFileReader::finishedReading, this, &RenderJob::onFinishedReading);
    QObject::connect(renderer, &BlenderRenderer::progressChanged, this, &RenderJob::onProgressChanged);
    QObject::connect(renderer, &BlenderRenderer::outputTextUpdate, this, &RenderJob::onOutputTextUpdated);
    QObject::connect(renderer, &BlenderRenderer::finishedRendering, this, &RenderJob::onFinishedRendering);
}

void RenderJob::configureRendererFromProject()
{
    renderer->setParameter(
        blenderPath,
        project.path,
        project.frameStart,
        project.frameEnd,
        project.frameStep,
        false
    );
    int resumeFrame = project.frameStart;
    if (project.frameStep > 0
        && project.finishedFrame > 0
        && project.finishedFrame < project.totalFrame)
    {
        resumeFrame += project.finishedFrame * project.frameStep;
    }
    renderer->setCurrentFrame(resumeFrame);
}

RenderJob::~RenderJob()
{
    shutdown();
    delete fileReader;
    delete renderer;
}

QUuid RenderJob::id() const
{
    return project.id;
}

ProjectInfo RenderJob::snapshot() const
{
    return project;
}

void RenderJob::setName(const QString &name)
{
    if (shuttingDown || name.isEmpty() || project.name == name)
    {
        return;
    }
    project.name = name;
    publishSnapshot();
}

void RenderJob::setBlenderPath(const QString &newBlenderPath)
{
    if (shuttingDown)
    {
        return;
    }
    blenderPath = newBlenderPath;
    if (!fileReader->isRunning())
    {
        fileReader->setParameter(project.path, blenderPath);
    }
    if (!renderer->isRunning())
    {
        renderer->setParameter(
            blenderPath,
            project.path,
            project.frameStart,
            project.frameEnd,
            project.frameStep,
            false
        );
    }
}

bool RenderJob::setFrameRange(int frameStart, int frameEnd, int frameStep)
{
    if (shuttingDown
        || project.state == ProjectState::Loading
        || project.state == ProjectState::Rendering
        || project.state == ProjectState::Error
        || frameStep <= 0
        || frameEnd < frameStart)
    {
        return false;
    }
    project.frameStart = frameStart;
    project.frameEnd = frameEnd;
    project.frameStep = frameStep;
    project.finishedFrame = 0;
    project.totalFrame = (frameEnd - frameStart) / frameStep + 1;
    project.state = ProjectState::Queued;
    renderer->setFrame(frameStart, frameEnd, frameStep);
    renderer->setCurrentFrame(frameStart);
    publishSnapshot();
    return true;
}

bool RenderJob::reload()
{
    if (shuttingDown || renderer->isRunning())
    {
        return false;
    }
    if (fileReader->isRunning())
    {
        fileReader->stop();
        fileReader->wait();
    }
    resetProjectData();
    project.state = ProjectState::Loading;
    publishSnapshot();
    fileReader->setParameter(project.path, blenderPath);
    fileReader->prepareStart();
    fileReader->start(QThread::LowestPriority);
    return true;
}

bool RenderJob::startRender()
{
    if (shuttingDown || project.state != ProjectState::Queued || renderer->isRunning())
    {
        return false;
    }
    project.state = ProjectState::Rendering;
    publishSnapshot();
    renderer->prepareStart();
    renderer->start(QThread::LowestPriority);
    return true;
}

void RenderJob::stopRender()
{
    if (shuttingDown)
    {
        return;
    }
    if (renderer->isRunning())
    {
        renderer->stop();
        renderer->wait();
    }
    if (project.state == ProjectState::Rendering)
    {
        project.state = ProjectState::Queued;
        publishSnapshot();
    }
}

void RenderJob::shutdown()
{
    if (shuttingDown)
    {
        return;
    }
    shuttingDown = true;
    if (fileReader->isRunning())
    {
        fileReader->stop();
        fileReader->wait();
    }
    if (renderer->isRunning())
    {
        renderer->stop();
        renderer->wait();
    }
}

void RenderJob::onFinishedReading(int status, BlenderFileInfo info)
{
    if (shuttingDown)
    {
        return;
    }
    if (status == 0)
    {
        project.frameStart = info.frameBegin;
        project.frameEnd = info.frameEnd;
        project.frameStep = info.frameStep;
        project.resolutionX = info.resolutionX;
        project.resolutionY = info.resolutionY;
        project.resolutionScale = info.resolutionScale;
        project.renderEngine = info.renderEngine;
        project.outputPath = info.outputPath;
        project.finishedFrame = 0;
        project.totalFrame = info.frameStep <= 0
            ? 0
            : (info.frameEnd - info.frameBegin) / info.frameStep + 1;
        project.state = project.outputPath.isEmpty() || project.totalFrame <= 0
            ? ProjectState::Error
            : ProjectState::Queued;
        renderer->setParameter(
            blenderPath,
            project.path,
            project.frameStart,
            project.frameEnd,
            project.frameStep,
            true
        );
    }
    else
    {
        resetProjectData();
        project.state = ProjectState::Error;
        renderer->setParameter(blenderPath, project.path, 0, 0, 0, true);
    }
    publishSnapshot();
}

void RenderJob::onProgressChanged()
{
    if (shuttingDown || project.state != ProjectState::Rendering)
    {
        return;
    }
    project.finishedFrame = renderer->getFinishedFrame();
    project.totalFrame = renderer->getTotalFrame();
    project.updateCompletionState();
    publishSnapshot();
}

void RenderJob::onOutputTextUpdated(QString text)
{
    if (!shuttingDown)
    {
        emit outputTextUpdated(text);
    }
}

void RenderJob::onFinishedRendering(int status)
{
    if (shuttingDown)
    {
        return;
    }
    if (status > 0)
    {
        return;
    }
    if (status == 0)
    {
        project.finishedFrame = project.totalFrame;
        project.state = ProjectState::Finished;
    }
    else if (project.state != ProjectState::Queued)
    {
        project.state = ProjectState::Error;
    }
    publishSnapshot();
    emit renderFinished(project.id);
}

void RenderJob::publishSnapshot()
{
    emit snapshotChanged(project);
}

void RenderJob::resetProjectData()
{
    project.outputPath.clear();
    project.frameStart = 0;
    project.frameEnd = 0;
    project.frameStep = 0;
    project.resolutionX = 0;
    project.resolutionY = 0;
    project.resolutionScale = 0;
    project.renderEngine.clear();
    project.finishedFrame = 0;
    project.totalFrame = 0;
}
