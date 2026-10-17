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

/**
 * To better group the options together and allow the window to be scalable with additional OpenRCT2 options, the window has
 * been changed to a tab interface similar to the options window seen in Locomotion.
 *
 * TODO Some parts, particularly the string handling and order of widgets needs reorganising.
 *      Padding between the widgets and the window needs reducing, an artifact from originally being inside group boxes.
 */

#include "../audio/audio.h"
#include "../audio/mixer.h"
#include "../config.h"
#include "../drawing/drawing.h"
#include "../input.h"
#include "../interface/themes.h"
#include "../interface/viewport.h"
#include "../interface/widget.h"
#include "../interface/window.h"
#include "../localisation/currency.h"
#include "../localisation/date.h"
#include "../localisation/localisation.h"
#include "../platform/platform.h"
#include "../rct2.h"
#include "../sprites.h"
#include "../title/TitleScreen.h"
#include "../title/TitleSequence.h"
#include "../title/TitleSequenceManager.h"
#include "../util/util.h"
#include "dropdown.h"
#include "error.h"

enum WINDOW_OPTIONS_PAGE {
	WINDOW_OPTIONS_PAGE_RENDERING,
	WINDOW_OPTIONS_PAGE_CULTURE,
	WINDOW_OPTIONS_PAGE_AUDIO,
	WINDOW_OPTIONS_PAGE_CONTROLS_AND_INTERFACE,
	WINDOW_OPTIONS_PAGE_MISC,
	WINDOW_OPTIONS_PAGE_COUNT
};

#pragma region Widgets

enum WINDOW_OPTIONS_WIDGET_IDX {
	WIDX_BACKGROUND,
	WIDX_TITLE,
	WIDX_CLOSE,
	WIDX_PAGE_BACKGROUND,
	WIDX_TAB_1,
	WIDX_TAB_2,
	WIDX_TAB_3,
	WIDX_TAB_4,
	WIDX_TAB_5,

	WIDX_PAGE_START,

	// Rendering
	WIDX_RENDERING_GROUP = WIDX_PAGE_START,
	WIDX_TILE_SMOOTHING_CHECKBOX,
	WIDX_GRIDLINES_CHECKBOX,
	WIDX_CONSTRUCTION_MARKER,
	WIDX_CONSTRUCTION_MARKER_DROPDOWN,
	WIDX_DAY_NIGHT_CHECKBOX,
	WIDX_UPPER_CASE_BANNERS_CHECKBOX,
	WIDX_RENDER_WEATHER_EFFECTS_CHECKBOX,
	WIDX_DISABLE_LIGHTNING_EFFECT_CHECKBOX,

	// Culture / Units
	WIDX_LANGUAGE = WIDX_PAGE_START,
	WIDX_LANGUAGE_DROPDOWN,
	WIDX_CURRENCY,
	WIDX_CURRENCY_DROPDOWN,
	WIDX_DISTANCE,
	WIDX_DISTANCE_DROPDOWN,
	WIDX_TEMPERATURE,
	WIDX_TEMPERATURE_DROPDOWN,
	WIDX_HEIGHT_LABELS,
	WIDX_HEIGHT_LABELS_DROPDOWN,
	WIDX_DATE_FORMAT,
	WIDX_DATE_FORMAT_DROPDOWN,

	// Audio
	WIDX_SOUND_CHECKBOX = WIDX_PAGE_START,
	WIDX_MUSIC_CHECKBOX,
	WIDX_TITLE_MUSIC,
	WIDX_TITLE_MUSIC_DROPDOWN,
	WIDX_SOUND_VOLUME,
	WIDX_MUSIC_VOLUME,
	WIDX_MASTER_VOLUME,	// n3ds port: upstream #7848 (the setting is 0.0.5's)

	// Controls and interface
	WIDX_INVERT_DRAG = WIDX_PAGE_START,
	WIDX_THEMES,
	WIDX_THEMES_DROPDOWN,
	WIDX_THEMES_BUTTON,
	WIDX_HOTKEY_DROPDOWN,
	WIDX_SELECT_BY_TRACK_TYPE,
	WIDX_SCENARIO_GROUPING,
	WIDX_SCENARIO_GROUPING_DROPDOWN,
	WIDX_SCENARIO_UNLOCKING,

	// Misc
	WIDX_REAL_NAME_CHECKBOX = WIDX_PAGE_START,
	WIDX_SAVE_PLUGIN_DATA_CHECKBOX,
	WIDX_AUTOSAVE,
	WIDX_AUTOSAVE_DROPDOWN,
	WIDX_TITLE_SEQUENCE,
	WIDX_TITLE_SEQUENCE_DROPDOWN,
	WIDX_AUTO_STAFF_PLACEMENT,
	WIDX_HANDYMEN_MOW_DEFAULT,
	WIDX_AUTO_OPEN_SHOPS,
	WIDX_DEFAULT_INSPECTION_INTERVAL,
	WIDX_DEFAULT_INSPECTION_INTERVAL_DROPDOWN
};

// n3ds port: the window fills the bottom screen (see window_options_n3ds_layout)
#define WW 			320
#define WH 			240

#define MAIN_OPTIONS_WIDGETS \
	{ WWT_FRAME,			0,	0,		WW-1,	0,		WH-1,	STR_NONE,				STR_NONE }, \
	{ WWT_CAPTION,			0,	1,		WW-2,	1,		14,		STR_OPTIONS_TITLE,		STR_WINDOW_TITLE_TIP }, \
	{ WWT_CLOSEBOX,			0,	WW-13,	WW-3,	2,		13,		STR_CLOSE_X,			STR_CLOSE_WINDOW_TIP }, \
	{ WWT_RESIZE,			1,	0,		WW-1,	43,		WH-1,	0xFFFFFFFF,				STR_NONE }, \
	{ WWT_TAB,				1,	3,		33,		17,		43,		0x20000000 | SPR_TAB,	STR_OPTIONS_RENDERING_TIP }, \
	{ WWT_TAB,				1,	34,		64,		17,		43,		0x20000000 | SPR_TAB,	STR_OPTIONS_CULTURE_TIP }, \
	{ WWT_TAB,				1,	65,		95,		17,		43,		0x20000000 | SPR_TAB,	STR_OPTIONS_AUDIO_TIP }, \
	{ WWT_TAB,				1,	96,		126,	17,		43,		0x20000000 | SPR_TAB,	STR_OPTIONS_CONTROLS_AND_INTERFACE_TIP }, \
	{ WWT_TAB,				1,	127,	157,	17,		43,		0x20000000 | SPR_TAB,	STR_OPTIONS_MISCELLANEOUS_TIP }

static rct_widget window_options_rendering_widgets[] = {
	MAIN_OPTIONS_WIDGETS,
#define FRAME_RENDERING_START 53
	{ WWT_GROUPBOX,			1,	5,      304,	FRAME_RENDERING_START + 0,		FRAME_RENDERING_START + 136,	STR_RENDERING_GROUP,			STR_NONE },								// Rendering group
	{ WWT_CHECKBOX,			1,	10,		290,	FRAME_RENDERING_START + 15,		FRAME_RENDERING_START + 26,		STR_TILE_SMOOTHING, 			STR_TILE_SMOOTHING_TIP },				// Landscape smoothing
	{ WWT_CHECKBOX,			1,	10,		290,	FRAME_RENDERING_START + 30,		FRAME_RENDERING_START + 41,		STR_GRIDLINES,					STR_GRIDLINES_TIP },					// Gridlines
	{ WWT_DROPDOWN,			1,	155,	299,	FRAME_RENDERING_START + 45,		FRAME_RENDERING_START + 55,		STR_NONE,						STR_NONE },								// Construction marker
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	FRAME_RENDERING_START + 46,		FRAME_RENDERING_START + 54,		STR_DROPDOWN_GLYPH,				STR_CONSTRUCTION_MARKER_COLOUR_TIP },
	{ WWT_CHECKBOX,			1,	10,		290,	FRAME_RENDERING_START + 60,		FRAME_RENDERING_START + 71,		STR_CYCLE_DAY_NIGHT,			STR_CYCLE_DAY_NIGHT_TIP },				// Cycle day-night
	{ WWT_CHECKBOX,			1,	10,		290,	FRAME_RENDERING_START + 75,		FRAME_RENDERING_START + 86,		STR_UPPERCASE_BANNERS,			STR_UPPERCASE_BANNERS_TIP },			// Uppercase banners
	{ WWT_CHECKBOX,			1,	10,		290,	FRAME_RENDERING_START + 90,		FRAME_RENDERING_START + 101,	STR_RENDER_WEATHER_EFFECTS,		STR_RENDER_WEATHER_EFFECTS_TIP },		// Render weather effects
	{ WWT_CHECKBOX,			1,	31,		290,	FRAME_RENDERING_START + 105,	FRAME_RENDERING_START + 116,	STR_DISABLE_LIGHTNING_EFFECT,	STR_DISABLE_LIGHTNING_EFFECT_TIP },		// Disable lightning effect
#undef FRAME_RENDERING_START
	{ WIDGETS_END },
};

