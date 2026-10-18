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

#include "../audio/audio.h"
#include "../config.h"
#include "../game.h"
#include "../editor.h"
#include "../interface/themes.h"
#include "../interface/widget.h"
#include "../interface/window.h"
#include "../localisation/localisation.h"
#include "../management/research.h"
#include "../object.h"
#include "../object_list.h"
#include "../object/ObjectManager.h"
#include "../object/ObjectRepository.h"
#include "../rct1.h"
#include "../ride/ride.h"
#include "../ride/ride_data.h"
#include "../ride/track.h"
#include "../ride/track_data.h"
#include "../ride/track_design.h"
#include "../scenario/scenario.h"
#include "../util/util.h"
#include "../world/footpath.h"
#include "dropdown.h"
#include "error.h"
#include "../sprites.h"

enum {
	FILTER_RCT2 = (1 << 0),
	FILTER_WW = (1 << 1),
	FILTER_TT = (1 << 2),
	FILTER_CUSTOM = (1 << 3),

	FILTER_RIDE_TRANSPORT = (1 << 5),
	FILTER_RIDE_GENTLE = (1 << 6),
	FILTER_RIDE_COASTER = (1 << 7),
	FILTER_RIDE_THRILL = (1 << 8),
	FILTER_RIDE_WATER = (1 << 9),
	FILTER_RIDE_STALL = (1 << 10),

	FILTER_SELECTED = (1 << 12),
	FILTER_NONSELECTED = (1 << 13),

	FILTER_ALL = 0x7EF,
} FILTER_FLAGS;

uint32 _filter_flags;
uint16 _filter_object_counts[11];
uint8 _filter_ride_tab;

#define _FILTER_ALL ((_filter_flags & FILTER_ALL) == FILTER_ALL)
#define _FILTER_RCT2 (_filter_flags & FILTER_RCT2)
#define _FILTER_WW (_filter_flags & FILTER_WW)
#define _FILTER_TT (_filter_flags & FILTER_TT)
#define _FILTER_CUSTOM (_filter_flags & FILTER_CUSTOM)
#define _FILTER_SELECTED (_filter_flags & FILTER_SELECTED)
#define _FILTER_NONSELECTED (_filter_flags & FILTER_NONSELECTED)

enum {
	WINDOW_OBJECT_SELECTION_PAGE_RIDE_VEHICLES_ATTRACTIONS,
	WINDOW_OBJECT_SELECTION_PAGE_SMALL_SCENERY,
	WINDOW_OBJECT_SELECTION_PAGE_LARGE_SCENERY,
	WINDOW_OBJECT_SELECTION_PAGE_WALLS_FENCES,
	WINDOW_OBJECT_SELECTION_PAGE_PATH_SIGNS,
	WINDOW_OBJECT_SELECTION_PAGE_FOOTPATHS,
	WINDOW_OBJECT_SELECTION_PAGE_PATH_EXTRAS,
	WINDOW_OBJECT_SELECTION_PAGE_SCENERY_GROUPS,
	WINDOW_OBJECT_SELECTION_PAGE_PARK_ENTRANCE,
	WINDOW_OBJECT_SELECTION_PAGE_WATER,
	WINDOW_OBJECT_SELECTION_PAGE_SCENARIO_DESCRIPTION,
	WINDOW_OBJECT_SELECTION_PAGE_COUNT
};

#pragma region Widgets

enum WINDOW_STAFF_LIST_WIDGET_IDX {
	WIDX_BACKGROUND,			// 0, 1
	WIDX_TITLE,					// 1, 2
	WIDX_CLOSE,					// 2, 4
	WIDX_TAB_CONTENT_PANEL,		// 3, 8
	WIDX_LIST,					// 4, 10
	WIDX_PREVIEW,				// 5, 20
	WIDX_FILTER_DROPDOWN,		// 6, 40
	WIDX_FILTER_RIDE_TAB_FRAME,
	WIDX_FILTER_RIDE_TAB_ALL,
	WIDX_FILTER_RIDE_TAB_TRANSPORT,
	WIDX_FILTER_RIDE_TAB_GENTLE,
	WIDX_FILTER_RIDE_TAB_COASTER,
	WIDX_FILTER_RIDE_TAB_THRILL,
	WIDX_FILTER_RIDE_TAB_WATER,
	WIDX_FILTER_RIDE_TAB_STALL,
};

// n3ds port: the window is one page of the bottom screen (320x240), laid out for ride objects
// alone: the designer and the manager, the only ways here, select nothing else, so the page
// tabs are gone. The ride category tabs are at the top with the filter dropdown beside them,
// the list (names, N3DS_LIST_ROW-px rows) on the left, the preview at half size with the
// object's name, ride type, source and description on the right. The content ends at
// N3DS_CONTENT_BOTTOM: the editor's bottom toolbar is over the rest of the page
#define N3DS_LIST_ROW        16
#define N3DS_CONTENT_BOTTOM  205
#define N3DS_COUNT_Y         195
#define N3DS_COLUMN_X        180
#define N3DS_COLUMN_WIDTH    136
#define N3DS_PREVIEW_FULL    112

static rct_widget window_editor_object_selection_widgets[] = {
	{ WWT_FRAME,			0,	0,		319,	0,		239,	0xFFFFFFFF,						STR_NONE },
	{ WWT_CAPTION,			0,	1,		318,	1,		14,		STR_OBJECT_SELECTION,			STR_WINDOW_TITLE_TIP },
	{ WWT_CLOSEBOX,			0,	307,	317,	2,		13,		STR_CLOSE_X,					STR_CLOSE_WINDOW_TIP },
	{ WWT_RESIZE,			1,	0,		319,	17,		239,	0xFFFFFFFF,						STR_NONE },
	{ WWT_SCROLL,			1,	4,		175,	50,		193,	SCROLL_VERTICAL,				STR_NONE },
	{ WWT_FLATBTN,			1,	219,	276,	50,		107,	0xFFFFFFFF,						STR_NONE },
	{ WWT_DROPDOWN_BUTTON,	0,	226,	315,	22,		39,		STR_OBJECT_FILTER,				STR_OBJECT_FILTER_TIP },
	{ WWT_IMGBTN,			1,	3,		316,	44,		47,		0xFFFFFFFF,						STR_NONE },
	{ WWT_TAB,				1,	3,		33,		18,		44,		0x20000000 | SPR_TAB,			STR_OBJECT_FILTER_ALL_RIDES_TIP },
	{ WWT_TAB,				1,	34,		64,		18,		44,		0x20000000 | SPR_TAB,			STR_TRANSPORT_RIDES_TIP },
	{ WWT_TAB,				1,	65,		95,		18,		44,		0x20000000 | SPR_TAB,			STR_GENTLE_RIDES_TIP },
	{ WWT_TAB,				1,	96,		126,	18,		44,		0x20000000 | SPR_TAB,			STR_ROLLER_COASTERS_TIP },
	{ WWT_TAB,				1,	127,	157,	18,		44,		0x20000000 | SPR_TAB,			STR_THRILL_RIDES_TIP },
	{ WWT_TAB,				1,	158,	188,	18,		44,		0x20000000 | SPR_TAB,			STR_WATER_RIDES_TIP },
	{ WWT_TAB,				1,	189,	219,	18,		44,		0x20000000 | SPR_TAB,			STR_SHOPS_STALLS_TIP },
	{ WIDGETS_END }
};

