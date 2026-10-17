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

#include "../core/Console.hpp"
#include "../object/ObjectManager.h"
#include "../OpenRCT2.h"
#include "TitleScreen.h"
#include "TitleSequence.h"
#include "TitleSequenceManager.h"
#include "TitleSequencePlayer.h"

extern "C"
{
    #include "../audio/audio.h"
    #include "../config.h"
    #include "../drawing/drawing.h"
    #include "../game.h"
    #include "../input.h"
    #include "../interface/screenshot.h"
    #include "../interface/viewport.h"
    #include "../interface/window.h"
    #include "../localisation/localisation.h"
    #include "../peep/staff.h"
    #include "../world/climate.h"
    #include "../world/scenery.h"
}

extern "C"
{
    bool gTitleHideVersionInfo = false;
    uint16 gTitleCurrentSequence;
}

static uint16                   _loadedTitleSequenceId = UINT16_MAX;
static ITitleSequencePlayer *   _sequencePlayer = nullptr;

static void TitleInitialise();
static void TryLoadSequence();

/**
 *
 *  rct2: 0x00678680
 */
static void TitleInitialise()
{
    if (_sequencePlayer == nullptr)
    {
        _sequencePlayer = CreateTitleSequencePlayer();
    }
    // n3ds port: OpenRCT2's sequence whatever the settings say (user's decision 2026-10-17: only
    // light sequences, and the choice is not important). It is the only light one: its first park
    // has 162 objects, 7 MB. RCT2's opens Six Flags Magic Mountain (528 objects: 7 s on every
    // showing of the title, and a title objects pack of 30 MB that took minutes to write). The
    // RCT1 ones cannot play in 0.0.5: the player reads their .sc4 files as RCT2 files
    // (TitleSequencePlayer.cpp LoadParkFromFile) and fails on each, and on the 3DS a failed
    // 9 MB chunk allocation from such a read left the next load hanging
    size_t seqId = title_sequence_manager_get_index_for_config_id("*OPENRCT2");
    if (seqId == SIZE_MAX)
    {
        seqId = 0;
    }
    title_sequence_change_preset((int)seqId);
    TryLoadSequence();
}

static void TryLoadSequence()
{
    if (_loadedTitleSequenceId != gTitleCurrentSequence)
    {
        uint16 numSequences = (uint16)TitleSequenceManager::GetCount();
        if (numSequences > 0)
        {
            uint16 targetSequence = gTitleCurrentSequence;
            do
            {
                // n3ds port: this loads the sequence's first park at the start of the game (going
                // back to the menu does it in title_load). Its objects come from one file:
                // N3dsObjectPack.h. The parks that the sequence loads later, in title_update, are
                // loaded as before.
                gN3dsLoadingTitleObjects = true;
                bool begun = _sequencePlayer->Begin(targetSequence) && _sequencePlayer->Update();
                gN3dsLoadingTitleObjects = false;
                if (begun)
                {
                    _loadedTitleSequenceId = targetSequence;
                    gTitleCurrentSequence = targetSequence;
                    gfx_invalidate_screen();
                    return;
                }
                targetSequence = (targetSequence + 1) % numSequences;
            }
            while (targetSequence != gTitleCurrentSequence);
        }
        Console::Error::WriteLine("Unable to play any title sequences.");
        _sequencePlayer->Eject();
        gTitleCurrentSequence = UINT16_MAX;
        _loadedTitleSequenceId = UINT16_MAX;
    }
}

