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
#include "../input.h"
#include "../scenario/scenario.h"
#include "../sprites.h"
#include "../localisation/localisation.h"
#include "../interface/themes.h"
#include "../interface/viewport.h"
#include "../interface/widget.h"
#include "../interface/window.h"
#include "../platform/platform.h"
#include "../ride/track_data.h"
#include "../util/util.h"
#include "../world/footpath.h"
#include "../world/scenery.h"
#include "error.h"

enum {
	WIDX_PREVIOUS_IMAGE,		// 1
	WIDX_PREVIOUS_STEP_BUTTON,	// 2
	WIDX_NEXT_IMAGE,			// 4
	WIDX_NEXT_STEP_BUTTON,		// 8
};

// n3ds port: a strip across the bottom of the page (platform.h): the back button on the left
// half, the next button on the right half, the step's name in the half without a button (the
// designer has one button in each of its two steps). In the designer step the strip is shown
// only while no window that fills the page is open, as the HUD's buttons are: it would cover
// the bottom of the new ride and construction windows
static rct_widget window_editor_bottom_toolbar_widgets[] = {
	{ WWT_IMGBTN, 0, 0, N3DS_EDITOR_TOOLBAR_BUTTON_WIDTH - 1, 0, N3DS_EDITOR_TOOLBAR_HEIGHT - 1, 0xFFFFFFFF, 0xFFFF },	// 1		0x9A9958
	{ WWT_FLATBTN, 0, 2, N3DS_EDITOR_TOOLBAR_BUTTON_WIDTH - 3, 2, N3DS_EDITOR_TOOLBAR_HEIGHT - 3, 0xFFFFFFFF, 0xFFFF },	// 2		0x9A9968
	{ WWT_IMGBTN, 0, N3DS_BOTTOM_WIDTH - N3DS_EDITOR_TOOLBAR_BUTTON_WIDTH, N3DS_BOTTOM_WIDTH - 1, 0, N3DS_EDITOR_TOOLBAR_HEIGHT - 1, 0xFFFFFFFF, 0xFFFF },	// 4		0x9A9978
	{ WWT_FLATBTN, 0, N3DS_BOTTOM_WIDTH - N3DS_EDITOR_TOOLBAR_BUTTON_WIDTH + 2, N3DS_BOTTOM_WIDTH - 3, 2, N3DS_EDITOR_TOOLBAR_HEIGHT - 3, 0xFFFFFFFF, 0xFFFF },	// 8		0x9A9988
	{ WIDGETS_END },
};

static void window_editor_bottom_toolbar_mouseup(rct_window *w, int widgetIndex);
static void window_editor_bottom_toolbar_invalidate(rct_window *w);
static void window_editor_bottom_toolbar_paint(rct_window *w, rct_drawpixelinfo *dpi);

// n3ds port: the scenario editor's steps are not in this build. The designer has two: the object
// selection and the designer itself, so the buttons are "next" on the first and "back" on the second
static void window_editor_bottom_toolbar_jump_back_to_object_selection();
static void window_editor_bottom_toolbar_jump_forward_from_object_selection();

static rct_window_event_list window_editor_bottom_toolbar_events = {
	NULL,
	window_editor_bottom_toolbar_mouseup,	//0x0066f5ae,
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
	window_editor_bottom_toolbar_invalidate, //0x0066f1c9,
	window_editor_bottom_toolbar_paint, //0x0066f25c,
	NULL
};

static const rct_string_id EditorStepNames[] = {
	STR_EDITOR_STEP_OBJECT_SELECTION,
	STR_EDITOR_STEP_LANDSCAPE_EDITOR,
	STR_EDITOR_STEP_INVENTIONS_LIST_SET_UP,
	STR_EDITOR_STEP_OPTIONS_SELECTION,
	STR_EDITOR_STEP_OBJECTIVE_SELECTION,
	STR_EDITOR_STEP_SAVE_SCENARIO,
	STR_EDITOR_STEP_ROLLERCOASTER_DESIGNER,
	STR_EDITOR_STEP_TRACK_DESIGNS_MANAGER,
};

static void sub_6DFED0();