#pragma endregion

#pragma region Events

static void window_editor_object_selection_close(rct_window *w);
static void window_editor_object_selection_mouseup(rct_window *w, int widgetIndex);
static void window_editor_object_selection_mousedown(int widgetIndex, rct_window*w, rct_widget* widget);
static void window_editor_object_selection_dropdown(rct_window *w, int widgetIndex, int dropdownIndex);
static void window_editor_object_selection_update(rct_window *w);
static void window_editor_object_selection_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height);
static void window_editor_object_selection_scroll_mousedown(rct_window *w, int scrollIndex, int x, int y);
static void window_editor_object_selection_scroll_mouseover(rct_window *w, int scrollIndex, int x, int y);
static void window_editor_object_selection_invalidate(rct_window *w);
static void window_editor_object_selection_paint(rct_window *w, rct_drawpixelinfo *dpi);
static void window_editor_object_selection_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex);

static rct_window_event_list window_editor_object_selection_events = {
	window_editor_object_selection_close,
	window_editor_object_selection_mouseup,
	NULL,
	window_editor_object_selection_mousedown,
	window_editor_object_selection_dropdown,
	NULL,
	window_editor_object_selection_update,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	window_editor_object_selection_scrollgetsize,
	window_editor_object_selection_scroll_mousedown,
	NULL,
	window_editor_object_selection_scroll_mouseover,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	window_editor_object_selection_invalidate,
	window_editor_object_selection_paint,
	window_editor_object_selection_scrollpaint
};

#pragma endregion

const int window_editor_object_selection_animation_loops[] = { 20, 32, 10, 72, 24, 28, 16 };
const int window_editor_object_selection_animation_divisor[] = { 4, 8, 2, 4, 4, 4, 2 };

static int window_editor_object_selection_select_object(uint8 bh, int flags, const rct_object_entry *entry);
static int get_object_from_object_selection(uint8 object_type, int y);
static void window_editor_object_selection_manage_tracks();
static void editor_load_selected_objects();
static bool filter_selected(uint8 objectFlags);
static bool filter_source(const ObjectRepositoryItem * item);
static bool filter_chunks(const ObjectRepositoryItem * item);
static void filter_update_counts();

void reset_selected_object_count_and_size();
static int sub_6AB211();

enum {
	DDIX_FILTER_RCT2,
	DDIX_FILTER_WW,
	DDIX_FILTER_TT,
	DDIX_FILTER_CUSTOM,
	DDIX_FILTER_SEPERATOR,
	DDIX_FILTER_SELECTED,
	DDIX_FILTER_NONSELECTED,
};

typedef struct list_item {
	const ObjectRepositoryItem * repositoryItem;
	rct_object_entry *entry;
	rct_object_filters *filter;
	uint8 *flags;
} list_item;

static rct_string_id get_ride_type_string_id(const ObjectRepositoryItem * item);

static int _numListItems = 0;
static list_item *_listItems = NULL;
static void * _loadedObject = NULL;
static uint8 * _objectSelectionFlags = NULL;
static int _numSelectedObjectsForType[11];
static int _numAvailableObjectsForType[11];
static bool _maxObjectsWasHit;

static void visible_list_dispose()
{
	SafeFree(_listItems);
	_numListItems = 0;
}

static int visible_list_sort_ride_name(const void *rawA, const void *rawB)
{
	list_item *a = (list_item*)rawA;
	list_item *b = (list_item*)rawB;

	const char *nameA = a->repositoryItem->Name;
	const char *nameB = b->repositoryItem->Name;
	return strcmp(nameA, nameB);
}

static int visible_list_sort_ride_type(const void *rawA, const void *rawB)
{
	list_item *a = (list_item*)rawA;
	list_item *b = (list_item*)rawB;

	const char *rideTypeA = language_get_string(get_ride_type_string_id(a->repositoryItem));
	const char *rideTypeB = language_get_string(get_ride_type_string_id(b->repositoryItem));
	int result = strcmp(rideTypeA, rideTypeB);
	if (result != 0)
		return result;

	return visible_list_sort_ride_name(rawA, rawB);
}

static void visible_list_refresh(rct_window *w)
{
	int numObjects = (int)object_repository_get_items_count();

	visible_list_dispose();
	w->selected_list_item = -1;
	_listItems = malloc(numObjects * sizeof(list_item));
	_numListItems = 0;

	list_item *currentListItem = &_listItems[0];
	const ObjectRepositoryItem *items = object_repository_get_items();
	for (int i = 0; i < numObjects; i++) {
		uint8 selectionFlags = _objectSelectionFlags[i];
		const ObjectRepositoryItem * item = &items[i];
		uint8 objectType = item->ObjectEntry.flags & 0x0F;
		if (objectType == w->selected_tab && !(selectionFlags & OBJECT_SELECTION_FLAG_6) &&
			filter_source(item) &&
			filter_chunks(item) &&
			filter_selected(selectionFlags)
		) {
			rct_object_filters * filter = calloc(1, sizeof(rct_object_filters));
			currentListItem->repositoryItem = item;
			currentListItem->entry = (rct_object_entry *)&item->ObjectEntry;
			currentListItem->filter = filter;
			currentListItem->flags = &_objectSelectionFlags[i];
			currentListItem++;
			_numListItems++;
		}
	}

	_listItems = realloc(_listItems, _numListItems * sizeof(list_item));

	// n3ds port: one order, without the sort buttons: rides by type and then by name, the
	// other pages by name
	bool ridePage = (w->selected_tab == WINDOW_OBJECT_SELECTION_PAGE_RIDE_VEHICLES_ATTRACTIONS);
	qsort(_listItems, _numListItems, sizeof(list_item), ridePage ? visible_list_sort_ride_type : visible_list_sort_ride_name);

	window_invalidate(w);
}

/**
 *
 *  rct2: 0x006AA64E
 */
void window_editor_object_selection_open()
{
	rct_window* window;

	window = window_bring_to_front_by_class(WC_EDITOR_OBJECT_SELECTION);
	if (window != NULL)
		return;

	if (!sub_6AB211())
		return;
	reset_selected_object_count_and_size();

	// n3ds port: one page of the bottom screen, not resizable
	window = window_create(
		0,
		0,
		320,
		240,
		&window_editor_object_selection_events,
		WC_EDITOR_OBJECT_SELECTION,
		WF_10
	);
	window->widgets = window_editor_object_selection_widgets;

	window->enabled_widgets =
		(1 << WIDX_FILTER_DROPDOWN) |
		(1 << WIDX_CLOSE);

	_filter_flags = gConfigInterface.object_selection_filter_flags;

	for (int i = WIDX_FILTER_RIDE_TAB_ALL; i <= WIDX_FILTER_RIDE_TAB_STALL; i++)
		window->enabled_widgets |= (1LL << i);
	window_init_scroll_widgets(window);

	window->var_4AE = 0;
	window->selected_tab = 0;
	window->selected_list_item = -1;
	window->object_entry = (rct_object_entry *)-1;

	visible_list_refresh(window);
}

