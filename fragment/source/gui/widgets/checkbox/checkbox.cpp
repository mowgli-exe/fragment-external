#include "../widgets.h"
#include "../../colors/colors.h"
#include <imgui/imgui_internal.h>

namespace
{
    struct checkbox_spacing_state
    {
        ImGuiID last_window_id = 0;
        int     last_frame = -1;
        bool    last_was_checkbox = false;
        float   last_bottom = 0.0f;
    };

    inline checkbox_spacing_state& get_spacing_state()
    {
        static checkbox_spacing_state state;
        return state;
    }

    inline ImVec4 scale_color(const ImVec4& col, float factor)
    {
        ImVec4 result = col;
        result.x = ImClamp(result.x * factor, 0.0f, 1.0f);
        result.y = ImClamp(result.y * factor, 0.0f, 1.0f);
        result.z = ImClamp(result.z * factor, 0.0f, 1.0f);
        return result;
    }

    inline ImGuiID checkbox_anim_key(ImGuiID base_id)
    {
        static const ImGuiID seed = ImHashStr("c_widgets::checkbox_anim");
        return ImHashData(&base_id, sizeof(base_id), seed);
    }

    inline ImGuiID checkbox_state_key(ImGuiID base_id)
    {
        static const ImGuiID seed = ImHashStr("c_widgets::checkbox_state");
        return ImHashData(&base_id, sizeof(base_id), seed);
    }

    inline float ease_out_expo(float t)
    {
        if (t <= 0.0f)
            return 0.0f;
        if (t >= 1.0f)
            return 1.0f;
        return 1.0f - ImPow(2.0f, -10.0f * t);
    }

}

bool c_widgets::checkbox(const char* label, bool* v)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImVec2 original_item_spacing = g.Style.ItemSpacing;
    constexpr float spacing_ratio = 0.7f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(original_item_spacing.x, original_item_spacing.y * spacing_ratio));
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

    checkbox_spacing_state& spacing = get_spacing_state();
    const float square_sz = ImGui::GetFrameHeight() * 0.825f;
    const bool last_item_was_checkbox =
        spacing.last_was_checkbox &&
        spacing.last_frame == g.FrameCount &&
        spacing.last_window_id == window->ID;
    const bool can_stack = !window->DC.IsSameLine;
    if (can_stack && last_item_was_checkbox)
    {
        const float desired_spacing = original_item_spacing.y * spacing_ratio + 0.5f;
        const float current_gap = ImGui::GetCursorScreenPos().y - spacing.last_bottom;
        const float max_correction = square_sz;
        if (current_gap > desired_spacing + 0.1f)
        {
            const float delta = current_gap - desired_spacing;
            if (delta < max_correction)
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - delta);
        }
        else if (current_gap + 0.1f < desired_spacing)
        {
            ImGui::Dummy(ImVec2(0.0f, desired_spacing - current_gap));
        }
    }

    ImVec2 pos = window->DC.CursorPos;
    float total_width = square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f);
    const float total_height = ImMax(label_size.y, square_sz);
    ImRect total_bb(pos, pos + ImVec2(total_width, total_height));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    ImGui::SetNextItemAllowOverlap();
    if (!ImGui::ItemAdd(total_bb, id))
    {
        ImGui::PopStyleVar();
        spacing.last_was_checkbox = false;
        return false;
    }

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
    {
        *v = !*v;
        ImGui::MarkItemEdited(id);
    }

    ImRect check_bb(pos, pos + ImVec2(square_sz, square_sz));
    ImDrawList* draw_list = window->DrawList;

    const float hover_lighten = hovered ? 1.25f : 1.0f;
    const ImU32 outer_col = ImGui::GetColorU32(hovered ? scale_color(c_colors::outter_border, hover_lighten) : c_colors::outter_border);
    const ImU32 inner_col = ImGui::GetColorU32(hovered ? scale_color(c_colors::main_border, hover_lighten) : c_colors::main_border);
    const float rounding = 0.0f;

    const bool prevAA = (draw_list->Flags & ImDrawListFlags_AntiAliasedLines) != 0;
    draw_list->Flags &= ~ImDrawListFlags_AntiAliasedLines;

    ImRect outer = check_bb;
    outer.Min.x = IM_ROUND(outer.Min.x);
    outer.Min.y = IM_ROUND(outer.Min.y);
    outer.Max.x = IM_ROUND(outer.Max.x);
    outer.Max.y = IM_ROUND(outer.Max.y);
    draw_list->AddRect(outer.Min, outer.Max, outer_col, 0.0f, ImDrawFlags_None, 1.0f);

    ImRect inner(outer.Min + ImVec2(1.0f, 1.0f),
        outer.Max - ImVec2(1.0f, 1.0f));
    draw_list->AddRect(inner.Min, inner.Max, inner_col, 0.0f, ImDrawFlags_None, 1.0f);

    if (prevAA)
        draw_list->Flags |= ImDrawListFlags_AntiAliasedLines;

    ImVec2 fill_min = inner.Min + ImVec2(1.0f, 1.0f);
    ImVec2 fill_max = inner.Max - ImVec2(1.0f, 1.0f);
    const bool is_active = *v;
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID anim_key = checkbox_anim_key(id);
    const ImGuiID state_key = checkbox_state_key(id);

    float active_t = storage->GetFloat(anim_key, 1.0f);
    bool last_state = storage->GetBool(state_key, is_active);
    if (last_state != is_active)
        active_t = 0.0f;
    storage->SetBool(state_key, is_active);

    const float active_speed = 4.0f;
    active_t = ImMin(active_t + g.IO.DeltaTime * active_speed, 1.0f);
    storage->SetFloat(anim_key, active_t);
    const float active_progress = is_active ? ease_out_expo(active_t) : (1.0f - ease_out_expo(active_t));

    float eased_progress = active_progress;

    ImVec4 fill_top_col = ImLerp(c_colors::top_window_background, c_colors::top_accent_color, eased_progress);
    ImVec4 fill_bottom_col = ImLerp(c_colors::bottom_window_background, c_colors::bottom_accent_color, eased_progress);
    const bool brighten_fill = hovered && !is_active;
    ImU32 fill_top = ImGui::GetColorU32(brighten_fill ? scale_color(fill_top_col, hover_lighten) : fill_top_col);
    ImU32 fill_bottom = ImGui::GetColorU32(brighten_fill ? scale_color(fill_bottom_col, hover_lighten) : fill_bottom_col);

    draw_list->AddRectFilledMultiColor(fill_min, fill_max, fill_top, fill_top, fill_bottom, fill_bottom);

    if (label_size.x > 0.0f)
    {
        ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + (square_sz - label_size.y) * 0.5f);
        const ImVec4 base_text_col = style.Colors[ImGuiCol_Text];
        ImVec4 text_col;
        if (is_active)
        {
            text_col = ImVec4(1.0f, 1.0f, 1.0f, base_text_col.w);
        }
        else if (hovered)
        {
            text_col = scale_color(base_text_col, 1.0f);
        }
        else
        {
            text_col = scale_color(base_text_col, 0.7f);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, text_col);
        ImGui::RenderText(label_pos, label);
        ImGui::PopStyleColor();
    }

    spacing.last_frame = g.FrameCount;
    spacing.last_window_id = window->ID;
    spacing.last_was_checkbox = true;
    spacing.last_bottom = total_bb.Max.y;

    ImGui::PopStyleVar();
    return pressed;
}