/**
* Creates the main editor top toolbar window.
* rct2: 0x0066F052 (part of 0x0066EF38)
*/
void window_editor_bottom_toolbar_open()
{
	rct_window* window;

	// n3ds port: the strip at the bottom of the page (see the widgets)
	window = window_create(0, N3DS_EDITOR_TOOLBAR_Y,
		N3DS_BOTTOM_WIDTH, N3DS_EDITOR_TOOLBAR_HEIGHT,
		&window_editor_bottom_toolbar_events,
		WC_BOTTOM_TOOLBAR, WF_STICK_TO_FRONT | WF_TRANSPARENT | WF_NO_BACKGROUND);
	window->widgets = window_editor_bottom_toolbar_widgets;

	window->enabled_widgets |=
		(1 << WIDX_PREVIOUS_STEP_BUTTON) |
		(1 << WIDX_NEXT_STEP_BUTTON) |
		(1 << WIDX_PREVIOUS_IMAGE) |
		(1 << WIDX_NEXT_IMAGE);

	window_init_scroll_widgets(window);
	sub_6DFED0();
}

/**
*
*  rct2: 0x0066F619
*/
void window_editor_bottom_toolbar_jump_back_to_object_selection() {
	window_close_all();
	gS6Info.editor_step = EDITOR_STEP_OBJECT_SELECTION;
	gfx_invalidate_screen();
}

/**
 *
 *  rct2: 0x006DFED0
 */
static void sub_6DFED0()
{
	for (int i = 0; i < 56; i++) {
		gResearchedSceneryItems[i] = 0xFFFFFFFF;
	}
}

/**
 *
 *  rct2: 0x006AB1CE
 */
static bool window_editor_bottom_toolbar_check_object_selection()
{
	int missingObjectType = editor_check_object_selection();
	if (missingObjectType < 0) {
		window_close_by_class(WC_EDITOR_OBJECT_SELECTION);
		return true;
	}

	// n3ds port: the original then clicked the tab of the missing object's type; the window
	// has the ride page alone now
	window_error_open(STR_INVALID_SELECTION_OF_OBJECTS, gGameCommandErrorText);
	return false;
}

/**
 *
 *  rct2: 0x0066F6E3
 */
static void sub_66F6E3()
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

	window_new_ride_open();
	gS6Info.editor_step = EDITOR_STEP_ROLLERCOASTER_DESIGNER;
	gfx_invalidate_screen();
}

/**
 *
 *  rct2: 0x0066F6B0
 */
void window_editor_bottom_toolbar_jump_forward_from_object_selection()
{
	if (!window_editor_bottom_toolbar_check_object_selection())
		return;

	sub_66F6E3();
}

/**
*
*  rct2: 0x0066F5AE
*/
static void window_editor_bottom_toolbar_mouseup(rct_window *w, int widgetIndex)
{
	if (widgetIndex == WIDX_PREVIOUS_STEP_BUTTON) {
		if (gS6Info.editor_step == EDITOR_STEP_ROLLERCOASTER_DESIGNER) {
			window_editor_bottom_toolbar_jump_back_to_object_selection();
		}
	} else if (widgetIndex == WIDX_NEXT_STEP_BUTTON) {
		if (gS6Info.editor_step == EDITOR_STEP_OBJECT_SELECTION) {
			window_editor_bottom_toolbar_jump_forward_from_object_selection();
		}
	}
}

static void hide_previous_step_button() {
	window_editor_bottom_toolbar_widgets[WIDX_PREVIOUS_STEP_BUTTON].type = WWT_EMPTY;
	window_editor_bottom_toolbar_widgets[WIDX_PREVIOUS_IMAGE].type = WWT_EMPTY;
}

static void hide_next_step_button() {
	window_editor_bottom_toolbar_widgets[WIDX_NEXT_STEP_BUTTON].type = WWT_EMPTY;
	window_editor_bottom_toolbar_widgets[WIDX_NEXT_IMAGE].type = WWT_EMPTY;
}

// n3ds port: whether a window that fills the page (window_n3ds_is_sheet) is open
static bool n3ds_sheet_window_open()
{
	for (rct_window *w = g_window_list; w < gWindowNextSlot; w++) {
		if (window_n3ds_is_sheet(w->classification))
			return true;
	}
	return false;
}

// n3ds port: a string in the small font, centred on x (the drawing functions that take a
// string id force the medium font)
static void n3ds_draw_small_string_centred(rct_drawpixelinfo *dpi, rct_string_id stringId, int x, int y, int colour)
{
	char *buffer = gCommonStringFormatBuffer;
	format_string(buffer, 256, stringId, NULL);
	gCurrentFontSpriteBase = FONT_SPRITE_BASE_SMALL;
	int width = gfx_get_string_width(buffer);
	gfx_draw_string(dpi, buffer, colour, x - width / 2, y);
}