/**
 *
 *  rct2: 0x006ABCD1
 */
static void setup_track_manager_objects()
{
	int numObjects = (int)object_repository_get_items_count();
	const ObjectRepositoryItem * items = object_repository_get_items();
	for (int i = 0; i < numObjects; i++) {
		uint8 * selectionFlags = &_objectSelectionFlags[i];
		const ObjectRepositoryItem * item = &items[i];
		uint8 object_type = item->ObjectEntry.flags & 0xF;
		if (object_type == OBJECT_TYPE_RIDE) {
			*selectionFlags |= OBJECT_SELECTION_FLAG_6;

			for (uint8 j = 0; j < 3; j++) {
				uint8 rideType = item->RideType[j];
				if (rideType != 0xFF && ride_type_has_flag(rideType, RIDE_TYPE_FLAG_HAS_TRACK)) {
					*selectionFlags &= ~OBJECT_SELECTION_FLAG_6;
					break;
				}
			}
		}
	}
}

/**
 *
 *  rct2: 0x006ABC1E
 */
static void setup_track_designer_objects()
{
	int numObjects = (int)object_repository_get_items_count();
	const ObjectRepositoryItem * items = object_repository_get_items();
	for (int i = 0; i < numObjects; i++) {
		uint8 * selectionFlags = &_objectSelectionFlags[i];
		const ObjectRepositoryItem * item = &items[i];
		uint8 objectType = item->ObjectEntry.flags & 0xF;
		if (objectType == OBJECT_TYPE_RIDE){
			*selectionFlags |= OBJECT_SELECTION_FLAG_6;

			for (uint8 j = 0; j < 3; j++) {
				uint8 rideType = item->RideType[j];
				if (rideType != 0xFF) {
					if (RideData4[rideType].flags & RIDE_TYPE_FLAG4_11) {
						*selectionFlags &= ~OBJECT_SELECTION_FLAG_6;
						break;
					}
				}
			}
		}
	}
}

/**
 *
 *  rct2: 0x006AA82B
 */
static void setup_in_use_selection_flags()
{
	for (uint8 object_type = 0; object_type < 11; object_type++){
		for (uint16 i = 0; i < object_entry_group_counts[object_type]; i++){
			gEditorSelectedObjects[object_type][i] = 0;
		}
	}

	for (uint8 object_type = 0; object_type < 11; object_type++){
		for (uint16 i = 0; i < object_entry_group_counts[object_type]; i++){
			if (object_entry_groups[object_type].chunks[i] != (uint8*)-1) {
				gEditorSelectedObjects[object_type][i] |= (1 << 1);
			}
		}
	}

	map_element_iterator iter;
	map_element_iterator_begin(&iter);
	do {
		uint16 type;
		rct_banner* banner;

		switch (map_element_get_type(iter.element)) {
		default:
		case MAP_ELEMENT_TYPE_SURFACE:
		case MAP_ELEMENT_TYPE_TRACK:
			break;
		case MAP_ELEMENT_TYPE_PATH:
			type = iter.element->properties.path.type;
			type >>= 4;
			assert(type < object_entry_group_counts[OBJECT_TYPE_PATHS]);
			gEditorSelectedObjects[OBJECT_TYPE_PATHS][type] |= (1 << 0);

			if (footpath_element_has_path_scenery(iter.element)) {
				uint8 path_additions = footpath_element_get_path_scenery_index(iter.element);
				gEditorSelectedObjects[OBJECT_TYPE_PATH_BITS][path_additions] |= 1;
			}
			break;
		case MAP_ELEMENT_TYPE_SCENERY:
			type = iter.element->properties.scenery.type;
			assert(type < object_entry_group_counts[OBJECT_TYPE_SMALL_SCENERY]);
			gEditorSelectedObjects[OBJECT_TYPE_SMALL_SCENERY][type] |= (1 << 0);
			break;
		case MAP_ELEMENT_TYPE_ENTRANCE:
			if (iter.element->properties.entrance.type != ENTRANCE_TYPE_PARK_ENTRANCE)
				break;

			gEditorSelectedObjects[OBJECT_TYPE_PARK_ENTRANCE][0] |= (1 << 0);

			type = iter.element->properties.entrance.path_type;
			assert(type < object_entry_group_counts[OBJECT_TYPE_PATHS]);
			gEditorSelectedObjects[OBJECT_TYPE_PATHS][type] |= (1 << 0);
			break;
		case MAP_ELEMENT_TYPE_FENCE:
			type = iter.element->properties.fence.type;
			assert(type < object_entry_group_counts[OBJECT_TYPE_WALLS]);
			gEditorSelectedObjects[OBJECT_TYPE_WALLS][type] |= (1 << 0);
			break;
		case MAP_ELEMENT_TYPE_SCENERY_MULTIPLE:
			type = iter.element->properties.scenerymultiple.type & 0x3FF;
			assert(type < object_entry_group_counts[OBJECT_TYPE_LARGE_SCENERY]);
			gEditorSelectedObjects[OBJECT_TYPE_LARGE_SCENERY][type] |= (1 << 0);
			break;
		case MAP_ELEMENT_TYPE_BANNER:
			banner = &gBanners[iter.element->properties.banner.index];
			type = banner->type;
			assert(type < object_entry_group_counts[OBJECT_TYPE_BANNERS]);
			gEditorSelectedObjects[OBJECT_TYPE_BANNERS][type] |= (1 << 0);
			break;
		}
	} while (map_element_iterator_next(&iter));

	for (uint8 ride_index = 0; ride_index < 0xFF; ride_index++) {
		rct_ride* ride = get_ride(ride_index);
		if (ride->type != RIDE_TYPE_NULL) {
			uint8 type = ride->subtype;
			gEditorSelectedObjects[OBJECT_TYPE_RIDE][type] |= (1 << 0);
		}
	}

	int numObjects = (int)object_repository_get_items_count();
	const ObjectRepositoryItem * items = object_repository_get_items();
	for (int i = 0; i < numObjects; i++) {
		uint8 *selectionFlags = &_objectSelectionFlags[i];
		const ObjectRepositoryItem * item = &items[i];
		*selectionFlags &= ~OBJECT_SELECTION_FLAG_IN_USE;

		uint8 entryType, entryIndex;
		if (find_object_in_entry_group(&item->ObjectEntry, &entryType, &entryIndex)) {
			if (gEditorSelectedObjects[entryType][entryIndex] & (1 << 0)) {
				*selectionFlags |=
					OBJECT_SELECTION_FLAG_IN_USE |
					OBJECT_SELECTION_FLAG_SELECTED;
			}
			if (gEditorSelectedObjects[entryType][entryIndex] & (1 << 1)) {
				*selectionFlags |= OBJECT_SELECTION_FLAG_SELECTED;
			}
		}
	}
}

/**
 *
 *  rct2: 0x006AB211
 */
