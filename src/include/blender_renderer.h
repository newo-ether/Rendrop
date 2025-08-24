// blender_renderer.h

#ifndef BLENDER_RENDERER_H
#define BLENDER_RENDERER_H

#include <QString>
#include <QProcess>
#include <QThread>
#include <atomic>

class BlenderRenderer : public QThread
{
    Q_OBJECT

public:
    BlenderRenderer(QObject *parent = nullptr);
    ~BlenderRenderer();

    void setParameter(
        QString blenderPath,
        QString filePath,
        int frameStart,
        int frameEnd,
        int frameStep
    );
    void setFrame(int frameStart, int frameEnd, int frameStep);
    void setCurrentFrame(int currentFrame);
    int getFinishedFrame() const;
    int getTotalFrame() const;
    void stop();

protected:
    void run() override;

signals:
    void progressChanged();
    void outputTextUpdate(QString text);
    void finishedRendering(int status);

private:
    void parseOutput(QString line);

private:
    QString blenderPath, filePath;
    std::atomic_int frameStart, frameEnd, frameStep;
    std::atomic_int currentFrame;
    std::atomic_bool currentFrameFinished;
    std::atomic_bool isParameterSet;
    std::atomic_bool stopped;
};

#endif // BLENDER_RENDERER_H
