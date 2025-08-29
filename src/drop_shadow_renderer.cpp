// drop_shadow_renderer.cpp

#include <QImage>
#include <QPixmap>
#include <QObject>
#include <QThread>
#include <QReadWriteLock>
#include <QEventLoop>

#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>

#include <vector>
#include <atomic>
#include <functional>

#include "drop_shadow_renderer.h"

DropShadowRenderer::DropShadowRenderer(QObject *parent):
    QThread(parent)
{
    glContext = new QOpenGLContext();
    glContext->setFormat(QSurfaceFormat::defaultFormat());
    glContext->create();

    glSurface = new QOffscreenSurface();
    glSurface->setFormat(glContext->format());
    glSurface->create();

    glContext->moveToThread(this);
    start();
}

DropShadowRenderer::~DropShadowRenderer()
{
    stopped = true;
    emit stopRequested();
    wait();

    delete glSurface;
    delete glContext;
}

void DropShadowRenderer::run()
{
    glContext->makeCurrent(glSurface);
    initializeOpenGL();

    QEventLoop loop;
    QObject::connect(this, &DropShadowRenderer::stopRequested, &loop, &QEventLoop::quit);
    QObject::connect(this, &DropShadowRenderer::drawRequested, &loop, &QEventLoop::quit);
    QObject::connect(this, &DropShadowRenderer::saveImage, this, &DropShadowRenderer::onSaveImage, Qt::QueuedConnection);

    stopped = false;
    draw = false;

    while (!stopped)
    {
        if (draw.exchange(false))
        {
            while (true)
            {
                lock.lockForRead();

                auto iter = std::find_if(widgetBuffers.begin(), widgetBuffers.end(), [](const auto &widgetBuffer) {
                    return widgetBuffer.lastInfo != widgetBuffer.info;
                });

                if (iter == widgetBuffers.end())
                {
                    lock.unlock();
                    break;
                }

                WidgetBuffer& widgetBuffer = *iter;
                int handle = widgetBuffer.handle;
                WidgetInfo info = widgetBuffer.info;
                widgetBuffer.lastInfo = info;
                auto updateFunc = widgetBuffer.updateFunc;

                lock.unlock();

                QImage image = render(info);
                emit saveImage(handle, image, updateFunc);
            }
        }
        else
        {
            loop.exec();
        }
    }

    glContext->doneCurrent();
}

int DropShadowRenderer::createWidgetBuffer(std::function<void ()> updateFunc)
{
    lock.lockForWrite();

    int handle = handleCounter++;
    widgetBuffers.push_back(WidgetBuffer(handle, updateFunc));

    lock.unlock();

    return handle;
}

void DropShadowRenderer::deleteWidgetBuffer(int handle)
{
    lock.lockForWrite();

    auto iter = std::find_if(widgetBuffers.begin(), widgetBuffers.end(), [handle](const auto &widgetBuffer) {
        return widgetBuffer.handle == handle;
    });

    if (iter != widgetBuffers.end())
    {
        widgetBuffers.erase(iter);
        lock.unlock();
        return;
    }

    iter = std::find_if(disabledWidgetBuffers.begin(), disabledWidgetBuffers.end(), [handle](const auto &widgetBuffer) {
        return widgetBuffer.handle == handle;
    });

    if (iter != disabledWidgetBuffers.end())
    {
        disabledWidgetBuffers.erase(iter);
    }

    lock.unlock();
}

void DropShadowRenderer::setWidgetBufferUpdateEnabled(int handle, bool enabled)
{
    lock.lockForWrite();

    if (enabled)
    {
        auto iter = std::find_if(disabledWidgetBuffers.begin(), disabledWidgetBuffers.end(), [handle](const auto &widgetBuffer) {
            return widgetBuffer.handle == handle;
        });

        if (iter != disabledWidgetBuffers.end())
        {
            WidgetBuffer widgetBuffer = *iter;
            disabledWidgetBuffers.erase(iter);
            widgetBuffers.push_back(widgetBuffer);
        }
    }
    else
    {
        auto iter = std::find_if(widgetBuffers.begin(), widgetBuffers.end(), [handle](const auto &widgetBuffer) {
            return widgetBuffer.handle == handle;
        });

        if (iter != widgetBuffers.end())
        {
            WidgetBuffer widgetBuffer = *iter;
            widgetBuffers.erase(iter);
            disabledWidgetBuffers.push_back(widgetBuffer);
        }
    }

    lock.unlock();
}

void DropShadowRenderer::setWidgetBuffer(
    int handle,
    float widgetWidth,
    float widgetHeight,
    float borderRadius,
    float offsetX,
    float offsetY,
    float alphaMax,
    float blurRadius
) {
    lock.lockForRead();

    auto iter = std::find_if(widgetBuffers.begin(), widgetBuffers.end(), [handle](const auto &widgetBuffer) {
        return widgetBuffer.handle == handle;
    });

    if (iter != widgetBuffers.end())
    {
        WidgetBuffer &widgetBuffer = *iter;
        widgetBuffer.info = WidgetInfo(
            widgetWidth,
            widgetHeight,
            borderRadius,
            offsetX,
            offsetY,
            alphaMax,
            blurRadius
        );
    }

    lock.unlock();

    draw = true;
    emit drawRequested();
}

