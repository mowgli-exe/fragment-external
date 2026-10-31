#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <imgui/imgui.h>

namespace c_colors
{
    inline float clamp01(float value)
    {
        return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    }

    inline ImVec4 scale_color(const ImVec4& color, float factor)
    {
        ImVec4 result = color;
        result.x = clamp01(result.x * factor);
        result.y = clamp01(result.y * factor);
        result.z = clamp01(result.z * factor);
        return result;
    }

    inline ImVec4 derive_bottom_accent(const ImVec4& top_color)
    {
        const ImVec4 accent_delta(-0.15f, -0.08f, -0.15f, 0.0f);
        ImVec4 bottom_color;
        bottom_color.x = clamp01(top_color.x + accent_delta.x);
        bottom_color.y = clamp01(top_color.y + accent_delta.y);
        bottom_color.z = clamp01(top_color.z + accent_delta.z);
        bottom_color.w = top_color.w;
        return bottom_color;
    }

    inline ImVec4 top_window_background    = ImVec4(0.082f, 0.082f, 0.082f, 1.0f);
    inline ImVec4 bottom_window_background = ImVec4(0.122f, 0.122f, 0.122f, 1.0f);

    inline ImVec4 top_child_background    = ImVec4(0.122f, 0.122f, 0.122f, 1.0f);
    inline ImVec4 bottom_child_background = ImVec4(0.086f, 0.086f, 0.086f, 1.0f);

    inline ImVec4 outter_border           = ImVec4(0.016f, 0.016f, 0.016f, 1.0f);
    inline ImVec4 main_border             = ImVec4(0.149f, 0.149f, 0.149f, 1.0f);

    inline ImVec4 top_accent_color        = ImVec4(0.196f, 0.47f, 0.784f, 1.0f);
    inline ImVec4 bottom_accent_color     = derive_bottom_accent(top_accent_color);

    inline ImVec4 white                   = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
    inline ImVec4 black                   = ImVec4(0.000f, 0.000f, 0.000f, 1.000f);
}
