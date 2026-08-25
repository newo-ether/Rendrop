// drop_shadow_renderer.cpp

#include <QImage>
#include <QPixmap>
#include <QObject>
#include <QThread>
#include <QReadWriteLock>
#include <QMutexLocker>

#include <vector>
#include <atomic>
#include <functional>
#include <algorithm>
#include <cmath>

#include "drop_shadow_renderer.h"

DropShadowRenderer::DropShadowRenderer(QObject *parent):
    QThread(parent)
{
    QObject::connect(
        this,
        &DropShadowRenderer::saveImage,
        this,
        &DropShadowRenderer::onSaveImage,
        Qt::QueuedConnection
    );
    start();
}

DropShadowRenderer::~DropShadowRenderer()
{
    {
        QMutexLocker locker(&wakeMutex);
        stopped = true;
    }
    wakeCondition.wakeAll();
    wait();
}

void DropShadowRenderer::run()
{
    while (true)
    {
        {
            QMutexLocker locker(&wakeMutex);
            while (!stopped && !draw)
            {
                wakeCondition.wait(&wakeMutex);
            }

            if (stopped)
            {
                break;
            }

            draw = false;
        }

        while (!stopped)
        {
            int handle = -1;
            WidgetInfo info;

            {
                QWriteLocker locker(&lock);
                auto iter = std::find_if(widgetBuffers.begin(), widgetBuffers.end(), [](const auto &widgetBuffer) {
                    return widgetBuffer.lastInfo != widgetBuffer.info;
                });

                if (iter == widgetBuffers.end())
                {
                    break;
                }

                handle = iter->handle;
                info = iter->info;
                iter->lastInfo = info;
            }

            emit saveImage(handle, render(info));
        }
    }
}

void DropShadowRenderer::requestDraw()
{
    {
        QMutexLocker locker(&wakeMutex);
        draw = true;
    }
    wakeCondition.wakeOne();
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
    bool needsDraw = false;

    {
        QWriteLocker locker(&lock);

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
                needsDraw = widgetBuffer.lastInfo != widgetBuffer.info;
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
    }

    if (needsDraw)
    {
        requestDraw();
    }
}

void DropShadowRenderer::setWidgetBuffer(
    int handle,
    float widgetWidth,
    float widgetHeight,
    float borderRadius,
    float offsetX,
    float offsetY,
    float alphaMax,
    float blurRadius,
    float devicePixelRatio
) {
    bool updated = false;

    {
        QWriteLocker locker(&lock);
        auto iter = std::find_if(widgetBuffers.begin(), widgetBuffers.end(), [handle](const auto &widgetBuffer) {
            return widgetBuffer.handle == handle;
        });

        if (iter != widgetBuffers.end())
        {
            iter->info = WidgetInfo(
                widgetWidth,
                widgetHeight,
                borderRadius,
                offsetX,
                offsetY,
                alphaMax,
                blurRadius,
                devicePixelRatio
            );
            updated = true;
        }
    }

    if (updated)
    {
        requestDraw();
    }
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

QImage DropShadowRenderer::render(WidgetInfo info)
{
    const float dpr = std::max(0.1f, info.devicePixelRatio.load());
    const int width = std::max(1, static_cast<int>(std::ceil(info.widgetWidth.load() * dpr)));
    const int height = std::max(1, static_cast<int>(std::ceil(info.widgetHeight.load() * dpr)));

    const float blurRadius = std::max(0.001f, info.blurRadius.load() * dpr);
    const float offsetX = info.offsetX.load() * dpr;
    const float offsetY = info.offsetY.load() * dpr;
    const float marginX = std::abs(offsetX) + blurRadius * 0.5f;
    const float marginY = std::abs(offsetY) + blurRadius * 0.5f;
    const float halfRectWidth = std::max(0.0f, (static_cast<float>(width) - marginX * 2.0f) * 0.5f);
    const float halfRectHeight = std::max(0.0f, (static_cast<float>(height) - marginY * 2.0f) * 0.5f);
    const float radius = std::clamp(
        info.borderRadius.load() * dpr,
        0.0f,
        std::min(halfRectWidth, halfRectHeight)
    );
    const float alphaMax = std::clamp(info.alphaMax.load(), 0.0f, 1.0f);
    const float halfBlur = blurRadius * 0.5f;

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < height; ++y)
    {
        QRgb *scanLine = reinterpret_cast<QRgb *>(image.scanLine(y));
        const float py = static_cast<float>(y) + 0.5f - static_cast<float>(height) * 0.5f - offsetY;

        for (int x = 0; x < width; ++x)
        {
            const float px = static_cast<float>(x) + 0.5f - static_cast<float>(width) * 0.5f - offsetX;
            const float dx = std::abs(px) - (halfRectWidth - radius);
            const float dy = std::abs(py) - (halfRectHeight - radius);
            const float outsideX = std::max(dx, 0.0f);
            const float outsideY = std::max(dy, 0.0f);
            const float distance =
                std::sqrt(outsideX * outsideX + outsideY * outsideY)
                + std::min(std::max(dx, dy), 0.0f)
                - radius;

            float alpha = 0.0f;
            if (distance <= -halfBlur)
            {
                alpha = alphaMax;
            }
            else if (distance < halfBlur)
            {
                const float factor = std::clamp((distance + halfBlur) / blurRadius, 0.0f, 1.0f);
                alpha = (1.0f - std::pow(factor, 0.3f)) * alphaMax;
            }

            scanLine[x] = qRgba(0, 0, 0, static_cast<int>(std::lround(alpha * 255.0f)));
        }
    }

    return image;
}

void DropShadowRenderer::onSaveImage(int handle, QImage image)
{
    std::function<void ()> updateFunc;

    {
        QWriteLocker locker(&lock);
        auto iter = std::find_if(widgetBuffers.begin(), widgetBuffers.end(), [handle](const auto &widgetBuffer) {
            return widgetBuffer.handle == handle;
        });

        if (iter == widgetBuffers.end())
        {
            return;
        }

        const float dpr = iter->info.devicePixelRatio.load();
        QPixmap pixmap = QPixmap::fromImage(image);
        pixmap.setDevicePixelRatio(dpr);
        iter->pixmap = pixmap;
        updateFunc = iter->updateFunc;
    }

    if (updateFunc)
    {
        updateFunc();
    }
}