static int sub_6AB211()
{
	int numObjects = (int)object_repository_get_items_count();
	_objectSelectionFlags = (uint8*)calloc(numObjects, sizeof(uint8));
	if (_objectSelectionFlags == NULL){
		log_error("Failed to allocate memory for object flag list.");
		return 0;
	}

	for (uint8 objectType = 0; objectType < 11; objectType++) {
		_numSelectedObjectsForType[objectType] = 0;
		_numAvailableObjectsForType[objectType] = 0;
	}

	const ObjectRepositoryItem * items = object_repository_get_items();
	for (int i = 0; i < numObjects; i++) {
		uint8 objectType = items[i].ObjectEntry.flags & 0xF;
		_numAvailableObjectsForType[objectType]++;
	}

	if (gScreenFlags & SCREEN_FLAGS_TRACK_DESIGNER) {
		setup_track_designer_objects();
	}

	if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER) {
		setup_track_manager_objects();
	}

	setup_in_use_selection_flags();
	reset_selected_object_count_and_size();

	return 1;
}

/**
 *
 *  rct2: 0x006AB316
 */
static void editor_object_flags_free()
{
	SafeFree(_objectSelectionFlags);
}

/**
 *
 *  rct2: 0x00685791
 */
static void remove_selected_objects_from_research(const rct_object_entry* installedObject){
	uint8 entry_type, entry_index;
	if (!find_object_in_entry_group(installedObject, &entry_type, &entry_index))
		return;

	if (entry_type == OBJECT_TYPE_RIDE){
		rct_ride_entry* rideEntry = (rct_ride_entry*)object_entry_groups[entry_type].chunks[entry_index];
		research_remove(entry_index | rideEntry->ride_type[0] << 8 | 0x10000);
		research_remove(entry_index | rideEntry->ride_type[1] << 8 | 0x10000);
		research_remove(entry_index | rideEntry->ride_type[2] << 8 | 0x10000);
	}
	else if (entry_type == OBJECT_TYPE_SCENERY_SETS){
		research_remove(entry_index);
	}
}

/**
 *
 *  rct2: 0x006ABB66
 */
static void unload_unselected_objects()
{
	int numItems = (int)object_repository_get_items_count();
	const ObjectRepositoryItem * items = object_repository_get_items();

	size_t numObjectsToUnload = 0;
	rct_object_entry * objectsToUnload = (rct_object_entry *)malloc(numItems * sizeof(rct_object_entry));

	for (int i = 0; i < numItems; i++) {
		if (!(_objectSelectionFlags[i] & OBJECT_SELECTION_FLAG_SELECTED)) {
			const rct_object_entry * entry = &items[i].ObjectEntry;

			remove_selected_objects_from_research(entry);
			objectsToUnload[numObjectsToUnload++] = *entry;
		}
	}

	object_manager_unload_objects(objectsToUnload, numObjectsToUnload);
	free(objectsToUnload);
}

/**
 *
 *  rct2: 0x006AB199
 */
static void window_editor_object_selection_close(rct_window *w)
{
	//if (!(gScreenFlags & SCREEN_FLAGS_EDITOR))
	//	return;

	unload_unselected_objects();
	editor_load_selected_objects();
	editor_object_flags_free();

	object_delete(_loadedObject);
	_loadedObject = NULL;

	if (gScreenFlags & SCREEN_FLAGS_EDITOR) {
		research_populate_list_random();
	}
	else {
		// Used for in-game object selection cheat
		// This resets the ride selection list and resets research to 0 on current item
		gSilentResearch = true;
		sub_684AC3();
		gSilentResearch = false;
	}
	research_remove_non_separate_vehicle_types();
	window_new_ride_init_vars();

	visible_list_dispose();
}

/**
 *
 *  rct2: 0x006AAFAB
 */
static void window_editor_object_selection_mouseup(rct_window *w, int widgetIndex)
{
	switch (widgetIndex) {
	case WIDX_CLOSE:
		if (gScreenFlags & SCREEN_FLAGS_EDITOR) {
			game_do_command(0, 1, 0, 0, GAME_COMMAND_LOAD_OR_QUIT, 1, 0);
		}
		else {
			// Used for in-game object selection cheat
			window_close(w);
		}
		break;

	case WIDX_FILTER_RIDE_TAB_ALL:
		_filter_flags |= 0x7E0;
		gConfigInterface.object_selection_filter_flags = _filter_flags;
		config_save_default();

		filter_update_counts();
		visible_list_refresh(w);

		w->selected_list_item = -1;
		w->object_entry = (rct_object_entry *)-1;
		w->scrolls[0].v_top = 0;
		window_invalidate(w);
		break;
	case WIDX_FILTER_RIDE_TAB_TRANSPORT:
	case WIDX_FILTER_RIDE_TAB_GENTLE:
	case WIDX_FILTER_RIDE_TAB_COASTER:
	case WIDX_FILTER_RIDE_TAB_THRILL:
	case WIDX_FILTER_RIDE_TAB_WATER:
	case WIDX_FILTER_RIDE_TAB_STALL:
		_filter_flags &= ~0x7E0;
		_filter_flags |= (1 << (widgetIndex - WIDX_FILTER_RIDE_TAB_TRANSPORT + 5));
		gConfigInterface.object_selection_filter_flags = _filter_flags;
		config_save_default();

		filter_update_counts();
		visible_list_refresh(w);

		w->selected_list_item = -1;
		w->object_entry = (rct_object_entry *)-1;
		w->scrolls[0].v_top = 0;
		window_invalidate(w);
		break;

	}
}

void window_editor_object_selection_mousedown(int widgetIndex, rct_window*w, rct_widget* widget)
{
	int num_items;

	//widget = &w->widgets[widgetIndex - 1];

	switch (widgetIndex) {
	case WIDX_FILTER_DROPDOWN:

		num_items = 4;
		gDropdownItemsFormat[DDIX_FILTER_RCT2] = STR_TOGGLE_OPTION;
		gDropdownItemsFormat[DDIX_FILTER_WW] = STR_TOGGLE_OPTION;
		gDropdownItemsFormat[DDIX_FILTER_TT] = STR_TOGGLE_OPTION;
		gDropdownItemsFormat[DDIX_FILTER_CUSTOM] = STR_TOGGLE_OPTION;
		gDropdownItemsArgs[DDIX_FILTER_RCT2] = STR_ROLLERCOASTER_TYCOON_2_DROPDOWN;
		gDropdownItemsArgs[DDIX_FILTER_WW] = STR_OBJECT_FILTER_WW;
		gDropdownItemsArgs[DDIX_FILTER_TT] = STR_OBJECT_FILTER_TT;
		gDropdownItemsArgs[DDIX_FILTER_CUSTOM] = STR_OBJECT_FILTER_CUSTOM;
		//Track manager cannot select multiple, so only show selection filters if not in track manager
		if (!(gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER)) {
			num_items = 7;
			gDropdownItemsFormat[DDIX_FILTER_SEPERATOR] = 0;
			gDropdownItemsFormat[DDIX_FILTER_SELECTED] = STR_TOGGLE_OPTION;
			gDropdownItemsFormat[DDIX_FILTER_NONSELECTED] = STR_TOGGLE_OPTION;
			gDropdownItemsArgs[DDIX_FILTER_SEPERATOR] = STR_NONE;
			gDropdownItemsArgs[DDIX_FILTER_SELECTED] = STR_SELECTED_ONLY;
			gDropdownItemsArgs[DDIX_FILTER_NONSELECTED] = STR_NON_SELECTED_ONLY;
		}

		window_dropdown_show_text(
			w->x + widget->left,
			w->y + widget->top,
			widget->bottom - widget->top + 1,
			w->colours[widget->colour],
			DROPDOWN_FLAG_STAY_OPEN,
			num_items
			);

		gDropdownItemsChecked = _filter_flags & 0xF;
		if (!(gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER)) {
			dropdown_set_checked(DDIX_FILTER_SELECTED, _FILTER_SELECTED);
			dropdown_set_checked(DDIX_FILTER_NONSELECTED, _FILTER_NONSELECTED);
		}
		break;

	}
}

