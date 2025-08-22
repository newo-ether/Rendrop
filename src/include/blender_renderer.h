// blender_renderer.h

#ifndef BLENDER_RENDERER_H
#define BLENDER_RENDERER_H

#include <QString>
#include <QProcess>
#include <QThread>

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
    void finishedRendering(int status);

private:
    QProcess *blenderProcess;
    QString blenderPath, filePath;
    int frameStart, frameEnd, frameStep;
    int currentFrame;
    bool isParameterSet;
    bool stopped;
};

#endif // BLENDER_RENDERER_H
