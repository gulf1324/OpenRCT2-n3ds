#pragma region Copyright (c) 2014-2016 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include <string>
#include "core/Guard.hpp"
#include "core/String.hpp"
#include "object/ObjectRepository.h"
#include "OpenRCT2.h"
#include "PlatformEnvironment.h"
#include "ride/TrackDesignRepository.h"
#include "scenario/ScenarioRepository.h"
#include "title/TitleScreen.h"
#include "title/TitleSequenceManager.h"

extern "C"
{
    #include "audio/audio.h"
    #include "config.h"
    #include "diagnostic.h"
    #include "editor.h"
    #include "game.h"
        #include "interface/themes.h"
    #include "intro.h"
    #include "localisation/localisation.h"
        #include "object_list.h"
    #include "platform/platform.h"
    #include "version.h"
}

// The game update inverval in milliseconds, (1000 / 40fps) = 25ms
constexpr uint32 UPDATE_TIME_MS = 25;

extern "C"
{
    int gExitCode;
    int gOpenRCT2StartupAction = STARTUP_ACTION_TITLE;
    utf8 gOpenRCT2StartupActionPath[512] = { 0 };
    utf8 gExePath[MAX_PATH];
    utf8 gCustomUserDataPath[MAX_PATH] = { 0 };
    utf8 gCustomOpenrctDataPath[MAX_PATH] = { 0 };
    utf8 gCustomRCT2DataPath[MAX_PATH] = { 0 };
    utf8 gCustomPassword[MAX_PATH] = { 0 };

    // This should probably be changed later and allow a custom selection of things to initialise like SDL_INIT
    bool gOpenRCT2Headless = false;

}

namespace OpenRCT2
{
    static IPlatformEnvironment * _env = nullptr;
    static std::string _versionInfo;
    static uint32 _lastTick;

    /** If set, will end the OpenRCT2 game loop. Intentially private to this module so that the flag can not be set back to false. */
    static bool _finished;

    static void SetupEnvironment();
    static void SetVersionInfoString();
    static void RunGameLoop();
    static void RunFixedFrame();
}

extern "C"
{
    /**
     * n3ds port: what is left of OpenRCT2's command line. 3dslink can pass arguments to the .3dsx
     * (scripts/send.cmd): a park file to open straight away, and --verbose. The HOME menu passes
     * none. The commands and options of the PC (edit, host, join, convert, screenshot and sprite
     * tools, data paths, headless) are gone with src/cmdline.
     */
    bool openrct2_read_arguments(int argc, const char * * argv)
    {
        for (int i = 1; i < argc; i++)
        {
            const char * argument = argv[i];
            if (strcmp(argument, "--verbose") == 0)
            {
                _log_levels[DIAGNOSTIC_LEVEL_VERBOSE] = 1;
            }
            else if (argument[0] == '-')
            {
                fprintf(stderr, "Unknown argument '%s'. Usage: openrct2 [<park file>] [--verbose]\n", argument);
                return false;
            }
            else
            {
                String::Set(gOpenRCT2StartupActionPath, sizeof(gOpenRCT2StartupActionPath), argument);
                gOpenRCT2StartupAction = STARTUP_ACTION_OPEN;
            }
        }
        return true;
    }

    void openrct2_write_full_version_info(utf8 * buffer, size_t bufferSize)
    {
        if (OpenRCT2::_versionInfo.empty())
        {
            OpenRCT2::SetVersionInfoString();
        }
        String::Set(buffer, bufferSize, OpenRCT2::_versionInfo.c_str());
    }

    static void openrct2_set_exe_path()
    {
        platform_get_exe_path(gExePath, sizeof(gExePath));
        log_verbose("Setting exe path to %s", gExePath);
    }

    bool openrct2_initialise()
    {
        utf8 userPath[MAX_PATH];
        platform_resolve_openrct_data_path();
        platform_resolve_user_data_path();
        platform_get_user_directory(userPath, NULL, sizeof(userPath));
        if (!platform_ensure_directory_exists(userPath))
        {
            log_fatal("Could not create user directory (do you have write access to your documents folder?)");
            return false;
        }

        openrct2_set_exe_path();

        config_set_defaults();
        if (!config_open_default())
        {
            if (!config_find_or_browse_install_directory())
            {
                config_save_default();
                utf8 path[MAX_PATH];
                config_get_default_path(path, sizeof(path));
                log_fatal("An RCT2 install directory must be specified! Please edit \"game_path\" in %s.", path);
                return false;
            }
            // n3ds port: a settings file that was just made is written once here. OpenRCT2 rewrote
            // it at every start to record the version last run (for its changelog window, removed).
            config_save_default();
        }

        // TODO add configuration option to allow multiple instances
        // if (!gOpenRCT2Headless && !platform_lock_single_instance()) {
        // 	log_fatal("OpenRCT2 is already running.");
        // 	return false;
        // }

        if (!rct2_init_directories())
        {
            return false;
        }
        if (!rct2_startup_checks())
        {
            return false;
        }

        // Sets up the environment OpenRCT2 is running in, e.g. directory paths
        OpenRCT2::SetupEnvironment();
        IObjectRepository * objRepo = CreateObjectRepository(OpenRCT2::_env);
        ITrackDesignRepository * tdRepo = CreateTrackDesignRepository(OpenRCT2::_env);
        CreateScenarioRepository(OpenRCT2::_env);

        if (!language_open(gConfigGeneral.language))
        {
            log_error("Failed to open configured language...");
            if (!language_open(LANGUAGE_ENGLISH_UK))
            {
                log_fatal("Failed to open fallback language...");
                return false;
            }
        }

        // n3ds port: where the start-up time goes, to shorten the black screen before the intro.
        // Each line of the log shows the time since the first one; the names say what is done.
        log_warning("n3ds start: settings, folders and language read");

        // TODO Ideally we want to delay this until we show the title so that we can
        //      still open the game window and draw a progress screen for the creation
        //      of the object cache.
        objRepo->LoadOrConstruct();
        log_warning("n3ds start: object index read");

        // TODO Like objects, this can take a while if there are a lot of track designs
        //      its also really something really we might want to do in the background
        //      as its not required until the player wants to place a new ride.
        tdRepo->Scan();
        log_warning("n3ds start: track designs scanned");

        TitleSequenceManager::Scan();
        log_warning("n3ds start: title sequences scanned");

        if (!gOpenRCT2Headless)
        {
            // n3ds port: only the sound effects' file is read here. rct2_init makes the effects
            // out of it, and opens the sound device once the game is registered with the system:
            // until then the HOME menu shows its start-up logo and the sound hardware is its
            // own (n3ds.c __appInit, Mixer::N3dsReadEffectsFile).
            audio_n3ds_read_effects_file();
        }
        log_warning("n3ds start: sound effects file read");

        theme_manager_initialise();

        if (!rct2_init())
        {
            return false;
        }

        rct2_copy_original_user_files_over();
        return true;
    }

    /**
     * Launches the game, after command line arguments have been parsed and processed.
     */
    void openrct2_launch()
    {
        if (openrct2_initialise())
        {
            gIntroState = INTRO_STATE_NONE;
            if ((gOpenRCT2StartupAction == STARTUP_ACTION_TITLE) && gConfigGeneral.play_intro)
            {
                gOpenRCT2StartupAction = STARTUP_ACTION_INTRO;
            }

            switch (gOpenRCT2StartupAction) {
            case STARTUP_ACTION_INTRO:
                gIntroState = INTRO_STATE_PUBLISHER_BEGIN;
                title_load();
                break;
            case STARTUP_ACTION_TITLE:
                title_load();
                break;
            case STARTUP_ACTION_OPEN:
                if (!rct2_open_file(gOpenRCT2StartupActionPath))
                {
                    fprintf(stderr, "Failed to load '%s'", gOpenRCT2StartupActionPath);
                    title_load();
                    break;
                }

                gScreenFlags = SCREEN_FLAGS_PLAYING;

                break;
            }

            OpenRCT2::RunGameLoop();
        }
        openrct2_dispose();

        // HACK Some threads are still running which causes the game to not terminate. Investigation required!
        exit(gExitCode);
    }

    void openrct2_dispose()
    {
        language_close_all();
        rct2_dispose();
        config_release();
        platform_free();
    }

    /**
     * Causes the OpenRCT2 game loop to finish.
     */
    void openrct2_finish()
    {
        OpenRCT2::_finished = true;
    }
}

