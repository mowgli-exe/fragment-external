#pragma once

#include <filesystem>
#include <string>

struct ImVec2;

namespace lua_vm
{
    bool initialize();
    void shutdown();
    bool is_ready();

    bool execute_string(const std::string& script, const std::string& chunk_name = "lua_script");
    bool execute_file(const std::filesystem::path& file_path);

    void on_frame(float delta_time);

    void render_editor_window(bool menu_has_frame, const ImVec2& menu_pos, const ImVec2& menu_size);
    void render_console_window(bool menu_has_frame, const ImVec2& menu_pos, const ImVec2& menu_size);
}