/**
*
*  rct2: 0x0066F1C9
*/
void window_editor_bottom_toolbar_invalidate(rct_window *w)
{
	colour_scheme_update_by_class(w, WC_EDITOR_TRACK_BOTTOM_TOOLBAR);

	window_editor_bottom_toolbar_widgets[WIDX_PREVIOUS_STEP_BUTTON].type = WWT_FLATBTN;
	window_editor_bottom_toolbar_widgets[WIDX_NEXT_STEP_BUTTON].type = WWT_FLATBTN;
	window_editor_bottom_toolbar_widgets[WIDX_PREVIOUS_IMAGE].type = WWT_IMGBTN;
	window_editor_bottom_toolbar_widgets[WIDX_NEXT_IMAGE].type = WWT_IMGBTN;

	if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER) {
		hide_previous_step_button();
		hide_next_step_button();
	} else {
		if (gS6Info.editor_step == EDITOR_STEP_OBJECT_SELECTION) {
			hide_previous_step_button();
		} else if (gS6Info.editor_step == EDITOR_STEP_ROLLERCOASTER_DESIGNER) {
			hide_next_step_button();
			// n3ds port: not over a window that fills the page (see the widgets)
			if (n3ds_sheet_window_open())
				hide_previous_step_button();
		}
	}
}

/**
*
*  rct2: 0x0066F25C
*/
void window_editor_bottom_toolbar_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	// n3ds port: the invalidate event above decides which button there is (the manager has
	// none and draws nothing, as in the original)
	bool drawPreviousButton = window_editor_bottom_toolbar_widgets[WIDX_PREVIOUS_IMAGE].type != WWT_EMPTY;
	bool drawNextButton = window_editor_bottom_toolbar_widgets[WIDX_NEXT_IMAGE].type != WWT_EMPTY;
	if (!drawPreviousButton && !drawNextButton) {
		window_draw_widgets(w, dpi);
		return;
	}

	rct_widget *image = &window_editor_bottom_toolbar_widgets[drawPreviousButton ? WIDX_PREVIOUS_IMAGE : WIDX_NEXT_IMAGE];
	int buttonIndex = drawPreviousButton ? WIDX_PREVIOUS_STEP_BUTTON : WIDX_NEXT_STEP_BUTTON;

	gfx_filter_rect(dpi,
		image->left + w->x, image->top + w->y,
		image->right + w->x, image->bottom + w->y, PALETTE_51);

	window_draw_widgets(w, dpi);

	gfx_fill_rect_inset(dpi,
		image->left + 1 + w->x, image->top + 1 + w->y,
		image->right - 1 + w->x, image->bottom - 1 + w->y,
		w->colours[1], INSET_RECT_F_30);

	// The step's name, in the half without the button
	short stateX = w->x + (drawPreviousButton ?
		(N3DS_BOTTOM_WIDTH + N3DS_EDITOR_TOOLBAR_BUTTON_WIDTH) / 2 : N3DS_EDITOR_TOOLBAR_BUTTON_WIDTH / 2);
	short stateY = w->height - 0x0C + w->y;
	gfx_draw_string_centred(dpi, EditorStepNames[gS6Info.editor_step],
		stateX, stateY, NOT_TRANSLUCENT(w->colours[2]) | COLOUR_FLAG_OUTLINE, 0);

	int textColour = NOT_TRANSLUCENT(w->colours[1]);
	if (gHoverWidget.window_classification == WC_BOTTOM_TOOLBAR &&
		gHoverWidget.widget_index == buttonIndex
	) {
		textColour = COLOUR_WHITE;
	}
	short textY = image->top + 6 + w->y;

	// The button's two lines in the small font: "Forward to Next Step:" and "Roller Coaster
	// Designer" are wider than the 150-px button leaves beside the arrow in the medium font
	if (drawPreviousButton) {
		gfx_draw_sprite(dpi, SPR_PREVIOUS, image->left + 6 + w->x, image->top + 6 + w->y, 0);
		short textX = (image->left + 30 + image->right) / 2 + w->x;
		n3ds_draw_small_string_centred(dpi, STR_BACK_TO_PREVIOUS_STEP, textX, textY, textColour);
		n3ds_draw_small_string_centred(dpi, STR_EDITOR_STEP_OBJECT_SELECTION, textX, textY + 10, textColour);
	} else {
		gfx_draw_sprite(dpi, SPR_NEXT, image->right - 29 + w->x, image->top + 6 + w->y, 0);
		short textX = (image->left + image->right - 30) / 2 + w->x;
		n3ds_draw_small_string_centred(dpi, STR_FORWARD_TO_NEXT_STEP, textX, textY, textColour);
		n3ds_draw_small_string_centred(dpi, STR_EDITOR_STEP_ROLLERCOASTER_DESIGNER, textX, textY + 10, textColour);
	}
}