namespace OpenRCT2
{
    static void SetupEnvironment()
    {
        utf8 path[260];
        std::string basePaths[4];
        basePaths[(size_t)DIRBASE::RCT2] = std::string(gRCT2AddressAppPath);
        platform_get_openrct_data_path(path, sizeof(path));
        basePaths[(size_t)DIRBASE::OPENRCT2] = std::string(path);
        platform_get_user_directory(path, nullptr, sizeof(path));
        basePaths[(size_t)DIRBASE::USER] = std::string(path);
        OpenRCT2::_env = CreatePlatformEnvironment(basePaths);
    }

    static void SetVersionInfoString()
    {
        utf8 buffer[256];
        size_t bufferSize = sizeof(buffer);
        String::Set(buffer, bufferSize, OPENRCT2_NAME ", v" OPENRCT2_VERSION);
        if (!String::IsNullOrEmpty(gGitBranch))
        {
            String::AppendFormat(buffer, bufferSize, "-%s", gGitBranch);
        }
        if (!String::IsNullOrEmpty(gCommitSha1Short))
        {
            String::AppendFormat(buffer, bufferSize, " build %s", gCommitSha1Short);
        }
        if (!String::IsNullOrEmpty(gBuildServer))
        {
            String::AppendFormat(buffer, bufferSize, " provided by %s", gBuildServer);
        }
    #if DEBUG
        String::AppendFormat(buffer, bufferSize, " (DEBUG)", gBuildServer);
    #endif
        _versionInfo = buffer;
    }

    /**
     * Run the main game loop until the finished flag is set.
     */
    static void RunGameLoop()
    {
        log_verbose("begin openrct2 loop");
        _finished = false;
        do
        {
            RunFixedFrame();
        }
        while (!_finished);
        log_verbose("finish openrct2 loop");
    }

    static void RunFixedFrame()
    {
        uint32 currentTick = SDL_GetTicks();
        uint32 ticksElapsed = currentTick - _lastTick;
        if (ticksElapsed < UPDATE_TIME_MS)
        {
            SDL_Delay(UPDATE_TIME_MS - ticksElapsed);
            _lastTick += UPDATE_TIME_MS;
        }
        else
        {
            _lastTick = currentTick;
        }
        // n3ds port: performance log (n3ds.c)
        uint64 perfBegin = platform_n3ds_perf_begin();
        N3DS_PERF(N3DS_PERF_EVENTS, platform_process_messages());
        rct2_update();
        platform_n3ds_perf_end(N3DS_PERF_UPDATE, perfBegin);
        platform_draw();
        platform_n3ds_perf_frame();
    }
}
