// drop_shadow_renderer.cpp

#include <QImage>

#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>

#include "drop_shadow_renderer.h"

DropShadowRenderer::DropShadowRenderer()
{
    initializeOpenGL();
}

DropShadowRenderer::~DropShadowRenderer()
{
    glContext->doneCurrent();
    delete glSurface;
    delete glContext;
}

void DropShadowRenderer::initializeOpenGL()
{
    glContext = new QOpenGLContext;
    glContext->setFormat(QSurfaceFormat::defaultFormat());
    glContext->create();

    glSurface = new QOffscreenSurface;
    glSurface->setFormat(glContext->format());
    glSurface->create();

    glContext->makeCurrent(glSurface);

    glFunctions = glContext->functions();
    glFunctions->initializeOpenGLFunctions();

    const char *vertexShaderSource = R"(
        #version 330 core

        layout(location = 0) in vec2 position;
        out vec2 uv;

        void main() {
            uv = position * vec2(0.5f, -0.5f);
            gl_Position = vec4(position, 0.0f, 1.0f);
        }
    )";

    const char *fragmentShaderSource = R"(
        #version 330 core

        in vec2 uv;
        out vec4 fragColor;

        uniform float borderRadius;
        uniform float widgetWidth;
        uniform float widgetHeight;
        uniform float rectWidth;
        uniform float rectHeight;
        uniform float offsetX;
        uniform float offsetY;
        uniform float alphaMax;
        uniform float blurRadius;

        float lerp(float k, float a, float b) {
            return (1.0f - k) * a + k * b;
        }

        float unlerp(float x, float a, float b) {
            return (x - a) / (b - a);
        }

        float sdfRoundRect(vec2 pos, float width, float height, float radius) {
            vec2 d = abs(pos) - vec2(width/2 - radius, height/2 - radius);
            vec2 q = max(d, vec2(0.0f));
            return length(q) - radius + min(max(q.x, q.y), 0.0f);
        }

        void main() {
            vec2 p = uv * vec2(widgetWidth, widgetHeight) - vec2(offsetX, offsetY);
            float dist = sdfRoundRect(p, rectWidth, rectHeight, borderRadius);
            float alpha = lerp(unlerp(dist, -blurRadius * 0.5f, blurRadius * 0.5f), alphaMax, 0.0f);
            fragColor = vec4(vec3(0.0f), alpha);
        }
    )";

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

QPixmap DropShadowRenderer::render(
    float widgetWidth,
    float widgetHeight,
    float borderRadius,
    float offsetX,
    float offsetY,
    float alphaMax,
    float blurRadius
) {
    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    fboFormat.setInternalTextureFormat(GL_RGBA8);

    QOpenGLFramebufferObject fbo(widgetWidth, widgetHeight, fboFormat);
    fbo.bind();

    glFunctions->glViewport(0, 0, widgetWidth, widgetHeight);
    glFunctions->glClearColor(0, 0, 0, 0);
    glFunctions->glClear(GL_COLOR_BUFFER_BIT);

    program->bind();

    float marginX = std::abs(offsetX) + blurRadius * 0.5f;
    float marginY = std::abs(offsetY) + blurRadius * 0.5f;

    program->setUniformValue("borderRadius", borderRadius);
    program->setUniformValue("widgetWidth", static_cast<float>(widgetWidth));
    program->setUniformValue("widgetHeight", static_cast<float>(widgetHeight));
    program->setUniformValue("rectWidth", static_cast<float>(widgetWidth - marginX * 2));
    program->setUniformValue("rectHeight", static_cast<float>(widgetHeight - marginY * 2));
    program->setUniformValue("offsetX", offsetX);
    program->setUniformValue("offsetY", offsetY);
    program->setUniformValue("alphaMax", alphaMax);
    program->setUniformValue("blurRadius", blurRadius);

    glFunctions->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glFunctions->glEnableVertexAttribArray(0);
    glFunctions->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glFunctions->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glFunctions->glDisableVertexAttribArray(0);
    program->release();

    fbo.release();

    QImage image = fbo.toImage();
    QPixmap pixmap = QPixmap::fromImage(image);

    return pixmap;
}
