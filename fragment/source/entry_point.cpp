#include "utils/logger.h"
#include "globals/globals.h"
#include "cache/local_player_cache.h"
#include "cache/player_cache.h"
#include "cache/dead_body_cache.h"
#include "gui/overlay.hpp"
#include "sdk/engine.h"
#include "sdk/offsets.h"
#include "features/lighting.h"
#include "features/aimbot.h"
#include "features/free_aim.h"
#include "features/pf_silent.h"
#include "features/hacks.h"
#include "features/shooter.h"
#include "features/triggerbot.h"
#include "features/visibility.h"
#include "features/tests.h"
#include "utils/window_utils.h"
#include <memory/memory.h>
#include <cstdint>
#include <array>
#include <chrono>
#include <atomic>
#include <thread>
#include <Windows.h>

namespace
{
    std::atomic<bool> teleport_watch_running{ false };
    std::thread teleport_watch_thread;
    std::atomic<bool> tests_loop_running{ false };
    std::thread tests_loop_thread;
    std::atomic<bool> diag_loop_running{ false };
    std::thread diag_loop_thread;
    std::uintptr_t last_datamodel_address = 0;
    std::int64_t last_place_id = 0;

    bool sync_globals()
    {
        globals->datamodel = rbx::engine->get_datamodel();
        if (!globals->datamodel.is_valid())
        {
            logger_core::log_warning("sync_globals -> datamodel invalid");
            return false;
        }

        globals->visualengine = rbx::instance_t(rbx::engine->get_visualengine());
        globals->renderview = rbx::engine->get_renderview();
        globals->players = globals->datamodel.find_first_child_by_class("Players");
        globals->workspace = globals->datamodel.find_first_child_by_class("Workspace");
        globals->lighting = globals->datamodel.find_first_child_by_class("Lighting");
        globals->mouse_service = globals->datamodel.find_first_child_by_class("MouseService");
        globals->text_chat_service = globals->datamodel.find_first_child("TextChatService");
        globals->user_input_service = globals->datamodel.find_first_child("UserInputService");
        globals->chat_input_bar_configuration = {};
        if (globals->text_chat_service.is_valid())
        {
            globals->chat_input_bar_configuration = globals->text_chat_service.find_first_child("ChatInputBarConfiguration");
        }
        globals->game_id = globals->datamodel.get_game_id();

        if (!globals->players.is_valid() || !globals->workspace.is_valid())
        {
            logger_core::log_warning(
                "sync_globals -> players=0x{:X}, workspace=0x{:X}, lighting=0x{:X}, mouse_service=0x{:X}",
                globals->players.get_address(),
                globals->workspace.get_address(),
                globals->lighting.get_address(),
                globals->mouse_service.get_address());
            return false;
        }

        return true;
    }

    void log_globals()
    {
        logger_core::log_success("datamodel: 0x{:X}", globals->datamodel.get_address());
        logger_core::log_success("players: 0x{:X}", globals->players.get_address());
        logger_core::log_success("game_id: {}", globals->game_id);
        logger_core::log_success("workspace: 0x{:X}", globals->workspace.get_address());
        logger_core::log_success("renderview: 0x{:X}", globals->renderview.get_address());
        logger_core::log_success("lighting: 0x{:X}", globals->lighting.get_address());
        logger_core::log_success("mouse_service: 0x{:X}", globals->mouse_service.get_address());
        logger_core::log_success("text_chat_service: 0x{:X}", globals->text_chat_service.get_address());
        logger_core::log_success("chat_input_bar_configuration: 0x{:X}", globals->chat_input_bar_configuration.get_address());
        logger_core::log_success("user_input_service: 0x{:X}", globals->user_input_service.get_address());
    }

    void restart_caches()
    {
        cache::localplayer->stop();
        cache::players_cache->stop();
        cache::dead_bodies_cache->stop();
        visibility::reset_occluder_cache();
        cache::localplayer->start();
        cache::players_cache->start();
        cache::dead_bodies_cache->start();
    }

    void start_tp_watch()
    {
        teleport_watch_running = true;
        last_datamodel_address = globals->datamodel.get_address();
        last_place_id = globals->game_id;

        teleport_watch_thread = std::thread([]()
            {
                const auto sleep_interval = std::chrono::milliseconds(750);
                while (teleport_watch_running.load(std::memory_order_relaxed))
                {
                    if (!memory->is_process_alive())
                    {
                        globals->reset();
                        rbx::engine->shutdown();

                        const HWND overlay_window = fragment::overlay::g_overlay_window;
                        if (overlay_window)
                        {
                            ::PostMessage(overlay_window, WM_CLOSE, 0, 0);
                        }
                        break;
                    }

                    if (!rbx::engine->refresh())
                    {
                        globals->reset();
                        std::this_thread::sleep_for(sleep_interval);
                        continue;
                    }

                    if (!sync_globals())
                    {
                        globals->reset();
                        std::this_thread::sleep_for(sleep_interval);
                        continue;
                    }

                    const auto datamodel_addr = globals->datamodel.get_address();
                    const auto place_id = globals->game_id;

                    if (datamodel_addr != last_datamodel_address || place_id != last_place_id)
                    {
                        logger_core::log_info("teleport detected -> datamodel=0x{:X}, place_id={}", datamodel_addr, place_id);
                        last_datamodel_address = datamodel_addr;
                        last_place_id = place_id;

                        lighting::apply();
                        restart_caches();
                        if (globals->mouse_service.is_valid())
                        {
                            std::thread([] { globals->mouse_service.cache_input_object(); }).detach();
                        }
                    }

                    std::this_thread::sleep_for(sleep_interval);
                }
            });
    }

