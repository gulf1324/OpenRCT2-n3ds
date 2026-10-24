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

#include "../config.h"
#include "../interface/window.h"
#include "../interface/widget.h"
#include "../localisation/localisation.h"
#include "../platform/platform.h"
#include "../interface/keyboard_shortcut.h"
#include "../interface/themes.h"

// n3ds port: the bottom screen's page, 16px rows that can be tapped (LIST_ROW_HEIGHT as the other lists)
#define WW 320
#define WH 240
#define LIST_ROW_HEIGHT 16

enum WINDOW_SHORTCUT_WIDGET_IDX {
	WIDX_BACKGROUND,
	WIDX_TITLE,
	WIDX_CLOSE,
	WIDX_SCROLL,
	WIDX_RESET
};

// 0x9DE48C
static rct_widget window_shortcut_widgets[] = {
	{ WWT_FRAME,			0,	0,		WW - 1,	0,		WH - 1,		STR_NONE,					STR_NONE },
	{ WWT_CAPTION,			0,	1,		WW - 2,	1,		14,			STR_SHORTCUTS_TITLE,		STR_WINDOW_TITLE_TIP },
	{ WWT_CLOSEBOX,			0,	WW-13,	WW - 3,	2,		13,			STR_CLOSE_X,				STR_CLOSE_WINDOW_TIP },
	{ WWT_SCROLL,			0,	4,		WW - 5,	18,		WH - 26,	SCROLL_VERTICAL,			STR_SHORTCUT_LIST_TIP },
	{ WWT_DROPDOWN_BUTTON,	0,	4,		153,	WH-23,	WH - 6,		STR_SHORTCUT_ACTION_RESET,	STR_SHORTCUT_ACTION_RESET_TIP },	// n3ds port: N3DS_CONTROL_HEIGHT
	{ WIDGETS_END }
};

static void window_shortcut_mouseup(rct_window *w, int widgetIndex);
static void window_shortcut_invalidate(rct_window *w);
static void window_shortcut_paint(rct_window *w, rct_drawpixelinfo *dpi);
static void window_shortcut_tooltip(rct_window* w, int widgetIndex, rct_string_id *stringId);
static void window_shortcut_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height);
static void window_shortcut_scrollmousedown(rct_window *w, int scrollIndex, int x, int y);
static void window_shortcut_scrollmouseover(rct_window *w, int scrollIndex, int x, int y);
static void window_shortcut_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex);

static rct_window_event_list window_shortcut_events = {
	NULL,
	window_shortcut_mouseup,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	window_shortcut_scrollgetsize,
	window_shortcut_scrollmousedown,
	NULL,
	window_shortcut_scrollmouseover,
	NULL,
	NULL,
	NULL,
	window_shortcut_tooltip,
	NULL,
	NULL,
	window_shortcut_invalidate,
	window_shortcut_paint,
	window_shortcut_scrollpaint
};