QPixmap DropShadowRenderer::getPixmap(int handle)
{
    lock.lockForRead();

    auto iter = std::find_if(widgetBuffers.begin(), widgetBuffers.end(), [handle](const auto &widgetBuffer) {
        return widgetBuffer.handle == handle;
    });

    QPixmap pixmap;

    if (iter != widgetBuffers.end())
    {
        pixmap = (*iter).pixmap;
    }

    lock.unlock();
    return pixmap;
}

void DropShadowRenderer::initializeOpenGL()
{
    glFunctions = glContext->functions();
    glFunctions->initializeOpenGLFunctions();

    const char *vertexShaderSource =
        "#version 330 core\n"
        "\n"
        "layout(location = 0) in vec2 position;\n"
        "out vec2 uv;\n"
        "\n"
        "void main() {\n"
        "    uv = position * vec2(0.5f, -0.5f);\n"
        "    gl_Position = vec4(position, 0.0f, 1.0f);\n"
        "}\n";

    const char *fragmentShaderSource =
        "#version 330 core\n"
        "\n"
        "in vec2 uv;\n"
        "out vec4 fragColor;\n"
        "\n"
        "uniform float borderRadius;\n"
        "uniform float widgetWidth;\n"
        "uniform float widgetHeight;\n"
        "uniform float rectWidth;\n"
        "uniform float rectHeight;\n"
        "uniform float offsetX;\n"
        "uniform float offsetY;\n"
        "uniform float alphaMax;\n"
        "uniform float blurRadius;\n"
        "\n"
        "float lerp(float k, float a, float b) {\n"
        "    return (1.0f - k) * a + k * b;\n"
        "}\n"
        "\n"
        "float unlerp(float x, float a, float b) {\n"
        "    return (x - a) / (b - a);\n"
        "}\n"
        "\n"
        "float sdfRoundRect(vec2 pos, float width, float height, float radius) {\n"
        "    vec2 d = abs(pos) - vec2(width/2 - radius, height/2 - radius);\n"
        "    vec2 q = max(d, vec2(0.0f));\n"
        "    return length(q) - radius + min(max(q.x, q.y), 0.0f);\n"
        "}\n"
        "\n"
        "void main() {\n"
        "    vec2 p = uv * vec2(widgetWidth, widgetHeight) - vec2(offsetX, offsetY);\n"
        "    float dist = sdfRoundRect(p, rectWidth, rectHeight, borderRadius);\n"
        "    float alpha = lerp(unlerp(dist, -blurRadius * 0.5f, blurRadius * 0.5f), alphaMax, 0.0f);\n"
        "    fragColor = vec4(vec3(0.0f), alpha);\n"
        "};\n";

    program = new QOpenGLShaderProgram();
    program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    program->link();

    static const float vertices[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f,  1.0f,
        1.0f,  1.0f,
    };

    glFunctions->glGenBuffers(1, &vbo);
    glFunctions->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glFunctions->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
}

QImage DropShadowRenderer::render(WidgetInfo info)
{
    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    fboFormat.setInternalTextureFormat(GL_RGBA8);

    QOpenGLFramebufferObject fbo(info.widgetWidth, info.widgetHeight, fboFormat);
    fbo.bind();

    glFunctions->glViewport(0, 0, info.widgetWidth, info.widgetHeight);
    glFunctions->glClearColor(0, 0, 0, 0);
    glFunctions->glClear(GL_COLOR_BUFFER_BIT);

    program->bind();

    float marginX = std::abs(info.offsetX) + info.blurRadius * 0.5f;
    float marginY = std::abs(info.offsetY) + info.blurRadius * 0.5f;

    program->setUniformValue("borderRadius", info.borderRadius);
    program->setUniformValue("widgetWidth", static_cast<float>(info.widgetWidth));
    program->setUniformValue("widgetHeight", static_cast<float>(info.widgetHeight));
    program->setUniformValue("rectWidth", static_cast<float>(info.widgetWidth - marginX * 2));
    program->setUniformValue("rectHeight", static_cast<float>(info.widgetHeight - marginY * 2));
    program->setUniformValue("offsetX", info.offsetX);
    program->setUniformValue("offsetY", info.offsetY);
    program->setUniformValue("alphaMax", info.alphaMax);
    program->setUniformValue("blurRadius", info.blurRadius);

    glFunctions->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glFunctions->glEnableVertexAttribArray(0);
    glFunctions->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glFunctions->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glFunctions->glDisableVertexAttribArray(0);
    program->release();

    fbo.release();

    return fbo.toImage();
}

void DropShadowRenderer::onSaveImage(int handle, QImage image, std::function<void ()> updateFunc)
{
    lock.lockForRead();

    auto iter = std::find_if(widgetBuffers.begin(), widgetBuffers.end(), [handle](const auto &widgetBuffer) {
        return widgetBuffer.handle == handle;
    });

    if (iter != widgetBuffers.end())
    {
        QPixmap pixmap = QPixmap::fromImage(image);
        (*iter).pixmap = pixmap;
        updateFunc();
    }

    lock.unlock();
}