static rct_widget window_options_culture_widgets[] = {
	MAIN_OPTIONS_WIDGETS,
	{ WWT_DROPDOWN,			1,	155,	299,	53,		64,		STR_NONE,				STR_NONE },	// language
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	54,		63,		STR_DROPDOWN_GLYPH,		STR_LANGUAGE_TIP },
	{ WWT_DROPDOWN,			1,	155,	299,	68,		79,		STR_ARG_12_STRINGID,	STR_NONE },	// Currency
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	69,		78,		STR_DROPDOWN_GLYPH,		STR_CURRENCY_TIP },
	{ WWT_DROPDOWN,			1,	155,	299,	83,		94,		STR_ARG_14_STRINGID,	STR_NONE },	// Distance and speed
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	84,		93,		STR_DROPDOWN_GLYPH,		STR_DISTANCE_AND_SPEED_TIP },
	{ WWT_DROPDOWN,			1,	155,	299,	98,		110,	STR_ARG_20_STRINGID,	STR_NONE },	// Temperature
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	99,		108,	STR_DROPDOWN_GLYPH,		STR_TEMPERATURE_FORMAT_TIP },
	{ WWT_DROPDOWN,			1,	155,	299,	113,	124,	STR_ARG_6_STRINGID,		STR_NONE },	// Height labels
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	114,	123,	STR_DROPDOWN_GLYPH,		STR_HEIGHT_LABELS_UNITS_TIP },
	{ WWT_DROPDOWN,			1,	155,	299,	128,	139,	STR_NONE,				STR_NONE },	// Date format
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	129,	138,	STR_DROPDOWN_GLYPH,		STR_DATE_FORMAT_TIP },
	{ WIDGETS_END },
};

static rct_widget window_options_audio_widgets[] = {
	MAIN_OPTIONS_WIDGETS,
	{ WWT_CHECKBOX,			1,	10,		229,	69,		80,		STR_SOUND_EFFECTS,		STR_SOUND_EFFECTS_TIP },		// Enable / disable sound effects
	{ WWT_CHECKBOX,			1,	10,		229,	84,		95,		STR_RIDE_MUSIC,			STR_RIDE_MUSIC_TIP },			// Enable / disable ride music
	{ WWT_DROPDOWN,			1,	155,	299,	112,	124,	STR_NONE,				STR_NONE },						// Title music
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	113,	123,	STR_DROPDOWN_GLYPH,		STR_TITLE_MUSIC_TIP },
	{ WWT_SCROLL,			1,	155,	299,	68,		80,		SCROLL_HORIZONTAL,		STR_NONE }, 					// Sound effect volume
	{ WWT_SCROLL,			1,	155,	299,	83,		95,		SCROLL_HORIZONTAL,		STR_NONE }, 					// Music volume
	{ WWT_SCROLL,			1,	155,	299,	53,		65,		SCROLL_HORIZONTAL,		STR_NONE }, 					// Master volume (n3ds port)
	{ WIDGETS_END },
};

static rct_widget window_options_controls_and_interface_widgets[] = {
	MAIN_OPTIONS_WIDGETS,
	{ WWT_CHECKBOX,			2,	10,		299,	98,			109,	STR_INVERT_RIGHT_MOUSE_DRAG,			STR_INVERT_RIGHT_MOUSE_DRAG_TIP },			// Invert right mouse dragging
	{ WWT_DROPDOWN,			1,	155,	299,	162,		173,	STR_NONE,								STR_NONE },									// Themes
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	163,		172,	STR_DROPDOWN_GLYPH,						STR_CURRENT_THEME_TIP },
	{ WWT_DROPDOWN_BUTTON,	1,	10,		145,	178,		189,	STR_EDIT_THEMES_BUTTON,					STR_EDIT_THEMES_BUTTON_TIP },				// Themes button
	{ WWT_DROPDOWN_BUTTON,	1,	26,		185,	128,		139,	STR_HOTKEY,								STR_HOTKEY_TIP },							// Set hotkeys buttons

	{ WWT_CHECKBOX,			2,	10,		299,	269,		280,	STR_SELECT_BY_TRACK_TYPE,				STR_SELECT_BY_TRACK_TYPE_TIP },				// Select by track type
	{ WWT_DROPDOWN,			2,	155,	299,	284,		295,	STR_NONE,								STR_NONE },									// Scenario select mode
	{ WWT_DROPDOWN_BUTTON,	2,	288,	298,	285,		294,	STR_DROPDOWN_GLYPH,						STR_SCENARIO_GROUPING_TIP },
	{ WWT_CHECKBOX,			2,	18,		299,	299,		310,	STR_OPTIONS_SCENARIO_UNLOCKING,			STR_SCENARIO_UNLOCKING_TIP },				// Unlocking of scenarios
	{ WIDGETS_END },
};

static rct_widget window_options_misc_widgets[] = {
	MAIN_OPTIONS_WIDGETS,
	{ WWT_CHECKBOX,			2,	10,		299,	54,		65,		STR_REAL_NAME,								STR_REAL_NAME_TIP },								// Show 'real' names of guests
	{ WWT_CHECKBOX,			2,	10,		299,	84,		95,		STR_SAVE_PLUGIN_DATA,						STR_SAVE_PLUGIN_DATA_TIP },							// Export plug-in objects with saved games
	{ WWT_DROPDOWN,			1,	155,	299,	144,	155,	STR_NONE,									STR_NONE },											// Autosave dropdown
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	145,	154,	STR_DROPDOWN_GLYPH,							STR_AUTOSAVE_FREQUENCY_TIP },						// Autosave dropdown button
	{ WWT_DROPDOWN,			1,	155,	299,	159,	170,	STR_NONE,									STR_NONE },											// Title sequence dropdown
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	160,	169,	STR_DROPDOWN_GLYPH,							STR_TITLE_SEQUENCE_TIP },							// Title seqeuence dropdown button
	{ WWT_CHECKBOX,			2,	10,		299,	189,	200,	STR_AUTO_STAFF_PLACEMENT,					STR_AUTO_STAFF_PLACEMENT_TIP },						// Auto staff placement
	{ WWT_CHECKBOX,			2,	10,		299,	204,	215,	STR_HANDYMEN_MOW_BY_DEFAULT,				STR_HANDYMEN_MOW_BY_DEFAULT_TIP },					// Handymen mow by default
	{ WWT_CHECKBOX,			2,	10,		299,	219,	230,	STR_AUTO_OPEN_SHOPS,						STR_AUTO_OPEN_SHOPS_TIP },							// Automatically open shops & stalls
	{ WWT_DROPDOWN,			1,	155,	299,	234,	245,	STR_NONE,									STR_NONE },											// Default inspection time dropdown
	{ WWT_DROPDOWN_BUTTON,	1,	288,	298,	235,	244,	STR_DROPDOWN_GLYPH,							STR_DEFAULT_INSPECTION_INTERVAL_TIP },				// Default inspection time dropdown button
	{ WIDGETS_END },
};

rct_widget *window_options_page_widgets[] = {
	window_options_rendering_widgets,
	window_options_culture_widgets,
	window_options_audio_widgets,
	window_options_controls_and_interface_widgets,
	window_options_misc_widgets
};

// A widget that is not for the 3DS: out of the window, above it. What the paint function draws
// beside such a widget goes there with it (it takes its positions from the widgets) and is cut
// off, and window_options_invalidate, which sets the types of the widgets again every time,
// finds them by where they are and takes them out again.
#define N3DS_OPTIONS_HIDDEN_Y -30

static void n3ds_hide_widget(rct_widget *widget)
{
	widget->type = WWT_EMPTY;
	widget->top = N3DS_OPTIONS_HIDDEN_Y;
	widget->bottom = N3DS_OPTIONS_HIDDEN_Y;
}

// The options are in rows of a height for a finger
#define N3DS_OPTIONS_ROW(n) (52 + (n) * 20)

static void n3ds_set_row(rct_widget *widget, int left, int right, int row)
{
	widget->left = left;
	widget->right = right;
	widget->top = N3DS_OPTIONS_ROW(row);
	widget->bottom = N3DS_OPTIONS_ROW(row) + N3DS_CONTROL_HEIGHT - 1;
}

// A volume slider at the right of its row (a scrollbar: it keeps its height)
static void n3ds_set_slider(rct_widget *widget, int row)
{
	widget->left = 155;
	widget->right = WW - 7;
	widget->top = N3DS_OPTIONS_ROW(row) + 3;
	widget->bottom = N3DS_OPTIONS_ROW(row) + 15;
}

// A dropdown at the right of its row; paint draws its label at the left
static void n3ds_set_dropdown_row(rct_widget *widget, int row)
{
	window_n3ds_place_dropdown(widget, 155, WW - 7, N3DS_OPTIONS_ROW(row));
}

// n3ds port: the languages the 3DS can draw, as their LANGUAGE_ numbers, in the order of the
// language dropdown: those of the game's sprite font, and Korean, whose font is built into the
// program (platform/n3ds_font.c). The others need a TrueType font of a PC. Returns how many.
static int window_options_n3ds_languages(int *languages)
{
	int count = 0;
	for (int i = 1; i < LANGUAGE_COUNT; i++) {
		if (LanguagesDescriptors[i].font == FONT_OPENRCT2_SPRITE || i == LANGUAGE_KOREAN)
			languages[count++] = i;
	}
	return count;
}

/**
 * n3ds port: the options that mean something on the 3DS, in a window that fills the bottom
 * screen (the user's decision; 310x332 on a PC). Left out:
 * the languages that the 3DS cannot draw (window_options_n3ds_languages), autosave (off on the
 * 3DS), and the title sequence (the 3DS plays OpenRCT2's: TitleScreen.cpp TitleInitialise).
 */
