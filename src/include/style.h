// style.h

#ifndef STYLE_H
#define STYLE_H

#include "color.h"

struct Style
{
    Color borderColor;

    Style(): borderColor(Color()) {}
    Style(const Color &borderColor): borderColor(borderColor) {}

    inline Style operator+(const Style &style)
    {
        return Style(borderColor + style.borderColor);
    }

    inline Style operator-(const Style &style)
    {
        return Style(borderColor - style.borderColor);
    }

    inline Style operator*(const Style &style)
    {
        return Style(borderColor * style.borderColor);
    }

    inline Style operator*(float value)
    {
        return Style(borderColor * value);
    }

    inline friend Style operator*(float value, const Style &style)
    {
        return Style(value * style.borderColor);
    }

    inline Style operator/(const Style &style)
    {
        return Style(borderColor / style.borderColor);
    }

    inline Style operator/(float value)
    {
        return Style(borderColor / value);
    }

    inline friend Style operator/(float value, const Style &style)
    {
        return Style(value / style.borderColor);
    }

    inline Style &operator+=(const Style &style)
    {
        borderColor += style.borderColor;

        return *this;
    }

    inline Style &operator-=(const Style &style)
    {
        borderColor -= style.borderColor;

        return *this;
    }

    inline Style &operator*=(const Style &style)
    {
        borderColor *= style.borderColor;

        return *this;
    }

    inline Style &operator*=(float value)
    {
        borderColor *= value;

        return *this;
    }

    inline Style &operator/=(const Style &style)
    {
        borderColor /= style.borderColor;

        return *this;
    }

    inline Style &operator/=(float value)
    {
        borderColor /= value;

        return *this;
    }

    inline bool operator==(const Style &style)
    {
        return borderColor == style.borderColor;
    }

    inline bool operator!=(const Style &style)
    {
        return !operator==(style);
    }

    inline float length()
    {
        return borderColor.length();
    }

    inline static Style lerp(const Style &a, const Style &b, float t)
    {
        Style result;
        result.borderColor = Color::lerp(a.borderColor, b.borderColor, t);
        return result;
    }

    inline static Style zero()
    {
        return Style(Color::zero());
    }
};

#endif // STYLE_H