static void window_editor_object_selection_dropdown(rct_window *w, int widgetIndex, int dropdownIndex)
{
	if (dropdownIndex == -1)
		return;

	switch (widgetIndex) {
	case WIDX_FILTER_DROPDOWN:
		if (dropdownIndex == DDIX_FILTER_SELECTED) {
			_filter_flags ^= FILTER_SELECTED;
			_filter_flags &= ~FILTER_NONSELECTED;
		}
		else if (dropdownIndex == DDIX_FILTER_NONSELECTED) {
			_filter_flags ^= FILTER_NONSELECTED;
			_filter_flags &= ~FILTER_SELECTED;
		}
		else {
			_filter_flags ^= (1 << dropdownIndex);
		}
		gConfigInterface.object_selection_filter_flags = _filter_flags;
		config_save_default();

		filter_update_counts();
		w->scrolls->v_top = 0;

		visible_list_refresh(w);
		window_invalidate(w);
		break;
	}
}

// n3ds port: the list's rows for the D-pad (window_n3ds_get_list_item)
bool window_editor_object_selection_n3ds_list_item(rct_window *w, int index, int *x, int *y, int *width, int *height)
{
	if (index < 0 || index >= _numListItems)
		return false;

	*x = 0;
	*y = index * N3DS_LIST_ROW;
	*width = w->widgets[WIDX_LIST].right - w->widgets[WIDX_LIST].left;
	*height = N3DS_LIST_ROW;
	return true;
}

/**
 *
 *  rct2: 0x006AB031
 */
static void window_editor_object_selection_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height)
{
	*height = _numListItems * N3DS_LIST_ROW;
}

/**
 *
 *  rct2: 0x006AB0B6
 */
static void window_editor_object_selection_scroll_mousedown(rct_window *w, int scrollIndex, int x, int y)
{
	// Used for in-game object selection cheat to prevent crashing the game
	// when windows attempt to draw objects that don't exist any more
	window_close_all_except_class(WC_EDITOR_OBJECT_SELECTION);

	int selected_object = get_object_from_object_selection((w->selected_tab & 0xFF), y);
	if (selected_object == -1)
		return;

	list_item * listItem = &_listItems[selected_object];
	uint8 object_selection_flags = *listItem->flags;
	if (object_selection_flags & OBJECT_SELECTION_FLAG_6)
		return;

	window_invalidate(w);

	audio_play_sound_panned(SOUND_CLICK_1, gCursorState.x, 0, 0, 0);


	if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER) {
		if (!window_editor_object_selection_select_object(0, 1, listItem->entry))
			return;

		// Close any other open windows such as options/colour schemes to prevent a crash.
		window_close_all();
		//window_close(w);

		//This function calls window_track_list_open
		window_editor_object_selection_manage_tracks();
		return;
	}

	int ebx = 6;
	// If already selected
	if (!(object_selection_flags & OBJECT_SELECTION_FLAG_SELECTED))
		ebx = 7;

	_maxObjectsWasHit = false;
	if (!window_editor_object_selection_select_object(0, ebx, listItem->entry)) {
		rct_string_id error_title = ebx & 1 ?
			STR_UNABLE_TO_SELECT_THIS_OBJECT :
			STR_UNABLE_TO_DE_SELECT_THIS_OBJECT;

		window_error_open(error_title, gGameCommandErrorText);
		return;
	}

	if (_FILTER_SELECTED || _FILTER_NONSELECTED) {
		filter_update_counts();
		visible_list_refresh(w);
		window_invalidate(w);
	}

	if (_maxObjectsWasHit) {
		window_error_open(STR_WARNING_TOO_MANY_OBJECTS_SELECTED, STR_NOT_ALL_OBJECTS_IN_THIS_SCENERY_GROUP_COULD_BE_SELECTED);
	}
}

/**
 *
 *  rct2: 0x006AB079
 */
static void window_editor_object_selection_scroll_mouseover(rct_window *w, int scrollIndex, int x, int y)
{
	int selectedObject = get_object_from_object_selection(w->selected_tab & 0xFF, y);
	if (selectedObject != -1) {
		list_item * listItem = &_listItems[selectedObject];
		uint8 objectSelectionFlags = *listItem->flags;
		if (objectSelectionFlags & OBJECT_SELECTION_FLAG_6) {
			selectedObject = -1;
		}
	}
	if (selectedObject != w->selected_list_item) {
		w->selected_list_item = selectedObject;

		object_delete(_loadedObject);
		_loadedObject = NULL;

		list_item * listItem = &_listItems[selectedObject];
		if (selectedObject == -1) {
			w->object_entry = NULL;
		} else {
			w->object_entry = listItem->entry;
			_loadedObject = object_repository_load_object(listItem->entry);
		}

		window_invalidate(w);
	}
}

/**
 *
 *  rct2: 0x006AA9FD
 */
static void window_editor_object_selection_invalidate(rct_window *w)
{
	colour_scheme_update(w);

	// n3ds port: the widgets are where the table puts them (no page tabs, no resizing); only
	// the pressed state and the title change
	w->pressed_widgets |= 1 << WIDX_PREVIEW;

	// Set window title
	if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER) {
		w->widgets[WIDX_TITLE].text = STR_TRACK_DESIGNS_MANAGER_SELECT_RIDE_TYPE;
	} else {
		w->widgets[WIDX_TITLE].text = STR_ROLLER_COASTER_DESIGNER_SELECT_RIDE_TYPES_VEHICLES;
	}

	// The ride category tabs: the pressed ones
	for (int i = 0; i < 7; i++)
		w->pressed_widgets &= ~(1 << (WIDX_FILTER_RIDE_TAB_ALL + i));
	if ((_filter_flags & 0x7E0) == 0x7E0)
		w->pressed_widgets |= (1 << WIDX_FILTER_RIDE_TAB_ALL);
	else {
		for (int i = 0; i < 6; i++) {
			if (_filter_flags & (1 << (5 + i)))
				w->pressed_widgets |= (uint64)(1 << (WIDX_FILTER_RIDE_TAB_TRANSPORT + i));
		}
	}
}