static void window_options_n3ds_layout()
{
	rct_widget *widgets;
	int row;

	// Rendering
	widgets = window_options_rendering_widgets;
	n3ds_hide_widget(&widgets[WIDX_RENDERING_GROUP]);
	row = 0;
	n3ds_set_row(&widgets[WIDX_TILE_SMOOTHING_CHECKBOX], 10, WW - 12, row++);
	n3ds_set_row(&widgets[WIDX_GRIDLINES_CHECKBOX], 10, WW - 12, row++);
	n3ds_set_dropdown_row(&widgets[WIDX_CONSTRUCTION_MARKER], row++);
	n3ds_set_row(&widgets[WIDX_DAY_NIGHT_CHECKBOX], 10, WW - 12, row++);
	n3ds_set_row(&widgets[WIDX_UPPER_CASE_BANNERS_CHECKBOX], 10, WW - 12, row++);
	n3ds_set_row(&widgets[WIDX_RENDER_WEATHER_EFFECTS_CHECKBOX], 10, WW - 12, row++);
	n3ds_set_row(&widgets[WIDX_DISABLE_LIGHTNING_EFFECT_CHECKBOX], 31, WW - 12, row++);

	// Culture / units
	widgets = window_options_culture_widgets;
	row = 0;
	n3ds_set_dropdown_row(&widgets[WIDX_LANGUAGE], row++);
	n3ds_set_dropdown_row(&widgets[WIDX_CURRENCY], row++);
	n3ds_set_dropdown_row(&widgets[WIDX_DISTANCE], row++);
	n3ds_set_dropdown_row(&widgets[WIDX_TEMPERATURE], row++);
	n3ds_set_dropdown_row(&widgets[WIDX_HEIGHT_LABELS], row++);
	n3ds_set_dropdown_row(&widgets[WIDX_DATE_FORMAT], row++);

	// Audio: the master volume (its label is drawn by paint), then sound effects and ride music,
	// each with its volume beside it (a scrollbar: it keeps its height), and the title music
	widgets = window_options_audio_widgets;
	n3ds_set_slider(&widgets[WIDX_MASTER_VOLUME], 0);
	n3ds_set_row(&widgets[WIDX_SOUND_CHECKBOX], 10, 150, 1);
	n3ds_set_slider(&widgets[WIDX_SOUND_VOLUME], 1);
	n3ds_set_row(&widgets[WIDX_MUSIC_CHECKBOX], 10, 150, 2);
	n3ds_set_slider(&widgets[WIDX_MUSIC_VOLUME], 2);
	n3ds_set_dropdown_row(&widgets[WIDX_TITLE_MUSIC], 3);

	// Controls and interface
	widgets = window_options_controls_and_interface_widgets;
	row = 0;
	n3ds_set_row(&widgets[WIDX_INVERT_DRAG], 10, WW - 12, row++);
	n3ds_set_dropdown_row(&widgets[WIDX_THEMES], row++);
	n3ds_set_row(&widgets[WIDX_THEMES_BUTTON], 10, 160, row++);
	n3ds_set_row(&widgets[WIDX_HOTKEY_DROPDOWN], 10, 160, row++);
	n3ds_set_row(&widgets[WIDX_SELECT_BY_TRACK_TYPE], 10, WW - 12, row++);
	n3ds_set_dropdown_row(&widgets[WIDX_SCENARIO_GROUPING], row++);
	n3ds_set_row(&widgets[WIDX_SCENARIO_UNLOCKING], 18, WW - 12, row++);

	// Miscellaneous
	widgets = window_options_misc_widgets;
	n3ds_hide_widget(&widgets[WIDX_AUTOSAVE]);
	n3ds_hide_widget(&widgets[WIDX_AUTOSAVE_DROPDOWN]);
	row = 0;
	n3ds_set_row(&widgets[WIDX_REAL_NAME_CHECKBOX], 10, WW - 12, row++);
	n3ds_set_row(&widgets[WIDX_SAVE_PLUGIN_DATA_CHECKBOX], 10, WW - 12, row++);
	n3ds_set_row(&widgets[WIDX_AUTO_STAFF_PLACEMENT], 10, WW - 12, row++);
	n3ds_set_row(&widgets[WIDX_HANDYMEN_MOW_DEFAULT], 10, WW - 12, row++);
	n3ds_set_row(&widgets[WIDX_AUTO_OPEN_SHOPS], 10, WW - 12, row++);
	n3ds_set_dropdown_row(&widgets[WIDX_DEFAULT_INSPECTION_INTERVAL], row++);
	n3ds_hide_widget(&widgets[WIDX_TITLE_SEQUENCE]);
	n3ds_hide_widget(&widgets[WIDX_TITLE_SEQUENCE_DROPDOWN]);
}

#pragma endregion

static const rct_string_id window_options_autosave_names[6] = {
	STR_SAVE_EVERY_MINUTE,
	STR_SAVE_EVERY_5MINUTES,
	STR_SAVE_EVERY_15MINUTES,
	STR_SAVE_EVERY_30MINUTES,
	STR_SAVE_EVERY_HOUR,
	STR_SAVE_NEVER,
};

static const rct_string_id window_options_title_music_names[] = {
	STR_OPTIONS_MUSIC_VALUE_NONE ,
	STR_ROLLERCOASTER_TYCOON_1_DROPDOWN ,
	STR_ROLLERCOASTER_TYCOON_2_DROPDOWN ,
	STR_OPTIONS_MUSIC_VALUE_RANDOM,
};

const int window_options_tab_animation_divisor[] = { 4, 8, 2, 2, 2, 1 };
const int window_options_tab_animation_frames[] = { 8, 8, 16, 4, 16, 1 };

static void window_options_set_page(rct_window *w, int page);
static void window_options_set_pressed_tab(rct_window *w);
static void window_options_draw_tab_image(rct_drawpixelinfo *dpi, rct_window *w, int page, int spriteIndex);
static void window_options_draw_tab_images(rct_drawpixelinfo *dpi, rct_window *w);
static void window_options_show_dropdown(rct_window *w, rct_widget *widget, int num_items);
static void window_options_update_height_markers();

#pragma region Events

static void window_options_mouseup(rct_window *w, int widgetIndex);
static void window_options_mousedown(int widgetIndex, rct_window*w, rct_widget* widget);
static void window_options_dropdown(rct_window *w, int widgetIndex, int dropdownIndex);
static void window_options_update(rct_window *w);
static void window_options_invalidate(rct_window *w);
static void window_options_paint(rct_window *w, rct_drawpixelinfo *dpi);
static void window_options_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height);

static rct_window_event_list window_options_events = {
	NULL,
	window_options_mouseup,
	NULL,
	window_options_mousedown,
	window_options_dropdown,
	NULL,
	window_options_update,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	window_options_scrollgetsize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	window_options_invalidate,
	window_options_paint,
	NULL
};

#pragma endregion

#pragma region Enabled Widgets

#define MAIN_OPTIONS_ENABLED_WIDGETS \
	(1 << WIDX_CLOSE) | \
	(1 << WIDX_TAB_1) | \
	(1 << WIDX_TAB_2) | \
	(1 << WIDX_TAB_3) | \
	(1 << WIDX_TAB_4) | \
	(1 << WIDX_TAB_5)

static uint32 window_options_page_enabled_widgets[] = {
	MAIN_OPTIONS_ENABLED_WIDGETS |
	(1 << WIDX_TILE_SMOOTHING_CHECKBOX) |
	(1 << WIDX_GRIDLINES_CHECKBOX) |
	(1 << WIDX_CONSTRUCTION_MARKER) |
	(1 << WIDX_CONSTRUCTION_MARKER_DROPDOWN) |
	(1 << WIDX_DAY_NIGHT_CHECKBOX) |
	(1 << WIDX_UPPER_CASE_BANNERS_CHECKBOX) |
	(1 << WIDX_RENDER_WEATHER_EFFECTS_CHECKBOX) |
	(1 << WIDX_DISABLE_LIGHTNING_EFFECT_CHECKBOX),

	MAIN_OPTIONS_ENABLED_WIDGETS |
	(1 << WIDX_LANGUAGE) |
	(1 << WIDX_LANGUAGE_DROPDOWN) |
	(1 << WIDX_CURRENCY) |
	(1 << WIDX_CURRENCY_DROPDOWN) |
	(1 << WIDX_DISTANCE) |
	(1 << WIDX_DISTANCE_DROPDOWN) |
	(1 << WIDX_TEMPERATURE) |
	(1 << WIDX_TEMPERATURE_DROPDOWN) |
	(1 << WIDX_HEIGHT_LABELS) |
	(1 << WIDX_HEIGHT_LABELS_DROPDOWN) |
	(1 << WIDX_DATE_FORMAT) |
	(1 << WIDX_DATE_FORMAT_DROPDOWN),

	MAIN_OPTIONS_ENABLED_WIDGETS |
	(1 << WIDX_SOUND_CHECKBOX) |
	(1 << WIDX_MUSIC_CHECKBOX) |
	(1 << WIDX_TITLE_MUSIC) |
	(1 << WIDX_TITLE_MUSIC_DROPDOWN),

	MAIN_OPTIONS_ENABLED_WIDGETS |
	(1 << WIDX_INVERT_DRAG) |
	(1 << WIDX_THEMES) |
	(1 << WIDX_THEMES_DROPDOWN) |
	(1 << WIDX_THEMES_BUTTON) |
	(1 << WIDX_HOTKEY_DROPDOWN) |
	(1 << WIDX_SELECT_BY_TRACK_TYPE) |
	(1 << WIDX_SCENARIO_GROUPING) |
	(1 << WIDX_SCENARIO_GROUPING_DROPDOWN) |
	(1 << WIDX_SCENARIO_UNLOCKING),

	MAIN_OPTIONS_ENABLED_WIDGETS |
	(1 << WIDX_REAL_NAME_CHECKBOX) |
	(1 << WIDX_SAVE_PLUGIN_DATA_CHECKBOX) |
	(1 << WIDX_AUTOSAVE) |
	(1 << WIDX_AUTOSAVE_DROPDOWN) |
	(1 << WIDX_AUTO_STAFF_PLACEMENT) |
	(1 << WIDX_HANDYMEN_MOW_DEFAULT) |
	(1 << WIDX_TITLE_SEQUENCE) |
	(1 << WIDX_TITLE_SEQUENCE_DROPDOWN) |
	(1 << WIDX_AUTO_OPEN_SHOPS) |
	(1 << WIDX_DEFAULT_INSPECTION_INTERVAL) |
	(1 << WIDX_DEFAULT_INSPECTION_INTERVAL_DROPDOWN)
};

