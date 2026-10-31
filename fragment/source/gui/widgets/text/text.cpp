#include "../widgets.h"
#include "../../colors/colors.h"
#include <imgui/imgui_internal.h>
#include <cstdarg>

namespace
{
    void render_text(const ImVec4& color, const char* fmt, va_list args)
    {
        if (!fmt)
            return;

        char buffer[1024];
        ImFormatStringV(buffer, IM_ARRAYSIZE(buffer), fmt, args);

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(buffer);
        ImGui::PopStyleColor();
    }
}

namespace c_widgets
{
    void text(const char* fmt, ...)
    {
        if (!fmt)
            return;

        va_list args;
        va_start(args, fmt);
        render_text(ImGui::GetStyle().Colors[ImGuiCol_Text], fmt, args);
        va_end(args);
    }

    void text_accent(const char* fmt, ...)
    {
        if (!fmt)
            return;

        va_list args;
        va_start(args, fmt);
        render_text(c_colors::top_accent_color, fmt, args);
        va_end(args);
    }

    void text_colored(const ImVec4& color, const char* fmt, ...)
    {
        if (!fmt)
            return;

        va_list args;
        va_start(args, fmt);
        render_text(color, fmt, args);
        va_end(args);
    }
}
