// drop_shadow_renderer.h

#ifndef DROP_SHADOW_RENDERER_H
#define DROP_SHADOW_RENDERER_H

#include <QImage>
#include <QPixmap>
#include <QObject>
#include <QThread>
#include <QReadWriteLock>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

#include <vector>
#include <limits>
#include <atomic>
#include <functional>

class DropShadowRenderer : public QThread
{
    Q_OBJECT

public:
    DropShadowRenderer(QObject *parent = nullptr);
    ~DropShadowRenderer();

    int createWidgetBuffer(std::function<void ()> updateFunc);
    void deleteWidgetBuffer(int handle);
    void setWidgetBuffer(
        int handle,
        float widgetWidth,
        float widgetHeight,
        float borderRadius,
        float offsetX,
        float offsetY,
        float alphaMax,
        float blurRadius
    );
    void setWidgetBufferUpdateEnabled(int handle, bool enabled);
    QPixmap getPixmap(int handle);

protected:
    void run() override;

private:
    struct WidgetInfo
    {
        std::atomic<float> widgetWidth;
        std::atomic<float> widgetHeight;
        std::atomic<float> borderRadius;
        std::atomic<float> offsetX;
        std::atomic<float> offsetY;
        std::atomic<float> alphaMax;
        std::atomic<float> blurRadius;

        inline WidgetInfo():
            widgetWidth(0),
            widgetHeight(0),
            borderRadius(0),
            offsetX(0),
            offsetY(0),
            alphaMax(0),
            blurRadius(0) {}

        inline WidgetInfo(
            float widgetWidth,
            float widgetHeight,
            float borderRadius,
            float offsetX,
            float offsetY,
            float alphaMax,
            float blurRadius
        ):
            widgetWidth(widgetWidth),
            widgetHeight(widgetHeight),
            borderRadius(borderRadius),
            offsetX(offsetX),
            offsetY(offsetY),
            alphaMax(alphaMax),
            blurRadius(blurRadius) {}

        inline WidgetInfo(const WidgetInfo &info):
            widgetWidth(info.widgetWidth.load()),
            widgetHeight(info.widgetHeight.load()),
            borderRadius(info.borderRadius.load()),
            offsetX(info.offsetX.load()),
            offsetY(info.offsetY.load()),
            alphaMax(info.alphaMax.load()),
            blurRadius(info.blurRadius.load()) {}

        inline WidgetInfo(WidgetInfo &&info):
            widgetWidth(info.widgetWidth.load()),
            widgetHeight(info.widgetHeight.load()),
            borderRadius(info.borderRadius.load()),
            offsetX(info.offsetX.load()),
            offsetY(info.offsetY.load()),
            alphaMax(info.alphaMax.load()),
            blurRadius(info.blurRadius.load()) {}

        inline WidgetInfo &operator=(const WidgetInfo &info)
        {
            widgetWidth = info.widgetWidth.load();
            widgetHeight = info.widgetHeight.load();
            borderRadius = info.borderRadius.load();
            offsetX = info.offsetX.load();
            offsetY = info.offsetY.load();
            alphaMax = info.alphaMax.load();
            blurRadius = info.blurRadius.load();

            return *this;
        }

        inline WidgetInfo &operator=(WidgetInfo &&info)
        {
            widgetWidth = info.widgetWidth.load();
            widgetHeight = info.widgetHeight.load();
            borderRadius = info.borderRadius.load();
            offsetX = info.offsetX.load();
            offsetY = info.offsetY.load();
            alphaMax = info.alphaMax.load();
            blurRadius = info.blurRadius.load();

            return *this;
        }

        inline bool operator==(const WidgetInfo &info) const {
            constexpr float epsilon = std::numeric_limits<float>::epsilon();
            return (
                std::abs(widgetWidth - info.widgetWidth) < epsilon &&
                std::abs(widgetHeight - info.widgetHeight) < epsilon &&
                std::abs(borderRadius - info.borderRadius) < epsilon &&
                std::abs(offsetX - info.offsetX) < epsilon &&
                std::abs(offsetY - info.offsetY) < epsilon &&
                std::abs(alphaMax - info.alphaMax) < epsilon &&
                std::abs(blurRadius - info.blurRadius) < epsilon
            );
        }

        inline bool operator!=(const WidgetInfo &info) const
        {
            return !operator==(info);
        }
    };

    struct WidgetBuffer
    {
        std::atomic_int handle;
        WidgetInfo lastInfo;
        WidgetInfo info;
        QPixmap pixmap;
        std::function<void ()> updateFunc;

        inline WidgetBuffer(int handle, std::function<void ()> updateFunc):
            handle(handle),
            lastInfo(WidgetInfo()),
            info(WidgetInfo()),
            pixmap(QPixmap()),
            updateFunc(updateFunc) {}

        inline WidgetBuffer(const WidgetBuffer &widgetBuffer):
            handle(widgetBuffer.handle.load()),
            lastInfo(widgetBuffer.lastInfo),
            info(widgetBuffer.info),
            pixmap(widgetBuffer.pixmap),
            updateFunc(widgetBuffer.updateFunc) {}

        inline WidgetBuffer(WidgetBuffer &&widgetBuffer):
            handle(widgetBuffer.handle.load()),
            lastInfo(widgetBuffer.lastInfo),
            info(widgetBuffer.info),
            pixmap(widgetBuffer.pixmap),
            updateFunc(widgetBuffer.updateFunc) {}

        inline WidgetBuffer &operator=(const WidgetBuffer &widgetBuffer)
        {
            handle = widgetBuffer.handle.load();
            lastInfo = widgetBuffer.lastInfo;
            info = widgetBuffer.info;
            pixmap = widgetBuffer.pixmap;
            updateFunc = widgetBuffer.updateFunc;

            return *this;
        }

        inline WidgetBuffer &operator=(WidgetBuffer &&widgetBuffer)
        {
            handle = widgetBuffer.handle.load();
            lastInfo = widgetBuffer.lastInfo;
            info = widgetBuffer.info;
            pixmap = widgetBuffer.pixmap;
            updateFunc = widgetBuffer.updateFunc;

            return *this;
        }
    };

    void initializeOpenGL();
    QImage render(WidgetInfo info);

signals:
    void stopRequested();
    void drawRequested();
    void saveImage(int handle, QImage image, std::function<void ()> updateFunc);

private slots:
    void onSaveImage(int handle, QImage image, std::function<void ()> updateFunc);

private:
    QOpenGLContext *glContext;
    QOffscreenSurface *glSurface;
    QOpenGLFunctions *glFunctions;
    QOpenGLShaderProgram *program;
    GLuint vbo;

    QReadWriteLock lock;
    std::vector<WidgetBuffer> widgetBuffers;
    std::vector<WidgetBuffer> disabledWidgetBuffers;
    std::atomic_int handleCounter = 0;

    std::atomic_bool stopped = false;
    std::atomic_bool draw = false;
};

#endif // DROP_SHADOW_RENDERER_H
