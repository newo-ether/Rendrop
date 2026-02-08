// color.h

#ifndef COLOR_H
#define COLOR_H

#include <QString>
#include <QColor>

#include <algorithm>

struct Color
{
    float r;
    float g;
    float b;
    float a;

    Color(): r(0), g(0), b(0), a(0) {}

    Color(float r, float g, float b, float a = 1.0f):
        r(r), g(g), b(b), a(a) {}

    Color(int r, int g, int b, int a = 255):
        r(std::clamp(r / 255.0f, 0.0f, 1.0f)),
        g(std::clamp(g / 255.0f, 0.0f, 1.0f)),
        b(std::clamp(b / 255.0f, 0.0f, 1.0f)),
        a(std::clamp(a / 255.0f, 0.0f, 1.0f)) {}

    inline QColor toQColor() const
    {
        return QColor::fromRgbF(
            std::clamp(r, 0.0f, 1.0f),
            std::clamp(g, 0.0f, 1.0f),
            std::clamp(b, 0.0f, 1.0f),
            std::clamp(a, 0.0f, 1.0f)
        );
    }

    inline Color operator+(const Color &color)
    {
        return Color(r + color.r, g + color.g, b + color.b, a + color.a);
    }

    inline Color operator-(const Color &color)
    {
        return Color(r - color.r, g - color.g, b - color.b, a - color.a);
    }

    inline Color operator*(const Color &color)
    {
        return Color(r * color.r, g * color.g, b * color.b, a * color.a);
    }

    inline Color operator*(float value)
    {
        return Color(r * value, g * value, b * value, a * value);
    }

    inline friend Color operator*(float value, const Color &color)
    {
        return Color(value * color.r, value * color.g, value * color.b, value * color.a);
    }

    inline Color operator/(const Color &color)
    {
        return Color(r / color.r, g / color.g, b / color.b, a / color.a);
    }

    inline Color operator/(float value)
    {
        return Color(r / value, g / value, b / value, a / value);
    }

    inline friend Color operator/(float value, const Color &color)
    {
        return Color(value / color.r, value / color.g, value / color.b, value / color.a);
    }

    inline Color &operator+=(const Color &color)
    {
        r += color.r;
        g += color.g;
        b += color.b;
        a += color.a;

        return *this;
    }

    inline Color &operator-=(const Color &color)
    {
        r -= color.r;
        g -= color.g;
        b -= color.b;
        a -= color.a;

        return *this;
    }

    inline Color &operator*=(const Color &color)
    {
        r *= color.r;
        g *= color.g;
        b *= color.b;
        a *= color.a;

        return *this;
    }

    inline Color &operator*=(float value)
    {
        r *= value;
        g *= value;
        b *= value;
        a *= value;

        return *this;
    }

    inline Color &operator/=(const Color &color)
    {
        r /= color.r;
        g /= color.g;
        b /= color.b;
        a /= color.a;

        return *this;
    }

    inline Color &operator/=(float value)
    {
        r /= value;
        g /= value;
        b /= value;
        a /= value;

        return *this;
    }

    inline bool operator==(const Color &color)
    {
        return std::abs(r - color.r) < 1.0f / 255.0f &&
               std::abs(g - color.g) < 1.0f / 255.0f &&
               std::abs(b - color.b) < 1.0f / 255.0f;
    }

    inline bool operator!=(const Color &color)
    {
        return !operator==(color);
    }

    inline float length()
    {
        return std::sqrtf(r * r + g * g + b * b + a * a);
    }

    inline QString toText()
    {
        Color color = clamp(*this);

        return "rgba("
               + QString::number(static_cast<int>(color.r * 255.0f)) + ", "
               + QString::number(static_cast<int>(color.g * 255.0f)) + ", "
               + QString::number(static_cast<int>(color.b * 255.0f)) + ", "
               + QString::number(static_cast<int>(color.a * 255.0f))
               + ")";
    }

    inline static Color lerp(const Color &a, const Color &b, float t)
    {
        return Color(
            a.r * (1.0f - t) + b.r * t,
            a.g * (1.0f - t) + b.g * t,
            a.b * (1.0f - t) + b.b * t,
            a.a * (1.0f - t) + b.a * t
        );
    }

    inline static Color clamp(const Color color)
    {
        return Color(
            std::clamp(color.r, 0.0f, 1.0f),
            std::clamp(color.g, 0.0f, 1.0f),
            std::clamp(color.b, 0.0f, 1.0f),
            std::clamp(color.a, 0.0f, 1.0f)
        );
    }

    inline static Color zero()
    {
        return Color(0.0f, 0.0f, 0.0f, 0.0f);
    }
};

#endif // COLOR_H