#pragma endregion

/**
*
*  rct2: 0x006BAC5B
*/
void window_options_open()
{
	rct_window* w;

	// Check if window is already open
	w = window_bring_to_front_by_class(WC_OPTIONS);
	if (w != NULL)
		return;

	window_options_n3ds_layout();
	w = window_create_centred(WW, WH, &window_options_events, WC_OPTIONS, 0);
	w->widgets = window_options_rendering_widgets;
	w->enabled_widgets = window_options_page_enabled_widgets[WINDOW_OPTIONS_PAGE_RENDERING];
	w->page = WINDOW_OPTIONS_PAGE_RENDERING;
	w->frame_no = 0;
	window_init_scroll_widgets(w);
	//window_invalidate(w);

	colour_scheme_update(w);
}

/**
*
*  rct2: 0x006BAFCA
*/
static void window_options_mouseup(rct_window *w, int widgetIndex)
{
	switch (widgetIndex) {
	case WIDX_CLOSE:
		window_close(w);
		break;
	case WIDX_TAB_1:
	case WIDX_TAB_2:
	case WIDX_TAB_3:
	case WIDX_TAB_4:
	case WIDX_TAB_5:
		window_options_set_page(w, widgetIndex - WIDX_TAB_1);
		break;
	}

	switch (w->page) {
	case WINDOW_OPTIONS_PAGE_RENDERING:
		switch (widgetIndex) {
		case WIDX_TILE_SMOOTHING_CHECKBOX:
			gConfigGeneral.landscape_smoothing ^= 1;
			config_save_default();
			gfx_invalidate_screen();
			break;
		case WIDX_GRIDLINES_CHECKBOX:
			gConfigGeneral.always_show_gridlines ^= 1;
			config_save_default();
			gfx_invalidate_screen();
			if ((w = window_get_main()) != NULL) {
				if (gConfigGeneral.always_show_gridlines)
					w->viewport->flags |= VIEWPORT_FLAG_GRIDLINES;
				else
					w->viewport->flags &= ~VIEWPORT_FLAG_GRIDLINES;
			}
			break;
		case WIDX_DAY_NIGHT_CHECKBOX:
			gConfigGeneral.day_night_cycle ^= 1;
			config_save_default();
			window_invalidate(w);
			break;
		case WIDX_UPPER_CASE_BANNERS_CHECKBOX:
			gConfigGeneral.upper_case_banners ^= 1;
			config_save_default();
			window_invalidate(w);
			break;
		case WIDX_DISABLE_LIGHTNING_EFFECT_CHECKBOX:
			gConfigGeneral.disable_lightning_effect ^= 1;
			config_save_default();
			window_invalidate(w);
			break;
		case WIDX_RENDER_WEATHER_EFFECTS_CHECKBOX:
			gConfigGeneral.render_weather_effects ^= 1;
			gConfigGeneral.render_weather_gloom = gConfigGeneral.render_weather_effects;
			config_save_default();
			window_invalidate(w);
			gfx_invalidate_screen();
			break;
		}
		break;

	case WINDOW_OPTIONS_PAGE_CULTURE:
		break;

	case WINDOW_OPTIONS_PAGE_AUDIO:
		switch (widgetIndex) {
		case WIDX_SOUND_CHECKBOX:
			gConfigSound.sound_enabled = !gConfigSound.sound_enabled;
			config_save_default();
			window_invalidate(w);
			break;
		case WIDX_MUSIC_CHECKBOX:
			gConfigSound.ride_music_enabled = !gConfigSound.ride_music_enabled;
			if (!gConfigSound.ride_music_enabled) {
				audio_stop_ride_music();
			}
			config_save_default();
			window_invalidate(w);
			break;
		}
		break;

	case WINDOW_OPTIONS_PAGE_CONTROLS_AND_INTERFACE:
		switch (widgetIndex) {
		case WIDX_INVERT_DRAG:
			gConfigGeneral.invert_viewport_drag ^= 1;
			config_save_default();
			window_invalidate(w);
			break;
		case WIDX_THEMES_BUTTON:
			window_themes_open();
			window_invalidate(w);
			break;
		case WIDX_HOTKEY_DROPDOWN:
			window_shortcut_keys_open();
			break;
		case WIDX_SELECT_BY_TRACK_TYPE:
			gConfigInterface.select_by_track_type ^= 1;
			config_save_default();
			window_invalidate(w);
			window_invalidate_by_class(WC_RIDE);
			window_invalidate_by_class(WC_CONSTRUCT_RIDE);
			break;
		case WIDX_SCENARIO_UNLOCKING:
			gConfigGeneral.scenario_unlocking_enabled ^= 1;
			config_save_default();
			window_close_by_class(WC_SCENARIO_SELECT);
			break;
		}
		break;

	case WINDOW_OPTIONS_PAGE_MISC:
		switch (widgetIndex) {
		case WIDX_REAL_NAME_CHECKBOX:
			peep_update_names(
				!(gParkFlags & PARK_FLAGS_SHOW_REAL_GUEST_NAMES)
			);
			break;
		case WIDX_SAVE_PLUGIN_DATA_CHECKBOX:
			gConfigGeneral.save_plugin_data ^= 1;
			config_save_default();
			window_invalidate(w);
			break;
		case WIDX_AUTO_STAFF_PLACEMENT:
			gConfigGeneral.auto_staff_placement ^= 1;
			config_save_default();
			window_invalidate(w);
			break;
		case WIDX_HANDYMEN_MOW_DEFAULT:
			gConfigGeneral.handymen_mow_default = !gConfigGeneral.handymen_mow_default;
			config_save_default();
			window_invalidate(w);
			break;
		case WIDX_AUTO_OPEN_SHOPS:
			gConfigGeneral.auto_open_shops = !gConfigGeneral.auto_open_shops;
			config_save_default();
			window_invalidate(w);
			break;
		}
		break;

	}
}