const rct_string_id ShortcutStringIds[] = {
	STR_SHORTCUT_CLOSE_TOP_MOST_WINDOW,
	STR_SHORTCUT_CLOSE_ALL_FLOATING_WINDOWS,
	STR_SHORTCUT_CANCEL_CONSTRUCTION_MODE,
	STR_SHORTCUT_PAUSE_GAME,
	STR_SHORTCUT_ZOOM_VIEW_OUT,
	STR_SHORTCUT_ZOOM_VIEW_IN,
	STR_SHORTCUT_ROTATE_VIEW_CLOCKWISE,
	STR_SHORTCUT_ROTATE_VIEW_ANTICLOCKWISE,
	STR_SHORTCUT_ROTATE_CONSTRUCTION_OBJECT,
	STR_SHORTCUT_UNDERGROUND_VIEW_TOGGLE,
	STR_SHORTCUT_REMOVE_BASE_LAND_TOGGLE,
	STR_SHORTCUT_REMOVE_VERTICAL_LAND_TOGGLE,
	STR_SHORTCUT_SEE_THROUGH_RIDES_TOGGLE,
	STR_SHORTCUT_SEE_THROUGH_SCENERY_TOGGLE,
	STR_SHORTCUT_INVISIBLE_SUPPORTS_TOGGLE,
	STR_SHORTCUT_INVISIBLE_PEOPLE_TOGGLE,
	STR_SHORTCUT_HEIGHT_MARKS_ON_LAND_TOGGLE,
	STR_SHORTCUT_HEIGHT_MARKS_ON_RIDE_TRACKS_TOGGLE,
	STR_SHORTCUT_HEIGHT_MARKS_ON_PATHS_TOGGLE,
	STR_SHORTCUT_ADJUST_LAND,
	STR_SHORTCUT_ADJUST_WATER,
	STR_SHORTCUT_BUILD_SCENERY,
	STR_SHORTCUT_BUILD_PATHS,
	STR_SHORTCUT_BUILD_NEW_RIDE,
	STR_SHORTCUT_SHOW_FINANCIAL_INFORMATION,
	STR_SHORTCUT_SHOW_RESEARCH_INFORMATION,
	STR_SHORTCUT_SHOW_RIDES_LIST,
	STR_SHORTCUT_SHOW_PARK_INFORMATION,
	STR_SHORTCUT_SHOW_GUEST_LIST,
	STR_SHORTCUT_SHOW_STAFF_LIST,
	STR_SHORTCUT_SHOW_RECENT_MESSAGES,
	STR_SHORTCUT_SHOW_MAP,
	STR_SHORTCUT_SCREENSHOT,
	STR_SHORTCUT_REDUCE_GAME_SPEED,
	STR_SHORTCUT_INCREASE_GAME_SPEED,
	STR_SHORTCUT_TOGGLE_VISIBILITY_OF_TOOLBARS,
	STR_SHORTCUT_SCROLL_MAP_UP,
	STR_SHORTCUT_SCROLL_MAP_LEFT,
	STR_SHORTCUT_SCROLL_MAP_DOWN,
	STR_SHORTCUT_SCROLL_MAP_RIGHT,
	STR_SHORTCUT_QUICK_SAVE_GAME,
	STR_SHORTCUT_SHOW_OPTIONS,
	STR_SHORTCUT_MUTE_SOUND,
	STR_SHORTCUT_SEE_THROUGH_PATHS_TOGGLE,
	// n3ds port: the construction shortcuts (upstream #5200): their names are not in the 0.0.5
	// language files, so they are in code (window_shortcut_keys_n3ds_name); STR_STRING marks them
	STR_STRING,
	STR_STRING,
	STR_STRING,
	STR_STRING,
	STR_STRING,
	STR_STRING,
	STR_STRING,
	STR_STRING,
	STR_STRING,
	STR_STRING,
	STR_STRING,
	STR_STRING,
};

// n3ds port: the names the language files do not have, English and Korean
static const char *N3dsConstructionShortcutNames[][2] = {
	{ "Ride construction - Turn left", "놀이기구 건설 - 좌회전" },
	{ "Ride construction - Turn right", "놀이기구 건설 - 우회전" },
	{ "Ride construction - Use default track", "놀이기구 건설 - 기본 트랙(직진·평지)" },
	{ "Ride construction - Slope down", "놀이기구 건설 - 내리막" },
	{ "Ride construction - Slope up", "놀이기구 건설 - 오르막" },
	{ "Ride construction - Toggle chain lift", "놀이기구 건설 - 체인 리프트 켜기/끄기" },
	{ "Ride construction - Bank left", "놀이기구 건설 - 왼쪽 뱅크" },
	{ "Ride construction - Bank right", "놀이기구 건설 - 오른쪽 뱅크" },
	{ "Ride construction - Previous track", "놀이기구 건설 - 이전 구간" },
	{ "Ride construction - Next track", "놀이기구 건설 - 다음 구간" },
	{ "Ride construction - Build current", "놀이기구 건설 - 짓기" },
	{ "Ride construction - Demolish current", "놀이기구 건설 - 마지막 구간 철거" },
};

const char *window_shortcut_keys_n3ds_name(int shortcut)
{
	if (shortcut < SHORTCUT_RIDE_CONSTRUCTION_TURN_LEFT || shortcut > SHORTCUT_RIDE_CONSTRUCTION_DEMOLISH_CURRENT)
		return NULL;
	return N3dsConstructionShortcutNames[shortcut - SHORTCUT_RIDE_CONSTRUCTION_TURN_LEFT][gCurrentLanguage == LANGUAGE_KOREAN ? 1 : 0];
}

/**
 *
 *  rct2: 0x006E3884
 */
