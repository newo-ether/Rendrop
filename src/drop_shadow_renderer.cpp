// drop_shadow_renderer.cpp

#include <QImage>

#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>

#include "drop_shadow_renderer.h"

DropShadowRenderer::DropShadowRenderer(bool enabled):
    enabled(enabled)
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
    glContext = new QOpenGLContext();
    glContext->setFormat(QSurfaceFormat::defaultFormat());
    glContext->create();

    glSurface = new QOffscreenSurface();
    glSurface->setFormat(glContext->format());
    glSurface->create();

    glContext->makeCurrent(glSurface);

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

QPixmap DropShadowRenderer::render(
    float widgetWidth,
    float widgetHeight,
    float borderRadius,
    float offsetX,
    float offsetY,
    float alphaMax,
    float blurRadius
) {
    if (!enabled)
    {
        return QPixmap();
    }

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

void DropShadowRenderer::setRendererEnabled(bool enabled)
{
    this->enabled = enabled;
}