extern "C"
{
    /**
     *
     *  rct2: 0x0068E8DA
     */
    void title_load()
    {
        log_verbose("loading title");

        // n3ds port: coming back from a game the title's park is loaded below, which takes a
        // couple of seconds: show the loading box meanwhile. (When the program starts nothing
        // has been shown yet, and no box is drawn.) Not only when there is a sequence player
        // already: a game started straight from a park file comes here for the first time on
        // its way back to the menu, and had no box (user's report).
        platform_n3ds_loading_begin();
        uint16 loadedSequenceBefore = _loadedTitleSequenceId;

        if (gGamePaused & GAME_PAUSED_NORMAL)
            pause_toggle();

        gScreenFlags = SCREEN_FLAGS_TITLE_DEMO;

        reset_park_entrances();
        user_string_clear_all();
        reset_sprite_list();
        ride_init_all();
        window_guest_list_init_vars_a();
        staff_reset_modes();
        map_init(150);
        park_init();
        date_reset();
        climate_reset(CLIMATE_COOL_AND_WET);
        scenery_set_default_placement_configuration();
        window_new_ride_init_vars();
        window_guest_list_init_vars_b();
        window_staff_list_init_vars();
        map_update_tile_pointers();
        reset_sprite_spatial_index();
        audio_stop_all_music_and_sounds();
        viewport_init_all();
        news_item_init_queue();
        window_main_open();
        title_create_windows();
        TitleInitialise();
        gfx_invalidate_screen();
        gScreenAge = 0;

        if (_sequencePlayer != nullptr)
        {
            // n3ds port: the original only rewinds the sequence here, and its park is loaded by
            // the next update. A frame is drawn before that: the title's windows over an empty
            // map, which then stayed on both screens for the two seconds the 3DS takes to load
            // the park (user's report: a blank screen and a broken logo on returning to the
            // menu). So the sequence takes its first steps here, park included.
            // Not when TitleInitialise above has just loaded the sequence (at the start of the
            // game): it is at that point already, and rewinding it loaded the park twice.
            if (loadedSequenceBefore == _loadedTitleSequenceId)
            {
                _sequencePlayer->Reset();
                // The first park's objects from the pack, as in TryLoadSequence. (This is the
                // way back to the menu: it was left out at first, and the 73 objects that the
                // title needed after a game were read from their files, 1.8 s on a 3DS.)
                gN3dsLoadingTitleObjects = true;
                _sequencePlayer->Update();
                gN3dsLoadingTitleObjects = false;
            }
        }

        // n3ds port: the title music starts with the title screen, not while the loading box is
        // still up over the game that was left (user's request: as on the way from the title
        // into a scenario, the music stops, the box shows, and the new screen comes with its own).
        audio_start_title_music();

        log_verbose("loading title finished");
    }

    /**
     * Creates the windows shown on the title screen; New game, load game,
     * tutorial, toolbox and exit.
     *  rct2: 0x0066B5C0 (part of 0x0066B3E8)
     */
    void title_create_windows()
    {
        // n3ds port: first, so it stays behind the other title windows
        window_n3ds_title_background_open();
        window_title_menu_open();
        window_title_exit_open();
        window_title_options_open();
        window_title_logo_open();
        window_resize_gui(gScreenWidth, gScreenHeight);
        gTitleHideVersionInfo = false;
    }

    void title_update()
    {
        screenshot_check();
        title_handle_keyboard_input();

        if (game_is_not_paused())
        {
            TryLoadSequence();
            _sequencePlayer->Update();

            sint32 numUpdates = 1;
            if (gGameSpeed > 1) {
                numUpdates = 1 << (gGameSpeed - 1);
            }
            for (sint32 i = 0; i < numUpdates; i++)
            {
                game_logic_update();
            }
            update_palette_effects();
            // update_rain_animation();
        }

        gInputFlags &= ~INPUT_FLAG_VIEWPORT_SCROLLING;

        window_map_tooltip_update_visibility();
        window_dispatch_update_all();

        gSavedAge++;

        game_handle_input();
    }

    void DrawOpenRCT2(rct_drawpixelinfo * dpi, int x, int y)
    {
        utf8 buffer[256];

        // Write format codes
        utf8 *ch = buffer;
        ch = utf8_write_codepoint(ch, FORMAT_MEDIUMFONT);
        ch = utf8_write_codepoint(ch, FORMAT_OUTLINE);
        ch = utf8_write_codepoint(ch, FORMAT_WHITE);

        // Write name and version information
        openrct2_write_full_version_info(ch, sizeof(buffer) - (ch - buffer));
        gfx_draw_string(dpi, buffer, COLOUR_BLACK, x + 5, y + 5 - 13);

        // Write platform information
        snprintf(ch, 256 - (ch - buffer), "%s (%s)", OPENRCT2_PLATFORM, OPENRCT2_ARCHITECTURE);
        gfx_draw_string(dpi, buffer, COLOUR_BLACK, x + 5, y + 5);
    }

    void * title_get_sequence_player()
    {
        return _sequencePlayer;
    }

    void title_sequence_change_preset(int preset)
    {
        int count = (int)title_sequence_manager_get_count();
        if (preset < 0 || preset >= count) {
            return;
        }

        const utf8 * configId = title_sequence_manager_get_config_id(preset);
        SafeFree(gConfigInterface.current_title_sequence_preset);
        gConfigInterface.current_title_sequence_preset = _strdup(configId);

        gTitleCurrentSequence = preset;
        window_invalidate_all();
    }
}
