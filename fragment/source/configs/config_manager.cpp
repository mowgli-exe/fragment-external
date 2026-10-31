#include "configs/config_manager.h"

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <system_error>
#include <sstream>
#include <Windows.h>
#include <shellapi.h>
#include <vector>
#include <string>

#include "globals/globals.h"
#include "features/lighting.h"

#pragma comment(lib, "shell32.lib")

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace config_manager
{
    namespace fs = std::filesystem;

    namespace
    {
        bool initialized = false;
        std::vector<std::string> cached_configs{};
        std::string last_status{};
        bool last_status_error = false;

        void set_status(bool success, const std::string& message)
        {
            last_status = message;
            last_status_error = !success;
        }

        std::string trim(const std::string& value)
        {
            const size_t start = value.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
                return std::string();

            const size_t end = value.find_last_not_of(" \t\r\n");
            return value.substr(start, end - start + 1);
        }

        std::string sanitize_string(const std::string& input)
        {
            const std::string trimmed = trim(input);
            std::string sanitized;
            sanitized.reserve(trimmed.size());

            for (char c : trimmed)
            {
                const unsigned char u = static_cast<unsigned char>(c);
                if (std::isalnum(u) || c == '_' || c == '-' || c == ' ')
                    sanitized.push_back(c == ' ' ? '_' : c);
            }

            while (!sanitized.empty() && (sanitized.front() == '_' || sanitized.front() == '-'))
                sanitized.erase(sanitized.begin());

            while (!sanitized.empty() && (sanitized.back() == '_' || sanitized.back() == '-'))
                sanitized.pop_back();

            return sanitized;
        }

        fs::path configs_directory()
        {
            static fs::path cached_dir;
            static bool has_cached_dir = false;

            if (!has_cached_dir)
            {
                char module_path[MAX_PATH]{};
                DWORD length = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
                if (length != 0)
                    cached_dir = fs::path(module_path).parent_path() / "configs";
                else
                    cached_dir = fs::current_path() / "configs";

                has_cached_dir = true;
            }

            return cached_dir;
        }

        bool ensure_directory()
        {
            std::error_code ec;
            const fs::path dir = configs_directory();

            if (!fs::exists(dir, ec))
            {
                fs::create_directories(dir, ec);
                if (ec)
                    return false;
            }
            else if (!fs::is_directory(dir, ec))
            {
                return false;
            }

            DWORD attributes = GetFileAttributesW(dir.wstring().c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_HIDDEN))
            {
                DWORD new_attributes = attributes | FILE_ATTRIBUTE_HIDDEN;
                SetFileAttributesW(dir.wstring().c_str(), new_attributes);
            }

            return true;
        }

        fs::path get_config_path(const std::string& raw_name, std::string& sanitized_out)
        {
            sanitized_out = sanitize_string(raw_name);
            if (sanitized_out.empty())
                return fs::path();

            return configs_directory() / (sanitized_out + ".cfg");
        }

        void reset_keybind_state(c_keybind& bind)
        {
            bind.awaiting_release = false;
            bind.was_down = false;
            bind.enabled = false;
            bind.waiting_for_input = false;
            bind.menu_opened = false;
        }

        void write_current_configuration(std::ostream& out)
        {
            auto write_bool = [&](const char* key, bool value) {
                out << key << '=' << (value ? 1 : 0) << '\n';
            };

            auto write_int = [&](const char* key, int value) {
                out << key << '=' << value << '\n';
            };

            auto write_float = [&](const char* key, float value) {
                out << key << '=' << value << '\n';
            };

        auto write_color = [&](const char* key, const ImVec4& value) {
            std::string prefix(key);
            write_float((prefix + "_r").c_str(), value.x);
            write_float((prefix + "_g").c_str(), value.y);
            write_float((prefix + "_b").c_str(), value.z);
            write_float((prefix + "_a").c_str(), value.w);
        };

        auto write_vec3 = [&](const char* key, const rbx::Vector3& value) {
            std::string prefix(key);
            write_float((prefix + "_x").c_str(), value.x);
            write_float((prefix + "_y").c_str(), value.y);
            write_float((prefix + "_z").c_str(), value.z);
        };

        auto write_keybind = [&](const char* key, const c_keybind& bind) {
            std::string prefix(key);
            write_int((prefix + "_key").c_str(), bind.key);
            write_int((prefix + "_type").c_str(), static_cast<int>(bind.type));
        };

        auto write_string = [&](const char* key, const std::string& value) {
            out << key << '=' << value << '\n';
        };

            write_bool("enable_vsync", features->enable_vsync);
            write_bool("debug_overlay", features->debug_overlay);
            write_bool("team_check", features->team_check);
            write_int("phantom_forces_team_filter", features->phantom_forces_team_filter);
            write_bool("show_watermark", features->show_watermark);
            write_bool("show_keybinds_list", features->show_keybinds_list);
            write_bool("show_player_list", features->show_player_list);
            write_bool("show_ai_chat_window", features->show_ai_chat_window);
            write_bool("show_appearance_window", features->show_appearance_window);
            write_bool("show_esp_preview_window", features->show_esp_preview_window);
            write_bool("show_lua_editor_window", features->show_lua_editor_window);
            write_bool("show_console_window", features->show_console_window);
            write_bool("explorer_auto_refresh", features->explorer_auto_refresh);
            write_bool("show_visibility_debug_primitives", features->show_visibility_debug_primitives);
            write_bool("show_configs_window", features->show_configs_window);
            write_bool("show_tool_part_labels", features->show_tool_part_labels);
            write_float("tool_part_trace_lifetime", features->tool_part_trace_lifetime);
            write_float("esp_preview_avatar_scale", features->esp_preview_avatar_scale);
            write_vec3("esp_preview_camera_offset", features->esp_preview_camera_offset);
            write_int("menu_theme", features->menu_theme);
            write_bool("hide_console", features->hide_console);
            write_bool("stream_proof", features->stream_proof);
            write_bool("desync", features->desync);
            write_keybind("desync_keybind", features->desync_keybind);
            write_bool("enable_auto_shooter", features->enable_auto_shooter);
            write_float("host_click_delay_ms", features->host_click_delay_ms);
            write_float("bullets_sent", features->bullets_sent);
            write_color("host_color", features->host_color);
            write_bool("freeze_players", features->freeze_players);
            write_keybind("freeze_players_keybind", features->freeze_players_keybind);
            write_bool("enable_tickrate_modifier", features->enable_tickrate_modifier);
            write_float("tickrate_modifier_value", features->tickrate_modifier_value);

            write_bool("enable_fly", features->enable_fly);
            write_keybind("fly_keybind", features->fly_keybind);
            write_float("fly_speed", features->fly_speed);
            write_float("fly_vertical_boost", features->fly_vertical_boost);
            write_float("fly_damping", features->fly_damping);
            write_bool("fly_check_typing", features->fly_check_typing);

            write_bool("enable_walkspeed", features->enable_walkspeed);
            write_keybind("walkspeed_keybind", features->walkspeed_keybind);
            write_float("walkspeed_value", features->walkspeed_value);
            write_int("walkspeed_mode", features->walkspeed_mode);

            write_bool("enable_bhop", features->enable_bhop);
            write_keybind("bhop_keybind", features->bhop_keybind);
            write_float("bhop_speed", features->bhop_speed);

            write_bool("enable_noclip", features->enable_noclip);
            write_keybind("noclip_keybind", features->noclip_keybind);

            write_bool("enable_aimbot", features->enable_aimbot);
            write_keybind("aimbot_keybind", features->aimbot_keybind);
            write_int("aimbot_mode", features->aimbot_mode);
            write_bool("aimbot_limit_fov", features->aimbot_limit_fov);
            write_float("aimbot_fov_radius", features->aimbot_fov_radius);
            write_bool("aimbot_draw_fov", features->aimbot_draw_fov);
            write_int("aimbot_fov_mode", features->aimbot_fov_mode);
            write_int("aimbot_hitbox", features->aimbot_hitbox);
            write_bool("aimbot_nearest_part", features->aimbot_nearest_part);
            write_bool("enable_aimbot_closest_point", features->enable_aimbot_closest_point);
            write_bool("aimbot_sticky", features->aimbot_sticky);
            write_bool("aimbot_only_enemies", features->aimbot_only_enemies);
            write_bool("aimbot_offscreen_check", features->aimbot_offscreen_check);
            write_bool("aimbot_check_team", features->aimbot_check_team);
            write_bool("aimbot_check_health", features->aimbot_check_health);
            write_bool("aimbot_check_knocked", features->aimbot_check_knocked);
            write_bool("aimbot_check_grabbed", features->aimbot_check_grabbed);
            write_bool("aimbot_check_reloading", features->aimbot_check_reloading);
            write_bool("aimbot_check_typing", features->aimbot_check_typing);
            write_bool("enable_aimbot_smooth", features->enable_aimbot_smooth);
            write_float("aimbot_smooth_x", features->aimbot_smooth_x);
            write_float("aimbot_smooth_y", features->aimbot_smooth_y);
            write_bool("enable_aimbot_prediction", features->enable_aimbot_prediction);
            write_int("aimbot_prediction_mode", features->aimbot_prediction_mode);
            write_float("aimbot_prediction_x", features->aimbot_prediction_x);
            write_float("aimbot_prediction_y", features->aimbot_prediction_y);
            write_float("aimbot_max_distance", features->aimbot_max_distance);

            write_bool("enable_triggerbot", features->enable_triggerbot);
            write_keybind("triggerbot_keybind", features->triggerbot_keybind);
            write_int("triggerbot_hitbox", features->triggerbot_hitbox);
            write_float("triggerbot_delay_ms", features->triggerbot_delay_ms);
            write_bool("triggerbot_hold_fire", features->triggerbot_hold_fire);
            write_bool("triggerbot_only_enemies", features->triggerbot_only_enemies);
            write_bool("triggerbot_check_team", features->triggerbot_check_team);
            write_bool("triggerbot_check_health", features->triggerbot_check_health);
            write_bool("triggerbot_check_knocked", features->triggerbot_check_knocked);
            write_bool("triggerbot_check_grabbed", features->triggerbot_check_grabbed);
            write_bool("triggerbot_check_reloading", features->triggerbot_check_reloading);
            write_bool("triggerbot_check_typing", features->triggerbot_check_typing);
            write_bool("triggerbot_nearest_part", features->triggerbot_nearest_part);
            write_bool("enable_triggerbot_closest_point", features->enable_triggerbot_closest_point);
            write_bool("triggerbot_sticky", features->triggerbot_sticky);
            write_bool("triggerbot_draw_fov", features->triggerbot_draw_fov);
            write_bool("triggerbot_limit_fov", features->triggerbot_limit_fov);
            write_float("triggerbot_fov_radius", features->triggerbot_fov_radius);
            write_int("triggerbot_fov_mode", features->triggerbot_fov_mode);
            write_bool("triggerbot_visibility_check", features->triggerbot_visibility_check);
            write_float("triggerbot_max_distance", features->triggerbot_max_distance);

            write_bool("enable_free_aim", features->enable_free_aim);
            write_keybind("free_aim_keybind", features->free_aim_keybind);
            write_int("free_aim_silent_mode", features->free_aim_silent_mode);
            write_bool("free_aim_limit_fov", features->free_aim_limit_fov);
            write_float("free_aim_fov_radius", features->free_aim_fov_radius);
            write_int("free_aim_hitbox", features->free_aim_hitbox);
            write_bool("free_aim_nearest_part", features->free_aim_nearest_part);
            write_bool("enable_free_aim_closest_point", features->enable_free_aim_closest_point);
            write_bool("free_aim_sticky", features->free_aim_sticky);
            write_bool("free_aim_only_enemies", features->free_aim_only_enemies);
            write_bool("free_aim_check_team", features->free_aim_check_team);
            write_bool("free_aim_check_health", features->free_aim_check_health);
            write_bool("free_aim_check_knocked", features->free_aim_check_knocked);
            write_bool("free_aim_check_grabbed", features->free_aim_check_grabbed);
            write_bool("free_aim_check_typing", features->free_aim_check_typing);
            write_bool("free_aim_mouse_spoof", features->free_aim_mouse_spoof);
            write_bool("free_aim_draw_fov", features->free_aim_draw_fov);
            write_int("free_aim_fov_mode", features->free_aim_fov_mode);
            write_float("free_aim_max_distance", features->free_aim_max_distance);
            write_bool("free_aim_enable_prediction", features->free_aim_enable_prediction);
            write_int("free_aim_prediction_mode", features->free_aim_prediction_mode);
            write_float("free_aim_prediction_x", features->free_aim_prediction_x);
            write_float("free_aim_prediction_y", features->free_aim_prediction_y);

            write_bool("enable_aim_trace", features->enable_aim_trace);
            write_keybind("aim_trace_keybind", features->aim_trace_keybind);
            write_float("aim_trace_max_distance", features->aim_trace_max_distance);
            write_bool("enable_hit_trace", features->enable_hit_trace);
            write_float("hit_trace_width", features->hit_trace_width);
            write_bool("hit_trace_outline", features->hit_trace_outline);
            write_color("hit_trace_color", features->hit_trace_color);
            write_float("hit_trace_lifespan", features->hit_trace_lifespan);
            write_bool("hit_trace_cross_enabled", features->hit_trace_cross_enabled);
            write_color("hit_trace_cross_color", features->hit_trace_cross_color);
            write_float("hit_trace_cross_size", features->hit_trace_cross_size);
            write_float("hit_trace_cross_thickness", features->hit_trace_cross_thickness);
            write_int("hit_trace_effect", features->hit_trace_effect);
            write_bool("enable_hit_logs", features->enable_hit_logs);
            write_int("hit_log_position", features->hit_log_position);

            write_bool("enable_esp", features->enable_esp);
            write_bool("esp_enemy_only", features->esp_enemy_only);
            write_bool("enable_grenade_indicator", features->enable_grenade_indicator);
            write_bool("grenade_indicator_show_timer", features->grenade_indicator_show_timer);
            write_float("grenade_indicator_fuse_time", features->grenade_indicator_fuse_time);
            write_float("grenade_indicator_radius", features->grenade_indicator_radius);
            write_color("grenade_indicator_safe_color", features->grenade_indicator_safe_color);
            write_color("grenade_indicator_danger_color", features->grenade_indicator_danger_color);
            write_bool("enable_target_snapline", features->enable_target_snapline);
            write_color("target_snapline_color", features->target_snapline_color);
            write_bool("target_snapline_outline", features->target_snapline_outline);
            write_color("target_snapline_outline_color", features->target_snapline_outline_color);
            write_float("target_snapline_width", features->target_snapline_width);
            write_int("target_snapline_style", features->target_snapline_style);
            write_bool("enable_crosshair", features->enable_crosshair);
            write_color("crosshair_color", features->crosshair_color);
            write_color("crosshair_outline_color", features->crosshair_outline_color);
            write_float("crosshair_line_width", features->crosshair_line_width);
            write_float("crosshair_gap", features->crosshair_gap);
            write_float("crosshair_size", features->crosshair_size);
            write_bool("crosshair_middle_dot", features->crosshair_middle_dot);
            write_float("crosshair_middle_dot_size", features->crosshair_middle_dot_size);
            write_bool("crosshair_animated_lines", features->crosshair_animated_lines);
            write_float("crosshair_spin_speed", features->crosshair_spin_speed);
            write_bool("crosshair_animated_fill", features->crosshair_animated_fill);
            write_color("crosshair_animated_color_a", features->crosshair_animated_color_a);
            write_color("crosshair_animated_color_b", features->crosshair_animated_color_b);
            write_float("crosshair_gradient_speed", features->crosshair_gradient_speed);
            write_bool("crosshair_ads_gap", features->crosshair_ads_gap);
            write_float("crosshair_ads_gap_scale", features->crosshair_ads_gap_scale);
            write_float("crosshair_ads_gap_lerp_speed", features->crosshair_ads_gap_lerp_speed);
            write_bool("crosshair_lerp", features->crosshair_lerp);
            write_float("crosshair_lerp_speed", features->crosshair_lerp_speed);
            write_bool("enable_bounding_box", features->enable_bounding_box);
            write_int("bounding_box_style", features->bounding_box_style);
            write_color("bounding_box_color", features->bounding_box_color);
            write_bool("draw_dormant", features->draw_dormant);
            write_bool("fade_dormant", features->fade_dormant);
            write_float("dormant_fade_time", features->dormant_fade_time);
            write_bool("override_dormant_color", features->override_dormant_color);
            write_color("dormant_color", features->dormant_color);
            write_bool("enable_highlight", features->enable_highlight);
            write_bool("enable_hit_flash", features->enable_hit_flash);
            write_bool("enable_death_overlay", features->enable_death_overlay);
            write_string("death_overlay_image_path", features->death_overlay_image_path);
            write_int("highlight_mode", features->highlight_mode);
            write_int("highlight_mesh_material", features->highlight_mesh_material);
            write_color("highlight_fill_color", features->highlight_fill_color);
            write_color("highlight_outline_color", features->highlight_outline_color);
            write_bool("enable_raycast_engine", features->enable_raycast_engine);
            write_bool("enable_visibility_check", features->enable_visibility_check);
            write_color("occluded_color", features->occluded_color);
            write_bool("aimbot_visibility_check", features->aimbot_visibility_check);
            write_bool("free_aim_visibility_check", features->free_aim_visibility_check);
            write_bool("enable_skeleton", features->enable_skeleton);
            write_color("skeleton_color", features->skeleton_color);
            write_bool("enable_skeleton_outline", features->enable_skeleton_outline);
            write_color("skeleton_outline_color", features->skeleton_outline_color);
            write_bool("enable_name_esp", features->enable_name_esp);
            write_int("name_esp_mode", features->name_esp_mode);
            write_int("name_esp_font", features->name_esp_font);
            write_color("name_esp_color", features->name_esp_color);
            write_bool("enable_offscreen_arrows", features->enable_offscreen_arrows);
            write_bool("offscreen_arrow_glow", features->offscreen_arrow_glow);
            write_bool("offscreen_arrow_name", features->offscreen_arrow_name);
            write_bool("offscreen_arrow_distance", features->offscreen_arrow_distance);
            write_bool("offscreen_arrow_healthbar", features->offscreen_arrow_healthbar);
            write_float("offscreen_arrow_size", features->offscreen_arrow_size);
            write_color("offscreen_arrow_color", features->offscreen_arrow_color);
            write_bool("enable_healthbar", features->enable_healthbar);
            write_int("healthbar_color_mode", features->healthbar_color_mode);
            write_color("healthbar_top_color", features->healthbar_top_color);
            write_color("healthbar_bottom_color", features->healthbar_bottom_color);
            write_bool("enable_armor_bar", features->enable_armor_bar);
            write_bool("enable_distance", features->enable_distance);
            write_float("max_distance", features->max_distance);
            write_color("distance_color", features->distance_color);
            write_bool("enable_body_status", features->enable_body_status);
            write_bool("show_status_movement", features->show_status_movement);
            write_bool("show_status_reload", features->show_status_reload);
            write_bool("show_status_grabbed", features->show_status_grabbed);
            write_bool("show_status_gun_firing", features->show_status_gun_firing);
            write_bool("show_status_knocked", features->show_status_knocked);
            write_color("status_color", features->status_color);
            write_bool("enable_radar", features->enable_radar);
            write_float("radar_size", features->radar_size);
            write_float("radar_zoom", features->radar_zoom);
            write_bool("radar_auto_zoom", features->radar_auto_zoom);
            write_float("radar_alpha", features->radar_alpha);
            write_int("radar_mode", features->radar_mode);
            write_int("radar_position", features->radar_position);

            write_bool("lighting_enable_brightness", features->lighting_enable_brightness);
            write_float("lighting_brightness_value", features->lighting_brightness_value);
            write_bool("lighting_enable_ambient", features->lighting_enable_ambient);
            write_vec3("lighting_ambient_color", features->lighting_ambient_color);
            write_bool("lighting_enable_outdoor_ambient", features->lighting_enable_outdoor_ambient);
            write_vec3("lighting_outdoor_ambient_color", features->lighting_outdoor_ambient_color);
            write_bool("lighting_enable_fog_color", features->lighting_enable_fog_color);
            write_vec3("lighting_fog_color", features->lighting_fog_color);
            write_bool("lighting_enable_fog_start", features->lighting_enable_fog_start);
            write_float("lighting_fog_start", features->lighting_fog_start);
            write_bool("lighting_enable_fog_end", features->lighting_enable_fog_end);
            write_float("lighting_fog_end", features->lighting_fog_end);
            write_bool("lighting_enable_exposure_compensation", features->lighting_enable_exposure_compensation);
            write_float("lighting_exposure_compensation_value", features->lighting_exposure_compensation_value);
            write_bool("lighting_enable_colorshift_top", features->lighting_enable_colorshift_top);
            write_vec3("lighting_colorshift_top", features->lighting_colorshift_top);
            write_bool("lighting_enable_colorshift_bottom", features->lighting_enable_colorshift_bottom);
            write_vec3("lighting_colorshift_bottom", features->lighting_colorshift_bottom);
            write_bool("lighting_enable_skybox", features->lighting_enable_skybox);
            write_int("lighting_skybox_preset", features->lighting_skybox_preset);
            write_int("lighting_star_count", features->lighting_star_count);
            write_bool("lighting_enable_sun_texture", features->lighting_enable_sun_texture);
            write_string("lighting_sun_texture", features->lighting_sun_texture);
            write_bool("lighting_enable_moon_texture", features->lighting_enable_moon_texture);
            write_string("lighting_moon_texture", features->lighting_moon_texture);
            write_float("lighting_reapply_interval_seconds", features->lighting_reapply_interval_seconds);

            write_bool("enable_arm_modifier", features->enable_arm_modifier);
            write_int("arm_modifier_material", features->arm_modifier_material);
            write_color("arm_modifier_color", features->arm_modifier_color);
        }
    }

    std::string sanitize(const std::string& name)
    {
        return sanitize_string(name);
    }

    void refresh()
    {
        cached_configs.clear();

        std::error_code ec;
        const fs::path dir = configs_directory();

        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
            return;

        for (const auto& entry : fs::directory_iterator(dir, ec))
        {
            if (ec)
                break;

            if (!entry.is_regular_file())
                continue;

            const fs::path& path = entry.path();
            if (path.extension() == ".cfg")
                cached_configs.emplace_back(path.stem().string());
        }

        std::sort(cached_configs.begin(), cached_configs.end());
    }

    void initialize()
    {
        if (initialized)
            return;

        if (!ensure_directory())
        {
            set_status(false, "failed to access config directory");
            return;
        }

        refresh();
        initialized = true;
    }

    const std::vector<std::string>& get_configs()
    {
        return cached_configs;
    }

    const std::string& get_status()
    {
        return last_status;
    }

    bool status_is_error()
    {
        return last_status_error;
    }

    bool save(const std::string& raw_name)
    {
        if (!ensure_directory())
        {
            set_status(false, "failed to access config directory");
            return false;
        }

        std::string sanitized;
        const fs::path path = get_config_path(raw_name, sanitized);

        if (sanitized.empty())
        {
            set_status(false, "invalid config name");
            return false;
        }

        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open())
        {
            set_status(false, "failed to open config file for writing");
            return false;
        }

        write_current_configuration(file);
        file.close();

        set_status(true, "saved config '" + sanitized + "'");
        refresh();
        return true;
    }

    bool export_to_string(const std::string& raw_name, std::string& out)
    {
        std::ostringstream buffer;
        write_current_configuration(buffer);
        out = buffer.str();

        const std::string sanitized = sanitize(raw_name);
        if (!sanitized.empty())
            set_status(true, "copied config '" + sanitized + "' to clipboard");
        else
            set_status(true, "copied config to clipboard");
        return true;
    }

    bool create(const std::string& raw_name)
    {
        std::string sanitized;
        const fs::path path = get_config_path(raw_name, sanitized);

        if (sanitized.empty())
        {
            set_status(false, "invalid config name");
            return false;
        }

        if (fs::exists(path))
        {
            set_status(false, "config already exists");
            return false;
        }

        if (!save(raw_name))
            return false;

        set_status(true, "created config '" + sanitized + "'");
        refresh();
        return true;
    }

    bool load(const std::string& raw_name)
    {
        std::string sanitized;
        const fs::path path = get_config_path(raw_name, sanitized);

        if (sanitized.empty())
        {
            set_status(false, "invalid config name");
            return false;
        }

        std::ifstream file(path);
        if (!file.is_open())
        {
            set_status(false, "failed to open config file for reading");
            return false;
        }

        std::unordered_map<std::string, std::string> values;
        std::string line;

        while (std::getline(file, line))
        {
            const size_t pos = line.find('=');
            if (pos == std::string::npos)
                continue;

            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            if (!key.empty())
                values[key] = value;
        }

        file.close();

        auto to_int = [](const std::string& str, int fallback) {
            try { return std::stoi(str); }
            catch (...) { return fallback; }
        };

        auto to_float = [](const std::string& str, float fallback) {
            try { return std::stof(str); }
            catch (...) { return fallback; }
        };

        auto read_bool = [&](const char* key, bool& target) {
            if (auto it = values.find(key); it != values.end())
                target = to_int(it->second, target ? 1 : 0) != 0;
        };

        auto read_int = [&](const char* key, int& target) {
            if (auto it = values.find(key); it != values.end())
                target = to_int(it->second, target);
        };

        auto read_float = [&](const char* key, float& target) {
            if (auto it = values.find(key); it != values.end())
                target = to_float(it->second, target);
        };

        auto read_color = [&](const char* key, ImVec4& target) {
            float r = target.x;
            float g = target.y;
            float b = target.z;
            float a = target.w;
            std::string prefix(key);
            read_float((prefix + "_r").c_str(), r);
            read_float((prefix + "_g").c_str(), g);
            read_float((prefix + "_b").c_str(), b);
            read_float((prefix + "_a").c_str(), a);
            target = ImVec4(
                std::clamp(r, 0.0f, 1.0f),
                std::clamp(g, 0.0f, 1.0f),
                std::clamp(b, 0.0f, 1.0f),
                std::clamp(a, 0.0f, 1.0f));
        };

        auto read_vec3 = [&](const char* key, rbx::Vector3& target) {
            float x = target.x;
            float y = target.y;
            float z = target.z;
            std::string prefix(key);
            read_float((prefix + "_x").c_str(), x);
            read_float((prefix + "_y").c_str(), y);
            read_float((prefix + "_z").c_str(), z);
            constexpr float limit = 1000.0f;
            target.x = std::clamp(x, -limit, limit);
            target.y = std::clamp(y, -limit, limit);
            target.z = std::clamp(z, -limit, limit);
        };

        auto read_keybind = [&](const char* key, c_keybind& bind) {
            std::string prefix(key);
            int key_val = bind.key;
            int type_val = static_cast<int>(bind.type);
            read_int((prefix + "_key").c_str(), key_val);
            read_int((prefix + "_type").c_str(), type_val);
            key_val = std::clamp(key_val, 0, 0xFF);
            type_val = std::clamp(type_val, 0, 2);
            bind.key = key_val;
            bind.type = static_cast<c_keybind::keybind_mode>(type_val);
            reset_keybind_state(bind);
        };

        auto read_string = [&](const char* key, std::string& target) {
            if (auto it = values.find(key); it != values.end())
            {
                target = it->second;
                constexpr std::size_t k_max_len = 256;
                if (target.size() > k_max_len)
                    target.resize(k_max_len);
            }
        };

        read_bool("enable_vsync", features->enable_vsync);
        read_bool("debug_overlay", features->debug_overlay);
        read_bool("team_check", features->team_check);
        read_int("phantom_forces_team_filter", features->phantom_forces_team_filter);
        features->phantom_forces_team_filter = std::clamp(features->phantom_forces_team_filter, 0, 1);
        read_bool("show_watermark", features->show_watermark);
        read_bool("show_keybinds_list", features->show_keybinds_list);
        read_bool("show_player_list", features->show_player_list);
        read_bool("show_ai_chat_window", features->show_ai_chat_window);
        read_bool("show_appearance_window", features->show_appearance_window);
        read_bool("show_esp_preview_window", features->show_esp_preview_window);
        read_bool("show_lua_editor_window", features->show_lua_editor_window);
        read_bool("show_console_window", features->show_console_window);
        read_bool("explorer_auto_refresh", features->explorer_auto_refresh);
        read_bool("show_visibility_debug_primitives", features->show_visibility_debug_primitives);
        read_bool("show_configs_window", features->show_configs_window);
        read_bool("show_tool_part_labels", features->show_tool_part_labels);
        read_float("tool_part_trace_lifetime", features->tool_part_trace_lifetime);
        features->tool_part_trace_lifetime = std::clamp(features->tool_part_trace_lifetime, 0.1f, 5.0f);
        read_float("esp_preview_avatar_scale", features->esp_preview_avatar_scale);
        features->esp_preview_avatar_scale = std::clamp(features->esp_preview_avatar_scale, 0.25f, 4.0f);
        read_vec3("esp_preview_camera_offset", features->esp_preview_camera_offset);
        read_bool("hide_console", features->hide_console);
        read_bool("stream_proof", features->stream_proof);
        read_bool("desync", features->desync);
        read_keybind("desync_keybind", features->desync_keybind);
        read_bool("enable_auto_shooter", features->enable_auto_shooter);
        read_float("host_click_delay_ms", features->host_click_delay_ms);
        features->host_click_delay_ms = std::clamp(features->host_click_delay_ms, 1.0f, 1000.0f);
        read_float("bullets_sent", features->bullets_sent);
        features->bullets_sent = std::clamp(features->bullets_sent, 1.0f, 500.0f);
        read_color("host_color", features->host_color);
        read_bool("freeze_players", features->freeze_players);
        read_keybind("freeze_players_keybind", features->freeze_players_keybind);
        read_bool("enable_tickrate_modifier", features->enable_tickrate_modifier);
        read_float("tickrate_modifier_value", features->tickrate_modifier_value);
        features->tickrate_modifier_value = std::clamp(features->tickrate_modifier_value, 1.0f, 660.0f);

        read_bool("enable_fly", features->enable_fly);
        read_keybind("fly_keybind", features->fly_keybind);
        read_float("fly_speed", features->fly_speed);
        features->fly_speed = std::clamp(features->fly_speed, 0.0f, 500.0f);
        read_float("fly_vertical_boost", features->fly_vertical_boost);
        features->fly_vertical_boost = std::clamp(features->fly_vertical_boost, 0.1f, 5.0f);
        read_float("fly_damping", features->fly_damping);
        features->fly_damping = std::clamp(features->fly_damping, 0.0f, 100.0f);
        read_bool("fly_check_typing", features->fly_check_typing);

        read_bool("enable_walkspeed", features->enable_walkspeed);
        read_keybind("walkspeed_keybind", features->walkspeed_keybind);
        read_float("walkspeed_value", features->walkspeed_value);
        features->walkspeed_value = std::clamp(features->walkspeed_value, 1.0f, 500.0f);
        read_int("walkspeed_mode", features->walkspeed_mode);
        features->walkspeed_mode = std::clamp(features->walkspeed_mode, 0, 1);

        read_bool("enable_bhop", features->enable_bhop);
        read_keybind("bhop_keybind", features->bhop_keybind);
        read_float("bhop_speed", features->bhop_speed);
        features->bhop_speed = std::clamp(features->bhop_speed, 1.0f, 500.0f);

        read_bool("enable_noclip", features->enable_noclip);
        read_keybind("noclip_keybind", features->noclip_keybind);

        read_bool("enable_aimbot", features->enable_aimbot);
        read_keybind("aimbot_keybind", features->aimbot_keybind);
        read_int("aimbot_mode", features->aimbot_mode);
        features->aimbot_mode = std::clamp(features->aimbot_mode, 0, 1);
        read_bool("aimbot_limit_fov", features->aimbot_limit_fov);
        read_float("aimbot_fov_radius", features->aimbot_fov_radius);
        features->aimbot_fov_radius = std::clamp(features->aimbot_fov_radius, 0.0f, 2000.0f);
        read_bool("aimbot_draw_fov", features->aimbot_draw_fov);
        read_int("aimbot_fov_mode", features->aimbot_fov_mode);
        features->aimbot_fov_mode = std::clamp(features->aimbot_fov_mode, 0, 1);
        read_int("aimbot_hitbox", features->aimbot_hitbox);
        features->aimbot_hitbox = std::clamp(features->aimbot_hitbox, 0, 2);
        read_bool("aimbot_nearest_part", features->aimbot_nearest_part);
        read_bool("enable_aimbot_closest_point", features->enable_aimbot_closest_point);
        read_bool("aimbot_sticky", features->aimbot_sticky);
        read_bool("aimbot_only_enemies", features->aimbot_only_enemies);
        read_bool("aimbot_offscreen_check", features->aimbot_offscreen_check);
        read_bool("aimbot_check_team", features->aimbot_check_team);
        read_bool("aimbot_check_health", features->aimbot_check_health);
        read_bool("aimbot_check_knocked", features->aimbot_check_knocked);
        read_bool("aimbot_check_grabbed", features->aimbot_check_grabbed);
        read_bool("aimbot_check_reloading", features->aimbot_check_reloading);
        read_bool("aimbot_check_typing", features->aimbot_check_typing);
        read_bool("enable_aimbot_smooth", features->enable_aimbot_smooth);
        read_float("aimbot_smooth_x", features->aimbot_smooth_x);
        read_float("aimbot_smooth_y", features->aimbot_smooth_y);
        features->aimbot_smooth_x = std::clamp(features->aimbot_smooth_x, 0.0f, 500.0f);
        features->aimbot_smooth_y = std::clamp(features->aimbot_smooth_y, 0.0f, 500.0f);
        read_bool("enable_aimbot_prediction", features->enable_aimbot_prediction);
        read_int("aimbot_prediction_mode", features->aimbot_prediction_mode);
        features->aimbot_prediction_mode = std::clamp(features->aimbot_prediction_mode, 0, 1);
        read_float("aimbot_prediction_x", features->aimbot_prediction_x);
        read_float("aimbot_prediction_y", features->aimbot_prediction_y);
        read_float("aimbot_max_distance", features->aimbot_max_distance);
        features->aimbot_max_distance = std::clamp(features->aimbot_max_distance, 0.0f, 5000.0f);
        features->aimbot_prediction_x = std::clamp(features->aimbot_prediction_x, 0.0f, 50.0f);
        features->aimbot_prediction_y = std::clamp(features->aimbot_prediction_y, 0.0f, 50.0f);

        read_bool("enable_triggerbot", features->enable_triggerbot);
        read_keybind("triggerbot_keybind", features->triggerbot_keybind);
        read_int("triggerbot_hitbox", features->triggerbot_hitbox);
        features->triggerbot_hitbox = std::clamp(features->triggerbot_hitbox, 0, 2);
        read_float("triggerbot_delay_ms", features->triggerbot_delay_ms);
        features->triggerbot_delay_ms = std::clamp(features->triggerbot_delay_ms, 0.0f, 300.0f);
        read_bool("triggerbot_hold_fire", features->triggerbot_hold_fire);
        read_bool("triggerbot_only_enemies", features->triggerbot_only_enemies);
        read_bool("triggerbot_check_team", features->triggerbot_check_team);
        read_bool("triggerbot_check_health", features->triggerbot_check_health);
        read_bool("triggerbot_check_knocked", features->triggerbot_check_knocked);
        read_bool("triggerbot_check_grabbed", features->triggerbot_check_grabbed);
        read_bool("triggerbot_check_reloading", features->triggerbot_check_reloading);
        read_bool("triggerbot_check_typing", features->triggerbot_check_typing);
        read_bool("triggerbot_nearest_part", features->triggerbot_nearest_part);
        read_bool("enable_triggerbot_closest_point", features->enable_triggerbot_closest_point);
        read_bool("triggerbot_sticky", features->triggerbot_sticky);
        read_bool("triggerbot_draw_fov", features->triggerbot_draw_fov);
        read_bool("triggerbot_limit_fov", features->triggerbot_limit_fov);
        read_float("triggerbot_fov_radius", features->triggerbot_fov_radius);
        features->triggerbot_fov_radius = std::clamp(features->triggerbot_fov_radius, 0.0f, 2000.0f);
        read_int("triggerbot_fov_mode", features->triggerbot_fov_mode);
        features->triggerbot_fov_mode = std::clamp(features->triggerbot_fov_mode, 0, 1);
        read_bool("triggerbot_visibility_check", features->triggerbot_visibility_check);
        read_float("triggerbot_max_distance", features->triggerbot_max_distance);
        features->triggerbot_max_distance = std::clamp(features->triggerbot_max_distance, 0.0f, 5000.0f);

        read_bool("enable_free_aim", features->enable_free_aim);
        read_keybind("free_aim_keybind", features->free_aim_keybind);
        read_int("free_aim_silent_mode", features->free_aim_silent_mode);
        features->free_aim_silent_mode = std::clamp(features->free_aim_silent_mode, 0, 1);
        read_bool("free_aim_limit_fov", features->free_aim_limit_fov);
        read_float("free_aim_fov_radius", features->free_aim_fov_radius);
        features->free_aim_fov_radius = std::clamp(features->free_aim_fov_radius, 0.0f, 2000.0f);
        read_int("free_aim_hitbox", features->free_aim_hitbox);
        features->free_aim_hitbox = std::clamp(features->free_aim_hitbox, 0, 2);
        read_bool("free_aim_nearest_part", features->free_aim_nearest_part);
        read_bool("enable_free_aim_closest_point", features->enable_free_aim_closest_point);
        read_bool("free_aim_sticky", features->free_aim_sticky);
        read_bool("free_aim_only_enemies", features->free_aim_only_enemies);
        read_bool("free_aim_check_team", features->free_aim_check_team);
        read_bool("free_aim_check_health", features->free_aim_check_health);
        read_bool("free_aim_check_knocked", features->free_aim_check_knocked);
        read_bool("free_aim_check_grabbed", features->free_aim_check_grabbed);
        read_bool("free_aim_check_typing", features->free_aim_check_typing);
        read_bool("free_aim_mouse_spoof", features->free_aim_mouse_spoof);
        read_bool("free_aim_draw_fov", features->free_aim_draw_fov);
        read_int("free_aim_fov_mode", features->free_aim_fov_mode);
        features->free_aim_fov_mode = std::clamp(features->free_aim_fov_mode, 0, 1);
        read_float("free_aim_max_distance", features->free_aim_max_distance);
        features->free_aim_max_distance = std::clamp(features->free_aim_max_distance, 0.0f, 5000.0f);
        read_bool("free_aim_enable_prediction", features->free_aim_enable_prediction);
        read_int("free_aim_prediction_mode", features->free_aim_prediction_mode);
        features->free_aim_prediction_mode = std::clamp(features->free_aim_prediction_mode, 0, 1);
        read_float("free_aim_prediction_x", features->free_aim_prediction_x);
        read_float("free_aim_prediction_y", features->free_aim_prediction_y);
        features->free_aim_prediction_x = std::clamp(features->free_aim_prediction_x, 0.0f, 50.0f);
        features->free_aim_prediction_y = std::clamp(features->free_aim_prediction_y, 0.0f, 50.0f);

        read_bool("enable_aim_trace", features->enable_aim_trace);
        read_keybind("aim_trace_keybind", features->aim_trace_keybind);
        read_float("aim_trace_max_distance", features->aim_trace_max_distance);
        features->aim_trace_max_distance = std::clamp(features->aim_trace_max_distance, 0.0f, 10000.0f);
        read_bool("enable_hit_trace", features->enable_hit_trace);
        read_float("hit_trace_width", features->hit_trace_width);
        features->hit_trace_width = std::clamp(features->hit_trace_width, 0.1f, 12.0f);
        read_bool("hit_trace_outline", features->hit_trace_outline);
        read_color("hit_trace_color", features->hit_trace_color);
        read_float("hit_trace_lifespan", features->hit_trace_lifespan);
        features->hit_trace_lifespan = std::clamp(features->hit_trace_lifespan, 0.02f, 10.0f);
        read_bool("hit_trace_cross_enabled", features->hit_trace_cross_enabled);
        read_color("hit_trace_cross_color", features->hit_trace_cross_color);
        read_float("hit_trace_cross_size", features->hit_trace_cross_size);
        features->hit_trace_cross_size = std::clamp(features->hit_trace_cross_size, 2.0f, 40.0f);
        read_float("hit_trace_cross_thickness", features->hit_trace_cross_thickness);
        features->hit_trace_cross_thickness = std::clamp(features->hit_trace_cross_thickness, 0.5f, 10.0f);
        read_int("hit_trace_effect", features->hit_trace_effect);
        features->hit_trace_effect = std::clamp(features->hit_trace_effect, 0, 3);
        read_bool("enable_hit_logs", features->enable_hit_logs);
        read_int("hit_log_position", features->hit_log_position);
        features->hit_log_position = std::clamp(features->hit_log_position, 0, 4);

        read_bool("enable_esp", features->enable_esp);
        read_bool("esp_enemy_only", features->esp_enemy_only);
        read_bool("enable_grenade_indicator", features->enable_grenade_indicator);
        read_bool("grenade_indicator_show_timer", features->grenade_indicator_show_timer);
        read_float("grenade_indicator_fuse_time", features->grenade_indicator_fuse_time);
        features->grenade_indicator_fuse_time = std::clamp(features->grenade_indicator_fuse_time, 0.5f, 15.0f);
        read_float("grenade_indicator_radius", features->grenade_indicator_radius);
        features->grenade_indicator_radius = std::clamp(features->grenade_indicator_radius, 6.0f, 28.0f);
        read_color("grenade_indicator_safe_color", features->grenade_indicator_safe_color);
        read_color("grenade_indicator_danger_color", features->grenade_indicator_danger_color);
        read_bool("enable_target_snapline", features->enable_target_snapline);
        read_color("target_snapline_color", features->target_snapline_color);
        read_bool("target_snapline_outline", features->target_snapline_outline);
        read_color("target_snapline_outline_color", features->target_snapline_outline_color);
        read_float("target_snapline_width", features->target_snapline_width);
        features->target_snapline_width = std::clamp(features->target_snapline_width, 0.5f, 8.0f);
        read_int("target_snapline_style", features->target_snapline_style);
        features->target_snapline_style = std::clamp(features->target_snapline_style, 0, 2);
        read_bool("enable_crosshair", features->enable_crosshair);
        read_color("crosshair_color", features->crosshair_color);
        read_color("crosshair_outline_color", features->crosshair_outline_color);
        read_float("crosshair_line_width", features->crosshair_line_width);
        features->crosshair_line_width = std::clamp(features->crosshair_line_width, 0.5f, 8.0f);
        read_float("crosshair_gap", features->crosshair_gap);
        features->crosshair_gap = std::clamp(features->crosshair_gap, 0.0f, 48.0f);
        read_float("crosshair_size", features->crosshair_size);
        features->crosshair_size = std::clamp(features->crosshair_size, 2.0f, 64.0f);
        read_bool("crosshair_middle_dot", features->crosshair_middle_dot);
        read_float("crosshair_middle_dot_size", features->crosshair_middle_dot_size);
        features->crosshair_middle_dot_size = std::clamp(features->crosshair_middle_dot_size, 1.0f, 12.0f);
        read_bool("crosshair_animated_lines", features->crosshair_animated_lines);
        read_float("crosshair_spin_speed", features->crosshair_spin_speed);
        features->crosshair_spin_speed = std::clamp(features->crosshair_spin_speed, 1.0f, 720.0f);
        read_bool("crosshair_animated_fill", features->crosshair_animated_fill);
        read_color("crosshair_animated_color_a", features->crosshair_animated_color_a);
        read_color("crosshair_animated_color_b", features->crosshair_animated_color_b);
        read_float("crosshair_gradient_speed", features->crosshair_gradient_speed);
        features->crosshair_gradient_speed = std::clamp(features->crosshair_gradient_speed, 0.05f, 3.0f);
        read_bool("crosshair_ads_gap", features->crosshair_ads_gap);
        read_float("crosshair_ads_gap_scale", features->crosshair_ads_gap_scale);
        features->crosshair_ads_gap_scale = std::clamp(features->crosshair_ads_gap_scale, 0.05f, 1.0f);
        read_float("crosshair_ads_gap_lerp_speed", features->crosshair_ads_gap_lerp_speed);
        features->crosshair_ads_gap_lerp_speed = std::clamp(features->crosshair_ads_gap_lerp_speed, 1.0f, 30.0f);
        read_bool("crosshair_unknown", features->crosshair_ads_gap);
        read_float("crosshair_unknown_gap_scale", features->crosshair_ads_gap_scale);
        features->crosshair_ads_gap_scale = std::clamp(features->crosshair_ads_gap_scale, 0.05f, 1.0f);
        read_float("crosshair_unknown_lerp_speed", features->crosshair_ads_gap_lerp_speed);
        features->crosshair_ads_gap_lerp_speed = std::clamp(features->crosshair_ads_gap_lerp_speed, 1.0f, 30.0f);
        read_bool("crosshair_lerp", features->crosshair_lerp);
        read_float("crosshair_lerp_speed", features->crosshair_lerp_speed);
        features->crosshair_lerp_speed = std::clamp(features->crosshair_lerp_speed, 1.0f, 30.0f);
        read_bool("enable_bounding_box", features->enable_bounding_box);
        read_int("bounding_box_style", features->bounding_box_style);
        features->bounding_box_style = std::clamp(features->bounding_box_style, 0, 1);
        read_color("bounding_box_color", features->bounding_box_color);
        read_int("menu_theme", features->menu_theme);
        features->menu_theme = std::clamp(features->menu_theme, 0, 7);
        read_bool("draw_dormant", features->draw_dormant);
        read_bool("fade_dormant", features->fade_dormant);
        read_float("dormant_fade_time", features->dormant_fade_time);
        features->dormant_fade_time = std::clamp(features->dormant_fade_time, 0.0f, 10.0f);
        read_bool("override_dormant_color", features->override_dormant_color);
        read_color("dormant_color", features->dormant_color);
        read_bool("enable_highlight", features->enable_highlight);
        read_bool("enable_hit_flash", features->enable_hit_flash);
        read_bool("enable_death_overlay", features->enable_death_overlay);
        read_string("death_overlay_image_path", features->death_overlay_image_path);
        read_int("highlight_mode", features->highlight_mode);
        features->highlight_mode = std::clamp(features->highlight_mode, 0, 2);
        read_int("highlight_mesh_material", features->highlight_mesh_material);
        features->highlight_mesh_material = std::clamp(features->highlight_mesh_material, 0, 4);
        read_color("highlight_fill_color", features->highlight_fill_color);
        read_color("highlight_outline_color", features->highlight_outline_color);
        read_bool("enable_raycast_engine", features->enable_raycast_engine);
        read_bool("enable_visibility_check", features->enable_visibility_check);
        read_color("occluded_color", features->occluded_color);
        read_bool("aimbot_visibility_check", features->aimbot_visibility_check);
        read_bool("free_aim_visibility_check", features->free_aim_visibility_check);
        read_bool("enable_skeleton", features->enable_skeleton);
        read_color("skeleton_color", features->skeleton_color);
        read_bool("enable_skeleton_outline", features->enable_skeleton_outline);
        read_color("skeleton_outline_color", features->skeleton_outline_color);
        read_bool("enable_name_esp", features->enable_name_esp);
        read_int("name_esp_mode", features->name_esp_mode);
        features->name_esp_mode = std::clamp(features->name_esp_mode, 0, 1);
        read_int("name_esp_font", features->name_esp_font);
        features->name_esp_font = std::clamp(features->name_esp_font, 0, 4);
        read_color("name_esp_color", features->name_esp_color);
        read_bool("enable_offscreen_arrows", features->enable_offscreen_arrows);
        read_bool("offscreen_arrow_glow", features->offscreen_arrow_glow);
        read_bool("offscreen_arrow_name", features->offscreen_arrow_name);
        read_bool("offscreen_arrow_distance", features->offscreen_arrow_distance);
        read_bool("offscreen_arrow_healthbar", features->offscreen_arrow_healthbar);
        read_float("offscreen_arrow_size", features->offscreen_arrow_size);
        features->offscreen_arrow_size = std::clamp(features->offscreen_arrow_size, 6.0f, 36.0f);
        read_color("offscreen_arrow_color", features->offscreen_arrow_color);
        read_bool("enable_healthbar", features->enable_healthbar);
        read_int("healthbar_color_mode", features->healthbar_color_mode);
        features->healthbar_color_mode = std::clamp(features->healthbar_color_mode, 0, 1);
        read_color("healthbar_top_color", features->healthbar_top_color);
        read_color("healthbar_bottom_color", features->healthbar_bottom_color);
        read_bool("enable_armor_bar", features->enable_armor_bar);
        read_bool("enable_distance", features->enable_distance);
        read_float("max_distance", features->max_distance);
        features->max_distance = std::clamp(features->max_distance, 0.0f, 5000.0f);
        read_color("distance_color", features->distance_color);
        read_bool("enable_body_status", features->enable_body_status);
        read_bool("show_status_movement", features->show_status_movement);
        read_bool("show_status_reload", features->show_status_reload);
        read_bool("show_status_grabbed", features->show_status_grabbed);
        read_bool("show_status_gun_firing", features->show_status_gun_firing);
        read_bool("show_status_knocked", features->show_status_knocked);
        read_color("status_color", features->status_color);
        read_bool("enable_radar", features->enable_radar);
        read_float("radar_size", features->radar_size);
        features->radar_size = std::clamp(features->radar_size, 80.0f, 400.0f);
        read_float("radar_zoom", features->radar_zoom);
        features->radar_zoom = std::clamp(features->radar_zoom, 0.25f, 4.0f);
        read_bool("radar_auto_zoom", features->radar_auto_zoom);
        read_float("radar_alpha", features->radar_alpha);
        features->radar_alpha = std::clamp(features->radar_alpha, 0.05f, 1.0f);
        read_int("radar_mode", features->radar_mode);
        features->radar_mode = std::clamp(features->radar_mode, 0, 1);
        read_int("radar_position", features->radar_position);
        features->radar_position = std::clamp(features->radar_position, 0, 3);

        read_bool("lighting_enable_brightness", features->lighting_enable_brightness);
        read_float("lighting_brightness_value", features->lighting_brightness_value);
        features->lighting_brightness_value = std::clamp(features->lighting_brightness_value, 0.0f, 50.0f);
        read_bool("lighting_enable_ambient", features->lighting_enable_ambient);
        read_vec3("lighting_ambient_color", features->lighting_ambient_color);
        read_bool("lighting_enable_outdoor_ambient", features->lighting_enable_outdoor_ambient);
        read_vec3("lighting_outdoor_ambient_color", features->lighting_outdoor_ambient_color);
        read_bool("lighting_enable_fog_color", features->lighting_enable_fog_color);
        read_vec3("lighting_fog_color", features->lighting_fog_color);
        read_bool("lighting_enable_fog_start", features->lighting_enable_fog_start);
        read_float("lighting_fog_start", features->lighting_fog_start);
        features->lighting_fog_start = std::clamp(features->lighting_fog_start, 0.0f, 200000.0f);
        read_bool("lighting_enable_fog_end", features->lighting_enable_fog_end);
        read_float("lighting_fog_end", features->lighting_fog_end);
        features->lighting_fog_end = std::clamp(features->lighting_fog_end, 0.0f, 200000.0f);
        read_bool("lighting_enable_exposure_compensation", features->lighting_enable_exposure_compensation);
        read_float("lighting_exposure_compensation_value", features->lighting_exposure_compensation_value);
        features->lighting_exposure_compensation_value = std::clamp(features->lighting_exposure_compensation_value, -50.0f, 50.0f);
        read_bool("lighting_enable_colorshift_top", features->lighting_enable_colorshift_top);
        read_vec3("lighting_colorshift_top", features->lighting_colorshift_top);
        read_bool("lighting_enable_colorshift_bottom", features->lighting_enable_colorshift_bottom);
        read_vec3("lighting_colorshift_bottom", features->lighting_colorshift_bottom);
        read_bool("lighting_enable_skybox", features->lighting_enable_skybox);
        read_int("lighting_skybox_preset", features->lighting_skybox_preset);
        features->lighting_skybox_preset = std::clamp(features->lighting_skybox_preset, 0, 4);
        read_int("lighting_star_count", features->lighting_star_count);
        features->lighting_star_count = std::clamp(features->lighting_star_count, 0, 6000);
        read_bool("lighting_enable_sun_texture", features->lighting_enable_sun_texture);
        read_string("lighting_sun_texture", features->lighting_sun_texture);
        read_bool("lighting_enable_moon_texture", features->lighting_enable_moon_texture);
        read_string("lighting_moon_texture", features->lighting_moon_texture);
        read_float("lighting_reapply_interval_seconds", features->lighting_reapply_interval_seconds);
        features->lighting_reapply_interval_seconds = std::clamp(features->lighting_reapply_interval_seconds, 0.1f, 10.0f);

        read_bool("enable_arm_modifier", features->enable_arm_modifier);
        if (values.find("enable_arm_modifier") == values.end())
        {
            read_bool("enable_camera_descendant_fx", features->enable_arm_modifier);
        }

        read_int("arm_modifier_material", features->arm_modifier_material);
        if (values.find("arm_modifier_material") == values.end())
        {
            read_int("camera_descendant_material", features->arm_modifier_material);
        }

        read_color("arm_modifier_color", features->arm_modifier_color);
        const bool has_arm_color =
            values.find("arm_modifier_color_r") != values.end() ||
            values.find("arm_modifier_color_g") != values.end() ||
            values.find("arm_modifier_color_b") != values.end() ||
            values.find("arm_modifier_color_a") != values.end();
        if (!has_arm_color)
        {
            read_color("camera_descendant_color", features->arm_modifier_color);
        }

        features->arm_modifier_material = std::clamp(features->arm_modifier_material, 0, 4096);

        lighting::apply();
        lighting::force_renderview_flag();
        set_status(true, "loaded config '" + sanitized + "'");
        return true;
    }

    bool remove(const std::string& raw_name)
    {
        std::string sanitized;
        const fs::path path = get_config_path(raw_name, sanitized);
        if (sanitized.empty())
        {
            set_status(false, "invalid config name");
            return false;
        }

        std::error_code ec;
        if (!fs::exists(path, ec))
        {
            set_status(false, "config not found");
            return false;
        }

        if (!fs::remove(path, ec) || ec)
        {
            set_status(false, "failed to remove config");
            return false;
        }

        set_status(true, "removed config '" + sanitized + "'");
        refresh();
        return true;
    }

    void open_directory()
    {
        if (!ensure_directory())
        {
            set_status(false, "failed to access config directory");
            return;
        }

        const fs::path dir = configs_directory();
        const HINSTANCE result = ShellExecuteW(nullptr, L"open", dir.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32)
            set_status(false, "failed to open config folder");
        else
            set_status(true, "opened config folder");
    }
}
