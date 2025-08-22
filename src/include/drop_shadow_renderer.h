// drop_shadow_renderer.h

#ifndef DROP_SHADOW_RENDERER_H
#define DROP_SHADOW_RENDERER_H

#include <QPixmap>

#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

class DropShadowRenderer
{
public:
    DropShadowRenderer(bool enabled = true);
    ~DropShadowRenderer();

    QPixmap render(
        float widgetWidth,
        float widgetHeight,
        float borderRadius,
        float offsetX,
        float offsetY,
        float alphaMax,
        float blurRadius
    );

    void setRendererEnabled(bool enabled);

private:
    void initializeOpenGL();

private:
    QOpenGLContext *glContext;
    QOffscreenSurface *glSurface;
    QOpenGLFunctions *glFunctions;
    QOpenGLShaderProgram *program;
    GLuint vbo;
    bool enabled;
};

#endif // DROP_SHADOW_RENDERER_H