/**
 *
 *  rct2: 0x006AAB56
 */
static void window_editor_object_selection_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	int i, x, y;
	rct_widget *widget;
	rct_string_id stringId;

	window_draw_widgets(w, dpi);

	const int ride_tabs[] = { SPR_TAB_RIDE_16, 0x20000000 | SPR_TAB_RIDES_TRANSPORT_0, SPR_TAB_RIDES_GENTLE_0, 0x20000000 | SPR_TAB_RIDES_ROLLER_COASTERS_0, SPR_TAB_RIDES_THRILL_0, SPR_TAB_RIDES_WATER_0, SPR_TAB_RIDES_SHOP_0, SPR_TAB_FINANCES_RESEARCH_0 };
	const int ThrillRidesTabAnimationSequence[] = {
		5, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 0, 0, 0
	};

	// Draw ride tabs
	for (i = 0; i < 7; i++) {
		widget = &w->widgets[WIDX_FILTER_RIDE_TAB_ALL + i];
		if (widget->type == WWT_EMPTY)
			continue;

		int spriteIndex = ride_tabs[i];
		int frame = 0;
		if (i != 0 && w->pressed_widgets & (1ULL << (WIDX_FILTER_RIDE_TAB_ALL + i))) {
			frame = w->frame_no / window_editor_object_selection_animation_divisor[i - 1];
		}
		spriteIndex += (i == 4 ? ThrillRidesTabAnimationSequence[frame] : frame);

		x = w->x + widget->left;
		y = w->y + widget->top;
		gfx_draw_sprite(dpi, spriteIndex | (w->colours[1] << 19), x, y, 0);
	}

	// Preview background
	widget = &w->widgets[WIDX_PREVIEW];
	gfx_fill_rect(
		dpi,
		w->x + widget->left + 1,
		w->y + widget->top + 1,
		w->x + widget->right - 1,
		w->y + widget->bottom - 1,
		ColourMapA[w->colours[1]].darkest
	);

	// Draw number of selected items (n3ds port: under the list)
	if (!(gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER)) {
		x = w->x + 4;
		y = w->y + N3DS_COUNT_Y;

		int numSelected = _numSelectedObjectsForType[OBJECT_TYPE_RIDE];
		int totalSelectable = object_entry_group_counts[OBJECT_TYPE_RIDE];
		if (gScreenFlags & SCREEN_FLAGS_TRACK_DESIGNER)
			totalSelectable = 4;

		set_format_arg(0, uint16, numSelected);
		set_format_arg(2, uint16, totalSelectable);
		gfx_draw_string_left(dpi, STR_OBJECT_SELECTION_SELECTION_SIZE, gCommonFormatArgs, COLOUR_BLACK, x, y);
	}

	if (w->selected_list_item == -1 || _loadedObject == NULL)
		return;

	list_item *listItem = &_listItems[w->selected_list_item];
	rct_object_entry *highlightedEntry = w->object_entry;

	// Draw preview. n3ds port: at half size (a ride's is a 112x112 image), as in the new ride
	// window: drawn full size into the scratch image and halved into the box
	{
		rct_drawpixelinfo scratch = n3ds_scratch_begin_at(0, 0, N3DS_PREVIEW_FULL, N3DS_PREVIEW_FULL);
		object_draw_preview(_loadedObject, &scratch, N3DS_PREVIEW_FULL, N3DS_PREVIEW_FULL);
		n3ds_scratch_copy_half(dpi, w->x + widget->left + 1, w->y + widget->top + 1, N3DS_PREVIEW_FULL, N3DS_PREVIEW_FULL);
	}

	// n3ds port: the name, the ride type and the source one under the other below the preview,
	// in the column's width; the description (wrapped) under them, cut at the content's bottom
	x = w->x + N3DS_COLUMN_X + N3DS_COLUMN_WIDTH / 2;
	y = w->y + widget->bottom + 3;
	set_format_arg(0, rct_string_id, STR_STRING);
	set_format_arg(2, const char *, listItem->repositoryItem->Name);
	gfx_draw_string_centred_clipped(dpi, STR_WINDOW_COLOUR_2_STRINGID, gCommonFormatArgs, COLOUR_BLACK, x, y, N3DS_COLUMN_WIDTH);

	y += 11;
	stringId = get_ride_type_string_id(listItem->repositoryItem);
	gfx_draw_string_centred_clipped(dpi, stringId, NULL, COLOUR_WHITE, x, y, N3DS_COLUMN_WIDTH);

	// Draw object source
	y += 11;
	uint8 source = (highlightedEntry->flags & 0xF0) >> 4;
	switch (source) {
	case 8: stringId = STR_ROLLERCOASTER_TYCOON_2_DROPDOWN; break;
	case 1: stringId = STR_OBJECT_FILTER_WW; break;
	case 2: stringId = STR_OBJECT_FILTER_TT; break;
	default: stringId = STR_OBJECT_FILTER_CUSTOM; break;
	}
	gfx_draw_string_centred_clipped(dpi, stringId, NULL, COLOUR_WHITE, x, y, N3DS_COLUMN_WIDTH);

	// Draw description of object
	const char *description = object_get_description(_loadedObject);
	if (description != NULL) {
		y += 13;
		rct_drawpixelinfo clipDPI;
		if (clip_drawpixelinfo(&clipDPI, dpi, w->x + N3DS_COLUMN_X, y, N3DS_COLUMN_WIDTH, w->y + N3DS_CONTENT_BOTTOM - y)) {
			set_format_arg(0, rct_string_id, STR_STRING);
			set_format_arg(2, const char *, description);
			gfx_draw_string_left_wrapped(&clipDPI, gCommonFormatArgs, 0, 0, N3DS_COLUMN_WIDTH, STR_WINDOW_COLOUR_2_STRINGID, COLOUR_BLACK);
		}
	}
}

/**
 *
 *  rct2: 0x006AADA3
 */
