#include "../widgets.h"
#include "../../colors/colors.h"
#include <imgui/imgui_internal.h>

namespace
{
    constexpr float kBorderThickness = 1.0f;

    inline ImRect round_rect(const ImRect& rect)
    {
        return ImRect(
            ImVec2(IM_ROUND(rect.Min.x), IM_ROUND(rect.Min.y)),
            ImVec2(IM_ROUND(rect.Max.x), IM_ROUND(rect.Max.y))
        );
    }

    inline ImRect shrink_rect(const ImRect& rect, float amount)
    {
        return ImRect(
            ImVec2(rect.Min.x + amount, rect.Min.y + amount),
            ImVec2(rect.Max.x - amount, rect.Max.y - amount)
        );
    }

    inline ImVec4 scale_color(const ImVec4& color, float factor)
    {
        ImVec4 result = color;
        result.x = ImClamp(result.x * factor, 0.0f, 1.0f);
        result.y = ImClamp(result.y * factor, 0.0f, 1.0f);
        result.z = ImClamp(result.z * factor, 0.0f, 1.0f);
        return result;
    }

    inline void draw_gradient(ImDrawList* draw_list, const ImRect& rect, const ImVec4& top_col, const ImVec4& bottom_col)
    {
        draw_list->AddRectFilledMultiColor(
            rect.Min,
            rect.Max,
            ImGui::GetColorU32(bottom_col),
            ImGui::GetColorU32(bottom_col),
            ImGui::GetColorU32(top_col),
            ImGui::GetColorU32(top_col)
        );
    }
}

bool c_widgets::button(const char* label, const ImVec2& size_arg)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!window || window->SkipItems)
        return false;

    ImVec2 size = size_arg;
    if (size.x <= 0.0f)
        size.x = ImGui::GetContentRegionAvail().x;
    if (size.y <= 0.0f)
        size.y = ImGui::GetFrameHeight();

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImGui::ItemSize(bb);
    const ImGuiID id = window->GetID(label);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered = false;
    bool held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_None);

    ImDrawList* draw_list = window->DrawList;
    ImRect outer = bb;
    ImRect inner = shrink_rect(outer, kBorderThickness);
    ImRect fill = shrink_rect(inner, 1.0f);
    ImRect draw_outer = round_rect(outer);
    ImRect draw_inner = round_rect(inner);
    ImRect draw_fill = round_rect(fill);

    const ImDrawListFlags old_flags = draw_list->Flags;
    draw_list->Flags |= ImDrawListFlags_AntiAliasedLines | ImDrawListFlags_AntiAliasedFill;

    auto adjust_color = [&](const ImVec4& col) -> ImVec4
    {
        if (held)
            return scale_color(col, 0.9f);
        if (hovered)
            return scale_color(col, 1.08f);
        return col;
    };

    ImVec4 base_top = adjust_color(c_colors::top_child_background);
    ImVec4 base_bottom = adjust_color(c_colors::bottom_child_background);

    draw_list->AddRect(draw_outer.Min, draw_outer.Max, ImGui::GetColorU32(c_colors::outter_border), 0.0f, 0, kBorderThickness);
    draw_list->AddRect(draw_inner.Min, draw_inner.Max, ImGui::GetColorU32(c_colors::main_border), 0.0f, 0, kBorderThickness);
    draw_gradient(draw_list, draw_fill, base_top, base_bottom);

    const char* text_begin = label ? label : "";
    const char* text_end = ImGui::FindRenderedTextEnd(text_begin);
    if (text_begin != text_end)
    {
        const ImVec2 text_size = ImGui::CalcTextSize(text_begin, text_end);
        const ImVec2 text_pos(
            draw_fill.Min.x + (draw_fill.GetWidth() - text_size.x) * 0.5f,
            draw_fill.Min.y + (draw_fill.GetHeight() - text_size.y) * 0.5f
        );

        ImVec4 base_text = ImGui::GetStyle().Colors[ImGuiCol_Text];
        ImVec4 text_col = scale_color(base_text, hovered ? 1.05f : 0.7f);
        if (held)
            text_col = ImVec4(1.0f, 1.0f, 1.0f, base_text.w);
        draw_list->AddText(text_pos, ImGui::GetColorU32(text_col), text_begin, text_end);
    }

    draw_list->Flags = old_flags;

    return pressed;
}