/**
*
*  rct2: 0x006BB01B
*/
static void window_options_mousedown(int widgetIndex, rct_window*w, rct_widget* widget)
{
	int num_items, i;

	widget = &w->widgets[widgetIndex - 1];

	switch (w->page) {
	case WINDOW_OPTIONS_PAGE_RENDERING:
		switch (widgetIndex) {
		case WIDX_CONSTRUCTION_MARKER_DROPDOWN:
			gDropdownItemsFormat[0] = STR_DROPDOWN_MENU_LABEL;
			gDropdownItemsFormat[1] = STR_DROPDOWN_MENU_LABEL;
			gDropdownItemsArgs[0] = STR_CONSTRUCTION_MARKER_COLOUR_WHITE;
			gDropdownItemsArgs[1] = STR_CONSTRUCTION_MARKER_COLOUR_TRANSLUCENT;

			window_options_show_dropdown(w, widget, 2);

			dropdown_set_checked(gConfigGeneral.construction_marker_colour, true);
			break;
		}
		break;

	case WINDOW_OPTIONS_PAGE_CULTURE:
		switch (widgetIndex) {
		case WIDX_HEIGHT_LABELS_DROPDOWN:
			gDropdownItemsFormat[0] = STR_DROPDOWN_MENU_LABEL;
			gDropdownItemsFormat[1] = STR_DROPDOWN_MENU_LABEL;
			gDropdownItemsArgs[0] = STR_UNITS;
			gDropdownItemsArgs[1] = STR_REAL_VALUES;

			window_options_show_dropdown(w, widget, 2);

			dropdown_set_checked(gConfigGeneral.show_height_as_units ? 0 : 1, true);
			break;
		case WIDX_CURRENCY_DROPDOWN:
			num_items = CURRENCY_END;

			for (i = 0; i < num_items; i++) {
				gDropdownItemsFormat[i] = STR_DROPDOWN_MENU_LABEL;
				gDropdownItemsArgs[i] = CurrencyDescriptors[i].stringId;
			}

			window_options_show_dropdown(w, widget, num_items);

			dropdown_set_checked(gConfigGeneral.currency_format, true);
			break;
		case WIDX_DISTANCE_DROPDOWN:
			gDropdownItemsFormat[0] = STR_DROPDOWN_MENU_LABEL;
			gDropdownItemsFormat[1] = STR_DROPDOWN_MENU_LABEL;
			gDropdownItemsFormat[2] = STR_DROPDOWN_MENU_LABEL;
			gDropdownItemsArgs[0] = STR_IMPERIAL;
			gDropdownItemsArgs[1] = STR_METRIC;
			gDropdownItemsArgs[2] = STR_SI;

			window_options_show_dropdown(w, widget, 3);

			dropdown_set_checked(gConfigGeneral.measurement_format, true);
			break;
		case WIDX_TEMPERATURE_DROPDOWN:
			gDropdownItemsFormat[0] = STR_DROPDOWN_MENU_LABEL;
			gDropdownItemsFormat[1] = STR_DROPDOWN_MENU_LABEL;
			gDropdownItemsArgs[0] = STR_CELSIUS;
			gDropdownItemsArgs[1] = STR_FAHRENHEIT;

			window_options_show_dropdown(w, widget, 2);

			dropdown_set_checked(gConfigGeneral.temperature_format, true);
			break;
		case WIDX_LANGUAGE_DROPDOWN:
			{
				// n3ds port: the languages the 3DS can draw
				int languages[LANGUAGE_COUNT];
				int count = window_options_n3ds_languages(languages);
				for (i = 0; i < count; i++) {
					gDropdownItemsFormat[i] = STR_OPTIONS_DROPDOWN_ITEM;
					gDropdownItemsArgs[i] = (uintptr_t)LanguagesDescriptors[languages[i]].native_name;
				}
				window_options_show_dropdown(w, widget, count);
				for (i = 0; i < count; i++) {
					if (languages[i] == gCurrentLanguage)
						dropdown_set_checked(i, true);
				}
			}
			break;
		case WIDX_DATE_FORMAT_DROPDOWN:
			for (i = 0; i < 4; i++) {
				gDropdownItemsFormat[i] = STR_DROPDOWN_MENU_LABEL;
				gDropdownItemsArgs[i] = DateFormatStringIds[i];
			}
			window_options_show_dropdown(w, widget, 4);
			dropdown_set_checked(gConfigGeneral.date_format, true);
			break;
		}
		break;

	case WINDOW_OPTIONS_PAGE_AUDIO:
		switch (widgetIndex) {
		case WIDX_TITLE_MUSIC_DROPDOWN:
			num_items = 4;

			for (i = 0; i < num_items ; i++) {
				gDropdownItemsFormat[i] = STR_DROPDOWN_MENU_LABEL;
				gDropdownItemsArgs[i] = window_options_title_music_names[i];
			}

			window_options_show_dropdown(w, widget, num_items);

			dropdown_set_checked(gConfigSound.title_music, true);
			break;
		}
		break;

	case WINDOW_OPTIONS_PAGE_CONTROLS_AND_INTERFACE:
		switch (widgetIndex) {
		case WIDX_THEMES_DROPDOWN:
			num_items = (int)theme_manager_get_num_available_themes();

			for (int i = 0; i < num_items; i++) {
				gDropdownItemsFormat[i] = STR_OPTIONS_DROPDOWN_ITEM;
				gDropdownItemsArgs[i] = (uintptr_t)theme_manager_get_available_theme_name(i);
			}

			window_dropdown_show_text_custom_width(
				w->x + widget->left,
				w->y + widget->top,
				widget->bottom - widget->top + 1,
				w->colours[1],
				DROPDOWN_FLAG_STAY_OPEN,
				num_items,
				widget->right - widget->left - 3
			);

			dropdown_set_checked((int)theme_manager_get_active_available_theme_index(), true);
			widget_invalidate(w, WIDX_THEMES_DROPDOWN);
			break;

		case WIDX_SCENARIO_GROUPING_DROPDOWN:
			num_items = 2;

			gDropdownItemsFormat[0] = STR_DROPDOWN_MENU_LABEL;
			gDropdownItemsArgs[0] = STR_OPTIONS_SCENARIO_DIFFICULTY;
			gDropdownItemsFormat[1] = STR_DROPDOWN_MENU_LABEL;
			gDropdownItemsArgs[1] = STR_OPTIONS_SCENARIO_ORIGIN;

			window_dropdown_show_text_custom_width(
				w->x + widget->left,
				w->y + widget->top,
				widget->bottom - widget->top + 1,
				w->colours[1],
				DROPDOWN_FLAG_STAY_OPEN,
				num_items,
				widget->right - widget->left - 3
			);

			dropdown_set_checked(gConfigGeneral.scenario_select_mode, true);
			break;
		}
		break;

	case WINDOW_OPTIONS_PAGE_MISC:
		switch (widgetIndex) {
		case WIDX_AUTOSAVE_DROPDOWN:
			for (i = AUTOSAVE_EVERY_MINUTE; i <= AUTOSAVE_NEVER; i++) {
				gDropdownItemsFormat[i] = STR_DROPDOWN_MENU_LABEL;
				gDropdownItemsArgs[i] = window_options_autosave_names[i];
			}

			window_options_show_dropdown(w, widget, AUTOSAVE_NEVER + 1);
			dropdown_set_checked(gConfigGeneral.autosave_frequency, true);
			break;
		case WIDX_TITLE_SEQUENCE_DROPDOWN:
			num_items = (int)title_sequence_manager_get_count();
			for (i = 0; i < num_items; i++) {
				gDropdownItemsFormat[i] = STR_OPTIONS_DROPDOWN_ITEM;
				gDropdownItemsArgs[i] = (uintptr_t)title_sequence_manager_get_name(i);
			}

			window_dropdown_show_text(
				w->x + widget->left,
				w->y + widget->top,
				widget->bottom - widget->top + 1,
				w->colours[1],
				DROPDOWN_FLAG_STAY_OPEN,
				num_items
			);

			dropdown_set_checked(gTitleCurrentSequence, true);
			break;
		case WIDX_DEFAULT_INSPECTION_INTERVAL_DROPDOWN:
			for (i = 0; i < 7; i++) {
				gDropdownItemsFormat[i] = STR_DROPDOWN_MENU_LABEL;
				gDropdownItemsArgs[i] = RideInspectionIntervalNames[i];
			}

			window_options_show_dropdown(w, widget, 7);
			dropdown_set_checked(gConfigGeneral.default_inspection_interval, true);
			break;
		}
		break;

	}
}

/**
*
*  rct2: 0x006BB076
*/
static void window_options_dropdown(rct_window *w, int widgetIndex, int dropdownIndex)
{
	if (dropdownIndex == -1)
		return;

	switch (w->page) {
	case WINDOW_OPTIONS_PAGE_RENDERING:
		switch (widgetIndex) {
		case WIDX_CONSTRUCTION_MARKER_DROPDOWN:
			if (dropdownIndex != gConfigGeneral.construction_marker_colour) {
				gConfigGeneral.construction_marker_colour = (uint8)dropdownIndex;
				config_save_default();
				gfx_invalidate_screen();
			}
			break;
		}
		break;

	case WINDOW_OPTIONS_PAGE_CULTURE:
		switch (widgetIndex) {
		case WIDX_HEIGHT_LABELS_DROPDOWN:
			// reset flag and set it to 1 if height as units is selected
			gConfigGeneral.show_height_as_units = 0;

			if (dropdownIndex == 0) {
				gConfigGeneral.show_height_as_units = 1;
			}
			config_save_default();
			window_options_update_height_markers();
			break;
		case WIDX_CURRENCY_DROPDOWN:
			gConfigGeneral.currency_format = (sint8)dropdownIndex;
			config_save_default();
			gfx_invalidate_screen();
			break;
		case WIDX_DISTANCE_DROPDOWN:
			gConfigGeneral.measurement_format = (sint8)dropdownIndex;
			config_save_default();
			window_options_update_height_markers();
			break;
		case WIDX_TEMPERATURE_DROPDOWN:
			if (dropdownIndex != gConfigGeneral.temperature_format) {
				gConfigGeneral.temperature_format = (sint8)dropdownIndex;
				config_save_default();
				gfx_invalidate_screen();
			}
			break;
		case WIDX_LANGUAGE_DROPDOWN:
			{
				// n3ds port: from the place in the dropdown, which lists the languages the 3DS
				// can draw, to the place among all languages, which the code below goes by
				int n3dsLanguages[LANGUAGE_COUNT];
				if (dropdownIndex >= window_options_n3ds_languages(n3dsLanguages))
					break;
				dropdownIndex = n3dsLanguages[dropdownIndex] - 1;
				int fallbackLanguage = gCurrentLanguage;
				if (dropdownIndex != gCurrentLanguage - 1) {
					if (!language_open(dropdownIndex + 1))
					{
						// Failed to open language file, try to recover by falling
						// back to previously used language
						if (language_open(fallbackLanguage))
						{
							// It worked, so we can say it with error message in-game
							window_error_open(STR_LANGUAGE_LOAD_FAILED, STR_NONE);
						}
						// report error to console regardless
						log_error("Failed to open language file.");
						dropdownIndex = fallbackLanguage - 1;
					} else {
						gConfigGeneral.language = dropdownIndex + 1;
						config_save_default();
						gfx_invalidate_screen();
					}
				}
			}
			break;
		case WIDX_DATE_FORMAT_DROPDOWN:
			if (dropdownIndex != gConfigGeneral.date_format) {
				gConfigGeneral.date_format = (uint8)dropdownIndex;
				config_save_default();
				gfx_invalidate_screen();
			}
			break;
		}
		break;

	case WINDOW_OPTIONS_PAGE_AUDIO:
		switch (widgetIndex) {
		case WIDX_TITLE_MUSIC_DROPDOWN:
			if ((dropdownIndex == 1 || dropdownIndex == 3) && !platform_file_exists(get_file_path(PATH_ID_CSS50))) {
				window_error_open(STR_OPTIONS_MUSIC_ERR_CSS50_NOT_FOUND, STR_OPTIONS_MUSIC_ERR_CSS50_NOT_FOUND_HINT);
			}
			else {
				gConfigSound.title_music = (sint8)dropdownIndex;
				config_save_default();
				window_invalidate(w);
			}

			audio_stop_title_music();
			if (dropdownIndex != 0)
				audio_start_title_music();
			break;
		}
		break;

	case WINDOW_OPTIONS_PAGE_CONTROLS_AND_INTERFACE:
		switch (widgetIndex) {
		case WIDX_THEMES_DROPDOWN:
			if (dropdownIndex != -1) {
				theme_manager_set_active_available_theme(dropdownIndex);
			}
			config_save_default();
			break;
		case WIDX_SCENARIO_GROUPING_DROPDOWN:
			if (dropdownIndex != gConfigGeneral.scenario_select_mode) {
				gConfigGeneral.scenario_select_mode = dropdownIndex;
				config_save_default();
				window_invalidate(w);
				window_close_by_class(WC_SCENARIO_SELECT);
			}
			break;
		}
		break;

	case WINDOW_OPTIONS_PAGE_MISC:
		switch (widgetIndex) {
		case WIDX_AUTOSAVE_DROPDOWN:
			if (dropdownIndex != gConfigGeneral.autosave_frequency) {
				gConfigGeneral.autosave_frequency = (uint8)dropdownIndex;
				config_save_default();
				window_invalidate(w);
			}
			break;
		case WIDX_TITLE_SEQUENCE_DROPDOWN:
			if (dropdownIndex != gTitleCurrentSequence) {
				title_sequence_change_preset(dropdownIndex);
				config_save_default();
				window_invalidate(w);
			}
			break;
		case WIDX_DEFAULT_INSPECTION_INTERVAL_DROPDOWN:
			if (dropdownIndex != gConfigGeneral.default_inspection_interval) {
				gConfigGeneral.default_inspection_interval = (uint8)dropdownIndex;
				config_save_default();
				window_invalidate(w);
			}
			break;
		}
		break;

	}
}