static void window_editor_object_selection_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex)
{
	int x, y, i, colour, colour2;

	uint8 paletteIndex = ColourMapA[w->colours[1]].mid_light;
	gfx_clear(dpi, paletteIndex);

	y = 0;
	for (i = 0; i < _numListItems; i++) {
		list_item *listItem = &_listItems[i];

		// n3ds port: rows of N3DS_LIST_ROW pixels (for a finger), the 10-px checkbox and the text
		// in the middle of the row
		if (y + N3DS_LIST_ROW >= dpi->y && y <= dpi->y + dpi->height) {
			int textY = y + (N3DS_LIST_ROW - 10) / 2;
			// Draw checkbox
			if (!(gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER) && !(*listItem->flags & 0x20))
				gfx_fill_rect_inset(dpi, 2, textY, 11, textY + 10, w->colours[1], INSET_RECT_F_E0);

			// Highlight background
			colour = COLOUR_BRIGHT_GREEN | COLOUR_FLAG_TRANSLUCENT;
			if (listItem->entry == w->object_entry && !(*listItem->flags & OBJECT_SELECTION_FLAG_6)) {
				gfx_filter_rect(dpi, 0, y, w->width, y + N3DS_LIST_ROW - 1, PALETTE_DARKEN_1);
				colour = COLOUR_BRIGHT_GREEN;
			}

			// Draw checkmark
			if (!(gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER) && (*listItem->flags & OBJECT_SELECTION_FLAG_SELECTED)) {
				x = 2;
				gCurrentFontSpriteBase = colour == COLOUR_BRIGHT_GREEN ? FONT_SPRITE_BASE_MEDIUM_EXTRA_DARK : FONT_SPRITE_BASE_MEDIUM_DARK;
				colour2 = NOT_TRANSLUCENT(w->colours[1]);
				if (*listItem->flags & (OBJECT_SELECTION_FLAG_IN_USE | OBJECT_SELECTION_FLAG_ALWAYS_REQUIRED))
					colour2 |= COLOUR_FLAG_INSET;

				gfx_draw_string(dpi, (char*)CheckBoxMarkString, colour2, x, textY);
			}

			x = gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER ? 0 : 15;

			char *bufferWithColour = gCommonStringFormatBuffer;
			char *buffer = utf8_write_codepoint(bufferWithColour, colour);
			if (*listItem->flags & OBJECT_SELECTION_FLAG_6) {
				colour = w->colours[1] & 0x7F;
				gCurrentFontSpriteBase = FONT_SPRITE_BASE_MEDIUM_DARK;
			}
			else {
				colour = COLOUR_BLACK;
				gCurrentFontSpriteBase = FONT_SPRITE_BASE_MEDIUM;
			}

			// Draw text (n3ds port: the name alone; the ride type is beside the preview. The
			// original had both in columns, which the 172-px list has no room for)
			safe_strcpy(buffer, listItem->repositoryItem->Name, 256 - (buffer - bufferWithColour));
			if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER) {
				while (*buffer != 0 && *buffer != 9)
					buffer++;

				*buffer = 0;
			}
			gfx_draw_string(dpi, bufferWithColour, colour, x, textY);
		}
		y += N3DS_LIST_ROW;
	}
}

/**
 *
 *  rct2: 0x006AA770
 */
void reset_selected_object_count_and_size()
{
	for (uint8 objectType = 0; objectType < 11; objectType++) {
		_numSelectedObjectsForType[objectType] = 0;
	}

	int numObjects = (int)object_repository_get_items_count();
	const ObjectRepositoryItem * items = object_repository_get_items();
	for (int i = 0; i < numObjects; i++) {
		uint8 objectType = items[i].ObjectEntry.flags & 0xF;
		if (_objectSelectionFlags[i] & OBJECT_SELECTION_FLAG_SELECTED) {
			_numSelectedObjectsForType[objectType]++;
		}
	}
}

/**
 * Master objects are objects that are not
 * optional / required dependants of an
 * object.
 */
static void set_object_selection_error(uint8 is_master_object, rct_string_id error_msg){
	gGameCommandErrorText = error_msg;
	if (!is_master_object){
		reset_selected_object_count_and_size();
	}
}

/**
 *
 *  rct2: 0x006AB54F
 */
static int window_editor_object_selection_select_object(uint8 bh, int flags, const rct_object_entry *entry)
{
	int numObjects = (int)object_repository_get_items_count();
	const ObjectRepositoryItem * item = object_repository_find_object_by_entry(entry);
	if (item == NULL) {
		set_object_selection_error(bh, STR_OBJECT_SELECTION_ERR_OBJECT_DATA_NOT_FOUND);
		return 0;
	}

	// Get repository item index
	int index = -1;
	const ObjectRepositoryItem * items = object_repository_get_items();
	for (int i = 0; i < numObjects; i++) {
		if (&items[i] == item) {
			index = i;
		}
	}

	uint8 * selectionFlags = &_objectSelectionFlags[index];
	if (!(flags & 1)) {
		if (!(*selectionFlags & OBJECT_SELECTION_FLAG_SELECTED)) {
			return 1;
		}
		else if (*selectionFlags & OBJECT_SELECTION_FLAG_IN_USE) {
			set_object_selection_error(bh, STR_OBJECT_SELECTION_ERR_CURRENTLY_IN_USE);
			return 0;
		}
		else if (*selectionFlags & OBJECT_SELECTION_FLAG_ALWAYS_REQUIRED) {
			set_object_selection_error(bh, STR_OBJECT_SELECTION_ERR_ALWAYS_REQUIRED);
			return 0;
		}

		uint8 objectType = item->ObjectEntry.flags & 0xF;
		if (objectType == OBJECT_TYPE_SCENERY_SETS && (flags & (1 << 2))) {
			for (int j = 0; j < item->NumThemeObjects; j++) {
				window_editor_object_selection_select_object(++bh, flags, &item->ThemeObjects[j]);
			}
		}

		_numSelectedObjectsForType[objectType]--;
		*selectionFlags &= ~OBJECT_SELECTION_FLAG_SELECTED;
		return 1;
	} else {
		if (bh == 0) {
			if (flags & (1 << 3)) {
				*selectionFlags |= OBJECT_SELECTION_FLAG_ALWAYS_REQUIRED;
			}
		}
		if (*selectionFlags & OBJECT_SELECTION_FLAG_SELECTED) {
			return 1;
		}

		uint8 objectType = item->ObjectEntry.flags & 0xF;
		uint16 maxObjects = object_entry_group_counts[objectType];
		if (gScreenFlags & SCREEN_FLAGS_TRACK_DESIGNER) {
			maxObjects = 4;
		}

		if (maxObjects <= _numSelectedObjectsForType[objectType]) {
			set_object_selection_error(bh, STR_OBJECT_SELECTION_ERR_TOO_MANY_OF_TYPE_SELECTED);
			return 0;
		}

		if (objectType == OBJECT_TYPE_SCENERY_SETS && (flags & (1 << 2))) {
			for (uint16 j = 0; j < item->NumThemeObjects; j++) {
				if (!window_editor_object_selection_select_object(++bh, flags, &item->ThemeObjects[j])) {
					_maxObjectsWasHit = true;
				}
			}
		}

		if (bh != 0 && !(flags & (1 << 1))) {
			char objectName[64];
			object_create_identifier_name(objectName, 64, &item->ObjectEntry);
			set_format_arg(0, const char *, objectName);
			set_object_selection_error(bh, STR_OBJECT_SELECTION_ERR_SHOULD_SELECT_X_FIRST);
			return 0;
		}

		if (maxObjects <= _numSelectedObjectsForType[objectType]) {
			set_object_selection_error(bh, STR_OBJECT_SELECTION_ERR_TOO_MANY_OF_TYPE_SELECTED);
			return 0;
		}

		_numSelectedObjectsForType[objectType]++;

		*selectionFlags |= OBJECT_SELECTION_FLAG_SELECTED;
		return 1;
	}
}

/**
 * Takes the y coordinate of the clicked on scroll list
 * and converts this into an object selection.
 * Returns the position in the list.
 * Object_selection_flags, installed_entry also populated
 *
 *  rct2: 0x006AA703
 */