// n3ds port: the rows are the items of the list for the D-pad (window_n3ds_get_list_item)
bool window_shortcut_keys_n3ds_list_item(rct_window *w, int index, int *x, int *y, int *width, int *height)
{
	if (index >= w->no_list_items)
		return false;

	*x = 0;
	*y = index * LIST_ROW_HEIGHT;
	*width = w->width;
	*height = LIST_ROW_HEIGHT;
	return true;
}

void window_shortcut_keys_open()
{
	rct_window* w;

	w = window_bring_to_front_by_class(WC_KEYBOARD_SHORTCUT_LIST);

	if (w) return;

	w = window_create_auto_pos(WW, WH, &window_shortcut_events, WC_KEYBOARD_SHORTCUT_LIST, 0);

	w->widgets = window_shortcut_widgets;
	w->enabled_widgets = (1 << WIDX_CLOSE) | (1 << WIDX_RESET);
	window_init_scroll_widgets(w);

	w->no_list_items = SHORTCUT_COUNT;
	w->selected_list_item = -1;
}

/**
*
*  rct2: 0x006E39E4
*/
static void window_shortcut_mouseup(rct_window *w, int widgetIndex)
{
	switch (widgetIndex){
	case WIDX_CLOSE:
		window_close(w);
		break;
	case WIDX_RESET:
		config_reset_shortcut_keys();
		config_shortcut_keys_save();
		window_invalidate(w);
		break;
	}
}

static void window_shortcut_invalidate(rct_window *w)
{
	colour_scheme_update(w);
}

/**
*
*  rct2: 0x006E38E0
*/
static void window_shortcut_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	window_draw_widgets(w, dpi);
}

/**
*
*  rct2: 0x006E3A0C
*/
static void window_shortcut_tooltip(rct_window* w, int widgetIndex, rct_string_id *stringId)
{
	set_format_arg(0, rct_string_id, STR_LIST);
}

/**
*
*  rct2: 0x006E3A07
*/
static void window_shortcut_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height)
{
	*height = w->no_list_items * LIST_ROW_HEIGHT;
}

/**
*
*  rct2: 0x006E3A3E
*/
static void window_shortcut_scrollmousedown(rct_window *w, int scrollIndex, int x, int y)
{
	int selected_item = y / LIST_ROW_HEIGHT;
	if (selected_item >= w->no_list_items)
		return;

	window_shortcut_change_open(selected_item);
}

/**
*
*  rct2: 0x006E3A16
*/
static void window_shortcut_scrollmouseover(rct_window *w, int scrollIndex, int x, int y)
{
	int selected_item = y / LIST_ROW_HEIGHT;
	if (selected_item >= w->no_list_items)
		return;

	w->selected_list_item = selected_item;

	window_invalidate(w);
}

/**
 *
 *  rct2: 0x006E38E6
 */
static void window_shortcut_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex)
{
	gfx_fill_rect(dpi, dpi->x, dpi->y, dpi->x + dpi->width - 1, dpi->y + dpi->height - 1, ColourMapA[w->colours[1]].mid_light);

	for (int i = 0; i < w->no_list_items; ++i) {
		int y = i * LIST_ROW_HEIGHT;
		if (y > dpi->y + dpi->height)
			break;

		if (y + LIST_ROW_HEIGHT < dpi->y)continue;
		int format = STR_BLACK_STRING;
		if (i == w->selected_list_item) {
			format = STR_WINDOW_COLOUR_2_STRINGID;
			gfx_filter_rect(dpi, 0, y, 800, y + LIST_ROW_HEIGHT - 1, PALETTE_DARKEN_1);
		}

		char templateString[128];
		keyboard_shortcut_format_string(templateString, 128, gShortcutKeys[i]);

		set_format_arg(0, rct_string_id, STR_SHORTCUT_ENTRY_FORMAT);
		const char *codedName = window_shortcut_keys_n3ds_name(i);
		if (codedName != NULL) {
			// n3ds port: a name in code takes a pointer after its STR_STRING
			set_format_arg(2, rct_string_id, STR_STRING);
			set_format_arg(4, const char *, codedName);
			set_format_arg(8, rct_string_id, STR_STRING);
			set_format_arg(10, char *, templateString);
		} else {
			set_format_arg(2, rct_string_id, ShortcutStringIds[i]);
			set_format_arg(4, rct_string_id, STR_STRING);
			set_format_arg(6, char *, templateString);
		}
		gfx_draw_string_left(dpi, format, gCommonFormatArgs, COLOUR_BLACK, 0, y - 1 + (LIST_ROW_HEIGHT - 10) / 2);
	}
}
