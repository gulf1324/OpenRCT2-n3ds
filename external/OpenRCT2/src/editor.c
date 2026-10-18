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

#include "audio/audio.h"
#include "drawing/drawing.h"
#include "editor.h"
#include "game.h"
#include "interface/window.h"
#include "interface/viewport.h"
#include "localisation/date.h"
#include "localisation/localisation.h"
#include "management/finance.h"
#include "management/news_item.h"
#include "object.h"
#include "object/ObjectManager.h"
#include "peep/staff.h"
#include "platform/platform.h"
#include "rct1.h"
#include "ride/ride.h"
#include "scenario/scenario.h"
#include "util/sawyercoding.h"
#include "util/util.h"
#include "world/banner.h"
#include "world/climate.h"
#include "world/footpath.h"
#include "world/map.h"
#include "world/park.h"
#include "world/scenery.h"
#include "world/sprite.h"

uint8 _editorSelectedRides[128];
uint8 _editorSelectedSmallScenery[252];
uint8 _editorSelectedLargeScenery[128];
uint8 _editorSelectedWalls[128];
uint8 _editorSelectedBanners[32];
uint8 _editorSelectedFootpaths[16];
uint8 _editorSelectedFootpathAdditions[15];
uint8 _editorSelectedSceneryGroups[19];
uint8 _editorSelectedParkEntrances[1];
uint8 _editorSelectedWaters[1];
uint8 _editorSelectedStexs[1];

uint8 * gEditorSelectedObjects[OBJECT_ENTRY_GROUP_COUNT] = {
	_editorSelectedRides,
	_editorSelectedSmallScenery,
	_editorSelectedLargeScenery,
	_editorSelectedWalls,
	_editorSelectedBanners,
	_editorSelectedFootpaths,
	_editorSelectedFootpathAdditions,
	_editorSelectedSceneryGroups,
	_editorSelectedParkEntrances,
	_editorSelectedWaters,
	_editorSelectedStexs,
};

static void set_all_land_owned();

/**
 *
 *  rct2: 0x00672957
 */
void trackdesigner_load()
{
	rct_window *mainWindow;

	audio_stop_all_music_and_sounds();
	gScreenFlags = SCREEN_FLAGS_TRACK_DESIGNER;
	gScreenAge = 0;

	object_manager_unload_all_objects();
	object_list_load();
	map_init(150);
	set_all_land_owned();
	banner_init();
	reset_park_entrances();
	user_string_clear_all();
	reset_sprite_list();
	ride_init_all();
	window_guest_list_init_vars_a();
	staff_reset_modes();
	park_init();
	finance_init();
	date_reset();
	window_guest_list_init_vars_b();
	window_staff_list_init_vars();
	gS6Info.editor_step = EDITOR_STEP_OBJECT_SELECTION;
	window_new_ride_init_vars();
	viewport_init_all();
	news_item_init_queue();
	window_editor_main_open();
	mainWindow = window_get_main();
	window_scroll_to_location(mainWindow, 2400, 2400, 112);
	mainWindow->flags &= ~WF_SCROLLING_TO_LOCATION;
	load_palette();
	gfx_invalidate_screen();
}

/**
 *
 *  rct2: 0x006729FD
 */
void trackmanager_load()
{
	rct_window *mainWindow;

	audio_stop_all_music_and_sounds();
	gScreenFlags = SCREEN_FLAGS_TRACK_MANAGER;
	gScreenAge = 0;

	object_manager_unload_all_objects();
	object_list_load();
	map_init(150);
	set_all_land_owned();
	banner_init();
	reset_park_entrances();
	user_string_clear_all();
	reset_sprite_list();
	ride_init_all();
	window_guest_list_init_vars_a();
	staff_reset_modes();
	park_init();
	finance_init();
	date_reset();
	window_guest_list_init_vars_b();
	window_staff_list_init_vars();
	gS6Info.editor_step = EDITOR_STEP_OBJECT_SELECTION;
	window_new_ride_init_vars();
	viewport_init_all();
	news_item_init_queue();
	window_editor_main_open();
	mainWindow = window_get_main();
	window_scroll_to_location(mainWindow, 2400, 2400, 112);
	mainWindow->flags &= ~WF_SCROLLING_TO_LOCATION;
	load_palette();
	gfx_invalidate_screen();
}

/**
 *
 *  rct2: 0x0068ABEC
 */
static void set_all_land_owned()
{
	int mapSize = gMapSize;

	game_do_command(64, 1, 64, 2, GAME_COMMAND_SET_LAND_OWNERSHIP, (mapSize - 3) * 32, (mapSize - 3) * 32);
}

/**
 *
 *  rct2: 0x0067009A
 */
void editor_open_windows_for_current_step()
{
	if (!(gScreenFlags & SCREEN_FLAGS_EDITOR))
		return;

	switch (gS6Info.editor_step) {
	case EDITOR_STEP_OBJECT_SELECTION:
		if (window_find_by_class(WC_EDITOR_OBJECT_SELECTION))
			return;

		if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER) {
			object_manager_unload_all_objects();
		}

		window_editor_object_selection_open();
		break;
	}
}

/**
 *
 *  rct2: 0x006AB9B8
 */
int editor_check_object_selection()
{
	// n3ds port: the designer and the manager only (the scenario editor, which also needed a path, a park
	// entrance and a water object, is not in this build)
	if (!editor_check_object_group_at_least_one_selected(OBJECT_TYPE_RIDE)) {
		gGameCommandErrorText = STR_AT_LEAST_ONE_RIDE_OBJECT_MUST_BE_SELECTED;
		return OBJECT_TYPE_RIDE;
	}

	return -1;
}