static int get_object_from_object_selection(uint8 object_type, int y)
{
	int listItemIndex = y / N3DS_LIST_ROW;
	if (listItemIndex < 0 || listItemIndex >= _numListItems)
		return -1;

	return listItemIndex;
}

/**
 *
 *  rct2: 0x006D33E2
 */
static void window_editor_object_selection_manage_tracks()
{
	for (int i = 0; i < 4; i++) {
		gResearchedRideTypes[i] = 0xFFFFFFFF;
	}

	for (int i = 0; i < countof(RideTypePossibleTrackConfigurations); i++) {
		gResearchedTrackTypesA[i] = (RideTypePossibleTrackConfigurations[i]         ) & 0xFFFFFFFFULL;
		gResearchedTrackTypesB[i] = (RideTypePossibleTrackConfigurations[i] >> 32ULL) & 0xFFFFFFFFULL;
	}


	for (int i = countof(RideTypePossibleTrackConfigurations); i < 128; i++) {
		gResearchedTrackTypesA[i] = (uint32)-1;
		gResearchedTrackTypesB[i] = (uint32)-1;
	}

	for (int i = 0; i < 8; i++) {
		gResearchedRideEntries[i] = 0xFFFFFFFF;
	}

	gS6Info.editor_step = EDITOR_STEP_TRACK_DESIGNS_MANAGER;

	int entry_index = 0;
	for (; ((intptr_t)object_entry_groups[0].chunks[entry_index]) == -1; ++entry_index);

	rct_ride_entry* ride_entry = get_ride_entry(entry_index);
	uint8* ride_type_array = &ride_entry->ride_type[0];

	int ride_type;
	for (int i = 0; (ride_type = ride_type_array[i]) == 0xFF; i++) { }

	ride_list_item item = { ride_type, entry_index };
	// track_load_list(item);
	window_track_list_open(item);
}

/**
 *
 *  rct2: 0x006ABBBE
 */
static void editor_load_selected_objects()
{
	int numItems = (int)object_repository_get_items_count();
	const ObjectRepositoryItem * items = object_repository_get_items();
	for (int i = 0; i < numItems; i++) {
		if (_objectSelectionFlags[i] & OBJECT_SELECTION_FLAG_SELECTED) {
			const ObjectRepositoryItem * item = &items[i];
			const rct_object_entry * entry = &item->ObjectEntry;
			void * loadedObject = object_manager_get_loaded_object(entry);
			if (loadedObject == NULL) {
				loadedObject = object_manager_load_object(entry);
				if (loadedObject == NULL) {
					log_error("Failed to load entry %.8s", entry->name);
				}
				else if (!(gScreenFlags & SCREEN_FLAGS_EDITOR)) {
					// Defaults selected items to researched (if in-game)
					uint8 objectType = entry->flags & 0x0F;
					uint8 entryIndex = object_manager_get_loaded_object_entry_index(loadedObject);
					if (objectType == OBJECT_TYPE_RIDE) {
						rct_ride_entry *rideType = get_ride_entry(entryIndex);
						research_insert(1, 0x10000 | (rideType->ride_type[0] << 8) | entryIndex, rideType->category[0]);
					}
					else if (objectType == OBJECT_TYPE_SCENERY_SETS) {
						research_insert(1, entryIndex, RESEARCH_CATEGORY_SCENERYSET);
					}
				}
			}
		}
	}
}

static void window_editor_object_selection_update(rct_window *w)
{
	for (int i = WIDX_FILTER_RIDE_TAB_TRANSPORT; i <= WIDX_FILTER_RIDE_TAB_STALL; i++) {
		if (!(w->pressed_widgets & (1ULL << i)))
			continue;

		w->frame_no++;
		if (w->frame_no >= window_editor_object_selection_animation_loops[i - WIDX_FILTER_RIDE_TAB_TRANSPORT])
			w->frame_no = 0;

		widget_invalidate(w, i);
		break;
	}
}

static bool filter_selected(uint8 objectFlag)
{
	if (_FILTER_SELECTED == _FILTER_NONSELECTED) {
		return true;
	}
	if (_FILTER_SELECTED && objectFlag & OBJECT_SELECTION_FLAG_SELECTED) {
		return true;
	}
	else if (_FILTER_NONSELECTED && !(objectFlag & OBJECT_SELECTION_FLAG_SELECTED)) {
		return true;
	}
	else {
		return false;
	}
}

static bool filter_source(const ObjectRepositoryItem * item)
{
	if (_FILTER_ALL)
		return true;

	uint8 source = (item->ObjectEntry.flags & 0xF0) >> 4;
	return (_FILTER_RCT2 && source == 8) || (_FILTER_WW && source == 1) || (_FILTER_TT && source == 2) || (_FILTER_CUSTOM && source != 8 && source != 1 && source != 2);
}

static bool filter_chunks(const ObjectRepositoryItem * item)
{
	switch (item->ObjectEntry.flags & 0x0F) {
	case OBJECT_TYPE_RIDE:
		if(!gConfigInterface.select_by_track_type) {
			if (_filter_flags & (1 << (item->RideCategory[0] + 5)))
				return true;
			if (_filter_flags & (1 << (item->RideCategory[1] + 5)))
				return true;
		}
		else {
			uint8 rideType = 0;
			for (int i = 0; i < 3; i++) {
				if (item->RideType[i] != 255) {
					rideType = item->RideType[i];
					break;
				}
			}
			if (_filter_flags & (1 << (gRideCategories[rideType] + 5)))
				return true;
		}
		return false;
	}
	return true;
}

static void filter_update_counts()
{
	if (!_FILTER_ALL) {
		uint8 *selectionFlags = _objectSelectionFlags;
		for (int i = 0; i < 11; i++) {
			_filter_object_counts[i] = 0;
		}

		size_t numObjects = object_repository_get_items_count();
		const ObjectRepositoryItem * items = object_repository_get_items();
		for (size_t i = 0; i < numObjects; i++) {
			const ObjectRepositoryItem * item = &items[i];
			if (filter_source(item) &&
				filter_chunks(item) &&
				filter_selected(*selectionFlags)
			) {
				uint8 objectType = item->ObjectEntry.flags & 0xF;
				_filter_object_counts[objectType]++;
			}
			selectionFlags++;
		}
	}
}

static rct_string_id get_ride_type_string_id(const ObjectRepositoryItem * item)
{
	rct_string_id result = STR_NONE;
	for (int i = 0; i < 3; i++) {
		uint8 rideType = item->RideType[i];
		if (rideType != 255) {
			result = RideNaming[rideType].name;
			break;
		}
	}
	return result;
}

bool editor_check_object_group_at_least_one_selected(int checkObjectType)
{
	int numObjects = (int)object_repository_get_items_count();
	const ObjectRepositoryItem * items = object_repository_get_items();

	for (int i = 0; i < numObjects; i++) {
		uint8 objectType = items[i].ObjectEntry.flags & 0x0F;
		if (checkObjectType == objectType && (_objectSelectionFlags[i] & OBJECT_SELECTION_FLAG_SELECTED)) {
			return true;
		}
	}
	return false;
}