    void stop_tp_watch()
    {
        if (!teleport_watch_running.exchange(false))
        {
            return;
        }

        if (teleport_watch_thread.joinable())
        {
            teleport_watch_thread.join();
        }
    }

    void start_tests_loop()
    {
        tests_loop_running = true;
        tests_loop_thread = std::thread([]()
            {
                const auto sleep_interval = std::chrono::milliseconds(50);
                while (tests_loop_running.load(std::memory_order_relaxed))
                {
                    tests::render();
                    std::this_thread::sleep_for(sleep_interval);
                }
            });
    }

    void stop_tests_loop()
    {
        if (!tests_loop_running.exchange(false))
        {
            return;
        }

        if (tests_loop_thread.joinable())
        {
            tests_loop_thread.join();
        }
    }

    void start_diag_loop()
    {
        diag_loop_running = true;
        diag_loop_thread = std::thread([]()
            {
                int tick = 0;
                while (diag_loop_running.load(std::memory_order_relaxed))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    if (++tick % 2 != 0)
                    {
                        continue;
                    }

                    if (!memory->is_process_alive())
                    {
                        continue;
                    }

                    const auto lp = cache::localplayer->snapshot();
                    const auto players = cache::players_cache->snapshot();
                    const std::size_t count = players ? players->size() : 0;

                    const auto local_char_addr = lp.character.get_address();
                    const auto local_char_valid = lp.character.get_class_name();
                    const auto raw_char_298 = lp.player.is_valid()
                        ? memory->read<std::uintptr_t>(lp.player.get_address() + roblox::offsets::player::character)
                        : 0;
                    logger_core::log_info(
                        "[diag] local=0x{:X} raw_char(+0x298)=0x{:X} char=0x{:X}({}) camera=0x{:X} end_parts=0x{:X} players={}",
                        lp.address,
                        raw_char_298,
                        local_char_addr,
                        local_char_valid.empty() ? "?" : local_char_valid,
                        lp.camera.get_address(),
                        lp.parts.humanoid_root_part.primitive,
                        count);

                    for (std::size_t i = 0; i < count && i < 10; ++i)
                    {
                        const auto& p = (*players)[i];
                        const auto char_valid = p.character.get_class_name();
                        logger_core::log_info(
                            "[diag]   [{}/{}] 0x{:X} '{}' char=0x{:X}({}) head.prim=0x{:X}",
                            i, count,
                            p.address,
                            p.name.empty() ? "?" : p.name,
                            p.character.get_address(),
                            char_valid.empty() ? "?" : char_valid,
                            p.parts.head.primitive);
                    }
                }
            });
    }

    void stop_diag_loop()
    {
        if (!diag_loop_running.exchange(false))
        {
            return;
        }

        if (diag_loop_thread.joinable())
        {
            diag_loop_thread.join();
        }
    }
}

int entry_point()
{
    static const char* process_name = "RobloxPlayerBeta.exe";

    if (!memory->find_process_id(process_name))
    {
        logger_core::log_error("failed to find process id -> {}", process_name);
        return 0;
    }
    logger_core::log_success("proc: {}", memory->get_process_id());

    if (!memory->attach_to_process(process_name))
    {
        logger_core::log_error("failed to attach to process -> {}", process_name);
        return 0;
    }

    if (!memory->get_process_handle())
    {
        logger_core::log_error("invalid process handle");
        return 0;
    }

    if (!memory->find_module_address(process_name))
    {
        logger_core::log_warning("invalid module address");
        return 0;
    }
    logger_core::log_success("module: {:X}", memory->get_module_address());

    HWND roblox_window = window_utils::find_main_window(memory->get_process_id());
    if (!roblox_window)
    {
        logger_core::log_error("failed to locate roblox window");
        return 0;
    }
    fragment::overlay::g_rbx_window = roblox_window;

    if (rbx::engine->initialize())
    {
        if (sync_globals())
        {
            log_globals();
            lighting::apply();
        }
        else
        {
            logger_core::log_warning("failed to populate globals after engine init");
        }

        cache::localplayer->start();
        cache::players_cache->start();
        cache::dead_bodies_cache->start();
        hacks::apply();
        std::thread([] { globals->mouse_service.cache_input_object(); }).detach();
        aimbot::start();
        free_aim::start();
        pf_silent::start();
        triggerbot::start();
        shooter::start();
        start_tp_watch();
        start_tests_loop();
    }
    else
    {
        logger_core::log_warning("rbx engine initialization failed - offsets likely invalid");
    }

    start_diag_loop();
    fragment::overlay::run_overlay();
    stop_diag_loop();
    stop_tests_loop();
    stop_tp_watch();
    hacks::stop();
    aimbot::stop();
    free_aim::stop();
    pf_silent::stop();
    triggerbot::stop();
    shooter::stop();
    lighting::stop();
    cache::localplayer->stop();
    cache::players_cache->stop();
    cache::dead_bodies_cache->stop();
    rbx::engine->shutdown();
    globals->reset();

    return 0;
}
