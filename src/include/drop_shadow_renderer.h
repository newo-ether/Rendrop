// drop_shadow_renderer.h

#ifndef DROPSHADOWRENDERER_H
#define DROPSHADOWRENDERER_H

#include <QPixmap>

#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

class DropShadowRenderer
{

public:
    DropShadowRenderer();
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

private:
    void initializeOpenGL();

private:
    QOpenGLContext *glContext;
    QOffscreenSurface *glSurface;
    QOpenGLFunctions *glFunctions;
    QOpenGLShaderProgram *program;
    GLuint vbo;
};

#endif // DROPSHADOWRENDERER_H
