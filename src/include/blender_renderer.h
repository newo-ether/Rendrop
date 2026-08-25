// blender_renderer.h

#ifndef BLENDER_RENDERER_H
#define BLENDER_RENDERER_H

#include <atomic>

#include <QString>
#include <QThread>

class BlenderRenderer : public QThread
{
    Q_OBJECT

public:
    explicit BlenderRenderer(QObject *parent = nullptr);
    ~BlenderRenderer() override;
    void setParameter(
        QString blenderPath,
        QString filePath,
        int frameStart,
        int frameEnd,
        int frameStep,
        bool resetCurrentFrame
    );
    void setFrame(int frameStart, int frameEnd, int frameStep);
    void setCurrentFrame(int currentFrame);
    int getFinishedFrame() const;
    int getTotalFrame() const;
    void prepareStart();
    void stop();

protected:
    void run() override;

signals:
    void progressChanged();
    void outputTextUpdate(QString text);
    void finishedRendering(int status);

private:
    void parseOutput(QString output);

    QString blenderPath;
    QString filePath;
    std::atomic_int frameStart;
    std::atomic_int frameEnd;
    std::atomic_int frameStep;
    std::atomic_int currentFrame;
    std::atomic_bool currentFrameFinished;
    std::atomic_bool isParameterSet;
    std::atomic_bool stopped;
};

#endif // BLENDER_RENDERER_H