/**
*
*  rct2: 0x006BAD48
*/
static void window_options_invalidate(rct_window *w)
{
	rct_widget* widget;

	colour_scheme_update(w);

	if (window_options_page_widgets[w->page] != w->widgets) {
		w->widgets = window_options_page_widgets[w->page];
		window_init_scroll_widgets(w);
	}
	window_options_set_pressed_tab(w);
	w->disabled_widgets = 0;

	switch (w->page) {
	case WINDOW_OPTIONS_PAGE_RENDERING:
		widget_set_checkbox_value(w, WIDX_TILE_SMOOTHING_CHECKBOX, gConfigGeneral.landscape_smoothing);
		widget_set_checkbox_value(w, WIDX_GRIDLINES_CHECKBOX, gConfigGeneral.always_show_gridlines);
		widget_set_checkbox_value(w, WIDX_DAY_NIGHT_CHECKBOX, gConfigGeneral.day_night_cycle);
		widget_set_checkbox_value(w, WIDX_UPPER_CASE_BANNERS_CHECKBOX, gConfigGeneral.upper_case_banners);
		widget_set_checkbox_value(w, WIDX_RENDER_WEATHER_EFFECTS_CHECKBOX, gConfigGeneral.render_weather_effects || gConfigGeneral.render_weather_gloom);
		widget_set_checkbox_value(w, WIDX_DISABLE_LIGHTNING_EFFECT_CHECKBOX, gConfigGeneral.disable_lightning_effect);
		if (!gConfigGeneral.render_weather_effects && !gConfigGeneral.render_weather_gloom) {
			widget_set_checkbox_value(w, WIDX_DISABLE_LIGHTNING_EFFECT_CHECKBOX, true);
			w->enabled_widgets &= ~(1 << WIDX_DISABLE_LIGHTNING_EFFECT_CHECKBOX);
			w->disabled_widgets |= (1 << WIDX_DISABLE_LIGHTNING_EFFECT_CHECKBOX);
		}
		else {
			w->enabled_widgets |= (1 << WIDX_DISABLE_LIGHTNING_EFFECT_CHECKBOX);
			w->disabled_widgets &= ~(1 << WIDX_DISABLE_LIGHTNING_EFFECT_CHECKBOX);
		}
		// construction marker: white/translucent
		static const rct_string_id construction_marker_colours[] = {
			STR_CONSTRUCTION_MARKER_COLOUR_WHITE,
			STR_CONSTRUCTION_MARKER_COLOUR_TRANSLUCENT,
		};
		window_options_rendering_widgets[WIDX_CONSTRUCTION_MARKER].text = construction_marker_colours[gConfigGeneral.construction_marker_colour];

		window_options_rendering_widgets[WIDX_TILE_SMOOTHING_CHECKBOX].type = WWT_CHECKBOX;
		window_options_rendering_widgets[WIDX_GRIDLINES_CHECKBOX].type = WWT_CHECKBOX;
		window_options_rendering_widgets[WIDX_CONSTRUCTION_MARKER].type = WWT_DROPDOWN;
		window_options_rendering_widgets[WIDX_CONSTRUCTION_MARKER_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		window_options_rendering_widgets[WIDX_DAY_NIGHT_CHECKBOX].type = WWT_CHECKBOX;
		window_options_rendering_widgets[WIDX_UPPER_CASE_BANNERS_CHECKBOX].type = WWT_CHECKBOX;
		window_options_rendering_widgets[WIDX_RENDER_WEATHER_EFFECTS_CHECKBOX].type = WWT_CHECKBOX;
		window_options_rendering_widgets[WIDX_DISABLE_LIGHTNING_EFFECT_CHECKBOX].type = WWT_CHECKBOX;
		break;

	case WINDOW_OPTIONS_PAGE_CULTURE:
		// currency: pounds, dollars, etc. (10 total)
		set_format_arg(12, rct_string_id, CurrencyDescriptors[gConfigGeneral.currency_format].stringId);

		// distance: metric / imperial / si
		{
			rct_string_id stringId;
			switch (gConfigGeneral.measurement_format) {
			default:
			case MEASUREMENT_FORMAT_IMPERIAL: stringId = STR_IMPERIAL; break;
			case MEASUREMENT_FORMAT_METRIC: stringId = STR_METRIC; break;
			case MEASUREMENT_FORMAT_SI: stringId = STR_SI; break;
			}
			set_format_arg(14, rct_string_id, stringId);
		}

		// temperature: celsius/fahrenheit
		set_format_arg(20, rct_string_id, (gConfigGeneral.temperature_format == TEMPERATURE_FORMAT_F) ? STR_FAHRENHEIT : STR_CELSIUS);

		// height: units/real values
		set_format_arg(6, rct_string_id, gConfigGeneral.show_height_as_units ? STR_UNITS : STR_REAL_VALUES);

		window_options_culture_widgets[WIDX_LANGUAGE].type = WWT_DROPDOWN;
		window_options_culture_widgets[WIDX_LANGUAGE_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		window_options_culture_widgets[WIDX_CURRENCY].type = WWT_DROPDOWN;
		window_options_culture_widgets[WIDX_CURRENCY_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		window_options_culture_widgets[WIDX_DISTANCE].type = WWT_DROPDOWN;
		window_options_culture_widgets[WIDX_DISTANCE_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		window_options_culture_widgets[WIDX_TEMPERATURE].type = WWT_DROPDOWN;
		window_options_culture_widgets[WIDX_TEMPERATURE_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		window_options_culture_widgets[WIDX_HEIGHT_LABELS].type = WWT_DROPDOWN;
		window_options_culture_widgets[WIDX_HEIGHT_LABELS_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		window_options_culture_widgets[WIDX_DATE_FORMAT].type = WWT_DROPDOWN;
		window_options_culture_widgets[WIDX_DATE_FORMAT_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		break;

	case WINDOW_OPTIONS_PAGE_AUDIO:
		// music: on/off
		set_format_arg(8, rct_string_id, gConfigSound.ride_music_enabled ? STR_OPTIONS_RIDE_MUSIC_ON : STR_OPTIONS_RIDE_MUSIC_OFF);

		widget_set_checkbox_value(w, WIDX_SOUND_CHECKBOX, gConfigSound.sound_enabled);
		widget_set_checkbox_value(w, WIDX_MUSIC_CHECKBOX, gConfigSound.ride_music_enabled);

		if(w->frame_no == 0){ // initialize only on first frame, otherwise the scrollbars wont be able to be modified
			widget = &window_options_audio_widgets[WIDX_SOUND_VOLUME];
			w->scrolls[0].h_left = (sint16)ceil((gConfigSound.sound_volume / 100.0f) * (w->scrolls[0].h_right - ((widget->right - widget->left) - 1)));
			widget = &window_options_audio_widgets[WIDX_MUSIC_VOLUME];
			w->scrolls[1].h_left = (sint16)ceil((gConfigSound.ride_music_volume / 100.0f) * (w->scrolls[1].h_right - ((widget->right - widget->left) - 1)));
			widget = &window_options_audio_widgets[WIDX_MASTER_VOLUME];
			w->scrolls[2].h_left = (sint16)ceil((gConfigSound.master_volume / 100.0f) * (w->scrolls[2].h_right - ((widget->right - widget->left) - 1)));
		}

		widget_scroll_update_thumbs(w, WIDX_SOUND_VOLUME);
		widget_scroll_update_thumbs(w, WIDX_MUSIC_VOLUME);
		widget_scroll_update_thumbs(w, WIDX_MASTER_VOLUME);

		window_options_audio_widgets[WIDX_SOUND_CHECKBOX].type = WWT_CHECKBOX;
		window_options_audio_widgets[WIDX_MUSIC_CHECKBOX].type = WWT_CHECKBOX;
		window_options_audio_widgets[WIDX_TITLE_MUSIC].type = WWT_DROPDOWN;
		window_options_audio_widgets[WIDX_TITLE_MUSIC_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		window_options_audio_widgets[WIDX_SOUND_VOLUME].type = WWT_SCROLL;
		window_options_audio_widgets[WIDX_MUSIC_VOLUME].type = WWT_SCROLL;
		window_options_audio_widgets[WIDX_MASTER_VOLUME].type = WWT_SCROLL;
		break;

	case WINDOW_OPTIONS_PAGE_CONTROLS_AND_INTERFACE:
		widget_set_checkbox_value(w, WIDX_INVERT_DRAG, gConfigGeneral.invert_viewport_drag);
		widget_set_checkbox_value(w, WIDX_SELECT_BY_TRACK_TYPE, gConfigInterface.select_by_track_type);
		widget_set_checkbox_value(w, WIDX_SCENARIO_UNLOCKING, gConfigGeneral.scenario_unlocking_enabled);

		if (gConfigGeneral.scenario_select_mode == SCENARIO_SELECT_MODE_ORIGIN) {
			w->disabled_widgets &= ~(1ULL << WIDX_SCENARIO_UNLOCKING);
		} else {
			w->disabled_widgets |= (1ULL << WIDX_SCENARIO_UNLOCKING);
		}

		window_options_controls_and_interface_widgets[WIDX_THEMES].type = WWT_DROPDOWN;
		window_options_controls_and_interface_widgets[WIDX_THEMES_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		window_options_controls_and_interface_widgets[WIDX_THEMES_BUTTON].type = WWT_DROPDOWN_BUTTON;
		window_options_controls_and_interface_widgets[WIDX_HOTKEY_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		window_options_controls_and_interface_widgets[WIDX_SELECT_BY_TRACK_TYPE].type = WWT_CHECKBOX;
		window_options_controls_and_interface_widgets[WIDX_SCENARIO_GROUPING].type = WWT_DROPDOWN;
		window_options_controls_and_interface_widgets[WIDX_SCENARIO_UNLOCKING].type = WWT_CHECKBOX;
		break;

	case WINDOW_OPTIONS_PAGE_MISC:
		// unknown park flag can disable real name checkbox
		if (gParkFlags & PARK_FLAGS_LOCK_REAL_NAMES_OPTION)
			w->disabled_widgets |= (1ULL << WIDX_REAL_NAME_CHECKBOX);

		// save plugin data checkbox: visible or not
		window_options_misc_widgets[WIDX_SAVE_PLUGIN_DATA_CHECKBOX].type = WWT_CHECKBOX;

		widget_set_checkbox_value(w, WIDX_REAL_NAME_CHECKBOX, gParkFlags & PARK_FLAGS_SHOW_REAL_GUEST_NAMES);
		widget_set_checkbox_value(w, WIDX_SAVE_PLUGIN_DATA_CHECKBOX, gConfigGeneral.save_plugin_data);
		widget_set_checkbox_value(w, WIDX_AUTO_STAFF_PLACEMENT, gConfigGeneral.auto_staff_placement);
		widget_set_checkbox_value(w, WIDX_HANDYMEN_MOW_DEFAULT, gConfigGeneral.handymen_mow_default);
		widget_set_checkbox_value(w, WIDX_AUTO_OPEN_SHOPS, gConfigGeneral.auto_open_shops);

		window_options_misc_widgets[WIDX_REAL_NAME_CHECKBOX].type = WWT_CHECKBOX;
		window_options_misc_widgets[WIDX_SAVE_PLUGIN_DATA_CHECKBOX].type = WWT_CHECKBOX;
		window_options_misc_widgets[WIDX_AUTOSAVE].type = WWT_DROPDOWN;
		window_options_misc_widgets[WIDX_AUTOSAVE_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		window_options_misc_widgets[WIDX_AUTO_STAFF_PLACEMENT].type = WWT_CHECKBOX;
		window_options_misc_widgets[WIDX_HANDYMEN_MOW_DEFAULT].type = WWT_CHECKBOX;
		window_options_misc_widgets[WIDX_TITLE_SEQUENCE].type = WWT_DROPDOWN;
		window_options_misc_widgets[WIDX_TITLE_SEQUENCE_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		window_options_misc_widgets[WIDX_AUTO_OPEN_SHOPS].type = WWT_CHECKBOX;
		window_options_misc_widgets[WIDX_DEFAULT_INSPECTION_INTERVAL].type = WWT_DROPDOWN;
		window_options_misc_widgets[WIDX_DEFAULT_INSPECTION_INTERVAL_DROPDOWN].type = WWT_DROPDOWN_BUTTON;
		break;

	}

	// n3ds port: the widgets that are not for the 3DS, shown again by the code above, are
	// taken out again (window_options_n3ds_layout). The window keeps the height of the screen.
	for (widget = &w->widgets[WIDX_PAGE_START]; widget->type != WWT_LAST; widget++) {
		if (widget->bottom == N3DS_OPTIONS_HIDDEN_Y)
			widget->type = WWT_EMPTY;
	}
}

// n3ds port: the volumes changed by the sliders are applied at once but written to the settings
// file only when the finger (or the mouse button) is off the slider: a write takes about 40 ms on
// a 3DS, and writing at every step of a swipe stalled the game (user's report: the title's top
// screen stuttered while a volume was swiped)
static bool _n3dsVolumeUnsaved = false;

static void window_options_update(rct_window *w)
{
	// Tab animation
	w->frame_no++;
	widget_invalidate(w, WIDX_TAB_1 + w->page);

	rct_widget *widget;
	if (w->page == WINDOW_OPTIONS_PAGE_AUDIO) {
		widget = &window_options_audio_widgets[WIDX_SOUND_VOLUME];
		uint8 sound_volume = (uint8)(((float)w->scrolls[0].h_left / (w->scrolls[0].h_right - ((widget->right - widget->left) - 1))) * 100);
		widget = &window_options_audio_widgets[WIDX_MUSIC_VOLUME];
		uint8 ride_music_volume = (uint8)(((float)w->scrolls[1].h_left / (w->scrolls[1].h_right - ((widget->right - widget->left) - 1))) * 100);
		widget = &window_options_audio_widgets[WIDX_MASTER_VOLUME];
		uint8 master_volume = (uint8)(((float)w->scrolls[2].h_left / (w->scrolls[2].h_right - ((widget->right - widget->left) - 1))) * 100);
		if (sound_volume != gConfigSound.sound_volume) {
			gConfigSound.sound_volume = sound_volume;
			_n3dsVolumeUnsaved = true;
		}
		if (ride_music_volume != gConfigSound.ride_music_volume) {
			gConfigSound.ride_music_volume = ride_music_volume;
			_n3dsVolumeUnsaved = true;
		}
		if (master_volume != gConfigSound.master_volume) {
			gConfigSound.master_volume = master_volume;
			_n3dsVolumeUnsaved = true;
		}
		if (_n3dsVolumeUnsaved && !platform_n3ds_touch_active() && gInputState != INPUT_STATE_SCROLL_LEFT) {
			_n3dsVolumeUnsaved = false;
			config_save_default();
		}
		widget_invalidate(w, WIDX_SOUND_VOLUME);
		widget_invalidate(w, WIDX_MUSIC_VOLUME);
		widget_invalidate(w, WIDX_MASTER_VOLUME);
	}
}

/**
*
*  rct2: 0x006BAEB4
*/
static void window_options_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	window_draw_widgets(w, dpi);
	window_options_draw_tab_images(dpi, w);

	// n3ds port: the labels and values below are drawn at the top of their dropdown's box, which
	// is higher on the 3DS (window_options_n3ds_layout). They are all moved down to the middle
	// of it by drawing them into a view of the screen that starts that much higher.
	rct_drawpixelinfo n3dsLoweredDpi = *dpi;
	n3dsLoweredDpi.y -= N3DS_CONTROL_TEXT_OFFSET;
	dpi = &n3dsLoweredDpi;

	switch (w->page) {
	case WINDOW_OPTIONS_PAGE_RENDERING:
		gfx_draw_string_left(dpi, STR_CONSTRUCTION_MARKER, w, w->colours[1], w->x + 10, w->y + window_options_rendering_widgets[WIDX_CONSTRUCTION_MARKER].top + 1);
		break;
	case WINDOW_OPTIONS_PAGE_CULTURE:
		gfx_draw_string_left(dpi, STR_OPTIONS_LANGUAGE, w, w->colours[1], w->x + 10, w->y + window_options_culture_widgets[WIDX_LANGUAGE].top + 1);

		gCurrentFontSpriteBase = FONT_SPRITE_BASE_MEDIUM;
		gfx_draw_string(
			dpi,
			(char*)LanguagesDescriptors[gCurrentLanguage].native_name,
			w->colours[1],
			w->x + window_options_culture_widgets[WIDX_LANGUAGE].left + 1,
			w->y + window_options_culture_widgets[WIDX_LANGUAGE].top
		);

		gfx_draw_string_left(dpi, STR_CURRENCY, w, w->colours[1], w->x + 10, w->y + window_options_culture_widgets[WIDX_CURRENCY].top + 1);
		gfx_draw_string_left(dpi, STR_DISTANCE_AND_SPEED, w, w->colours[1], w->x + 10, w->y + window_options_culture_widgets[WIDX_DISTANCE].top + 1);
		gfx_draw_string_left(dpi, STR_TEMPERATURE, w, w->colours[1], w->x + 10, w->y + window_options_culture_widgets[WIDX_TEMPERATURE].top + 1);
		gfx_draw_string_left(dpi, STR_HEIGHT_LABELS, w, w->colours[1], w->x + 10, w->y + window_options_culture_widgets[WIDX_HEIGHT_LABELS].top + 1);
		gfx_draw_string_left(dpi, STR_DATE_FORMAT, w, w->colours[1], w->x + 10, w->y + window_options_culture_widgets[WIDX_DATE_FORMAT].top + 1);
		gfx_draw_string_left(
			dpi,
			DateFormatStringIds[gConfigGeneral.date_format],
			NULL,
			w->colours[1],
			w->x + window_options_culture_widgets[WIDX_DATE_FORMAT].left + 1,
			w->y + window_options_culture_widgets[WIDX_DATE_FORMAT].top
		);
		break;
	case WINDOW_OPTIONS_PAGE_AUDIO:
	{
		// n3ds port: the master volume's label (upstream #7848's STR_MASTER_VOLUME is not in 0.0.5's
		// language files: the text is here, in English and Korean)
		const char *masterLabel = gCurrentLanguage == LANGUAGE_KOREAN ? "마스터 음량:" : "Master volume:";
		gfx_draw_string(dpi, (char *)masterLabel, w->colours[1], w->x + 10, w->y + window_options_audio_widgets[WIDX_MASTER_VOLUME].top - 2);
		gfx_draw_string_left(dpi, STR_OPTIONS_MUSIC_LABEL, w, w->colours[1], w->x + 10, w->y + window_options_audio_widgets[WIDX_TITLE_MUSIC].top + 1);
		gfx_draw_string_left(
			dpi,
			window_options_title_music_names[gConfigSound.title_music],
			NULL,
			w->colours[1],
			w->x + window_options_audio_widgets[WIDX_TITLE_MUSIC].left + 1,
			w->y + window_options_audio_widgets[WIDX_TITLE_MUSIC].top
		);
		break;
	}
	case WINDOW_OPTIONS_PAGE_CONTROLS_AND_INTERFACE:
		size_t activeAvailableThemeIndex = theme_manager_get_active_available_theme_index();
		const utf8 * activeThemeName = theme_manager_get_available_theme_name(activeAvailableThemeIndex);
		set_format_arg(0, uintptr_t, (uintptr_t)activeThemeName);

		gfx_draw_string_left(dpi, STR_THEMES_LABEL_CURRENT_THEME, NULL, w->colours[1], w->x + 10, w->y + window_options_controls_and_interface_widgets[WIDX_THEMES].top + 1);
		gfx_draw_string_left_clipped(
			dpi,
			STR_STRING,
			gCommonFormatArgs,
			w->colours[1],
			w->x + window_options_controls_and_interface_widgets[WIDX_THEMES].left + 1,
			w->y + window_options_controls_and_interface_widgets[WIDX_THEMES].top,
			window_options_controls_and_interface_widgets[WIDX_THEMES_DROPDOWN].left - window_options_controls_and_interface_widgets[WIDX_THEMES].left - 4
		);
		gfx_draw_string_left(dpi, STR_OPTIONS_SCENARIO_GROUPING, NULL, w->colours[1], w->x + 10, w->y + window_options_controls_and_interface_widgets[WIDX_SCENARIO_GROUPING].top + 1);
		gfx_draw_string_left_clipped(
			dpi,
			gConfigGeneral.scenario_select_mode == SCENARIO_SELECT_MODE_DIFFICULTY ?
				STR_OPTIONS_SCENARIO_DIFFICULTY :
				STR_OPTIONS_SCENARIO_ORIGIN,
			NULL,
			w->colours[1],
			w->x + window_options_controls_and_interface_widgets[WIDX_SCENARIO_GROUPING].left + 1,
			w->y + window_options_controls_and_interface_widgets[WIDX_SCENARIO_GROUPING].top,
			window_options_controls_and_interface_widgets[WIDX_SCENARIO_GROUPING_DROPDOWN].left - window_options_controls_and_interface_widgets[WIDX_SCENARIO_GROUPING].left - 4
		);
		break;
	case WINDOW_OPTIONS_PAGE_MISC:
		gfx_draw_string_left(dpi, STR_OPTIONS_AUTOSAVE_FREQUENCY_LABEL, w, w->colours[1], w->x + 10, w->y + window_options_misc_widgets[WIDX_AUTOSAVE].top + 1);
		gfx_draw_string_left(
			dpi,
			window_options_autosave_names[gConfigGeneral.autosave_frequency],
			NULL,
			w->colours[1],
			w->x + window_options_misc_widgets[WIDX_AUTOSAVE].left + 1,
			w->y + window_options_misc_widgets[WIDX_AUTOSAVE].top
		);

		const utf8 * name = title_sequence_manager_get_name(gTitleCurrentSequence);
		set_format_arg(0, uintptr_t, (uintptr_t)name);
		gfx_draw_string_left(dpi, STR_TITLE_SEQUENCE, w, w->colours[1], w->x + 10, w->y + window_options_misc_widgets[WIDX_TITLE_SEQUENCE].top + 1);
		gfx_draw_string_left_clipped(
			dpi,
			STR_STRING,
			gCommonFormatArgs,
			w->colours[1],
			w->x + window_options_misc_widgets[WIDX_TITLE_SEQUENCE].left + 1,
			w->y + window_options_misc_widgets[WIDX_TITLE_SEQUENCE].top,
			window_options_misc_widgets[WIDX_TITLE_SEQUENCE_DROPDOWN].left - window_options_misc_widgets[WIDX_TITLE_SEQUENCE].left - 4
		);

		gfx_draw_string_left(dpi, STR_DEFAULT_INSPECTION_INTERVAL, w, w->colours[1], w->x + 10, w->y + window_options_misc_widgets[WIDX_DEFAULT_INSPECTION_INTERVAL].top + 1);
		gfx_draw_string_left(
			dpi,
			RideInspectionIntervalNames[gConfigGeneral.default_inspection_interval],
			NULL,
			w->colours[1],
			w->x + window_options_misc_widgets[WIDX_DEFAULT_INSPECTION_INTERVAL].left + 1,
			w->y + window_options_misc_widgets[WIDX_DEFAULT_INSPECTION_INTERVAL].top
		);
		break;
	}
}

// helper function, all dropdown boxes have similar properties
static void window_options_show_dropdown(rct_window *w, rct_widget *widget, int num_items)
{
	window_dropdown_show_text_custom_width(
		w->x + widget->left,
		w->y + widget->top,
		widget->bottom - widget->top + 1,
		w->colours[1],
		DROPDOWN_FLAG_STAY_OPEN,
		num_items,
		widget->right - widget->left - 3
	);
}

static void window_options_update_height_markers()
{
	config_save_default();
	gfx_invalidate_screen();
}

static void window_options_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height)
{
	if (w->page == WINDOW_OPTIONS_PAGE_AUDIO) {
		*width = 1000;
	}
}

#pragma region Common

static void window_options_set_page(rct_window *w, int page)
{
	w->page = page;
	w->frame_no = 0;
	w->enabled_widgets = window_options_page_enabled_widgets[page];
	w->pressed_widgets = 0;
	w->widgets = window_options_page_widgets[page];

	window_invalidate(w);
	window_event_resize_call(w);
	window_event_invalidate_call(w);
	window_init_scroll_widgets(w);
	window_invalidate(w);
}

static void window_options_set_pressed_tab(rct_window *w)
{
	int i;
	for (i = 0; i < WINDOW_OPTIONS_PAGE_COUNT; i++)
		w->pressed_widgets &= ~(1 << (WIDX_TAB_1 + i));
	w->pressed_widgets |= 1LL << (WIDX_TAB_1 + w->page);
}

static void window_options_draw_tab_image(rct_drawpixelinfo *dpi, rct_window *w, int page, int spriteIndex)
{
	int widgetIndex = WIDX_TAB_1 + page;

	if (!(w->disabled_widgets & (1LL << widgetIndex))) {
		if (w->page == page) {
			int frame = w->frame_no / window_options_tab_animation_divisor[w->page];
			spriteIndex += (frame % window_options_tab_animation_frames[w->page]);
		}

		gfx_draw_sprite(dpi, spriteIndex, w->x + w->widgets[widgetIndex].left, w->y + w->widgets[widgetIndex].top, 0);
	}
}

static void window_options_draw_tab_images(rct_drawpixelinfo *dpi, rct_window *w)
{
	window_options_draw_tab_image(dpi, w, WINDOW_OPTIONS_PAGE_RENDERING, SPR_TAB_PAINT_0);
	window_options_draw_tab_image(dpi, w, WINDOW_OPTIONS_PAGE_CULTURE, SPR_TAB_TIMER_0);
	window_options_draw_tab_image(dpi, w, WINDOW_OPTIONS_PAGE_AUDIO, SPR_TAB_MUSIC_0);
	window_options_draw_tab_image(dpi, w, WINDOW_OPTIONS_PAGE_CONTROLS_AND_INTERFACE, SPR_TAB_GEARS_0);
	window_options_draw_tab_image(dpi, w, WINDOW_OPTIONS_PAGE_MISC, SPR_TAB_WRENCH_0);
}

#pragma endregion
