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

#include "../localisation/localisation.h"
#include "../sprites.h"
#include "../interface/themes.h"
#include "../interface/widget.h"
#include "../interface/window.h"

enum WINDOW_ABOUT_WIDGET_IDX {
	WIDX_BACKGROUND,
	WIDX_TITLE,
	WIDX_CLOSE,
	WIDX_SCROLL,
	WIDX_MUSIC_CREDITS,
	WIDX_PUBLISHER_CREDITS
};

// n3ds port: one page of the bottom screen (320x240). The credits (330 px tall on PC) scroll
// above the two buttons, which are at the 3DS control height, one under the other
#define N3DS_ABOUT_CONTENT_WIDTH 296

rct_widget window_about_widgets[] = {
	{ WWT_FRAME,			0,	0,			319,	0,		239,	0xFFFFFFFF,								STR_NONE },				// panel / background
	{ WWT_CAPTION,			0,	1,			318,	1,		14,		STR_ROLLERCOASTER_TYCOON_2,				STR_WINDOW_TITLE_TIP },	// title bar
	{ WWT_CLOSEBOX,			0,	307,		317,	2,		13,		STR_CLOSE_X,							STR_CLOSE_WINDOW_TIP },	// close x button
	{ WWT_SCROLL,			1,	4,			315,	18,		190,	SCROLL_VERTICAL,						STR_NONE },				// credits
	{ WWT_DROPDOWN_BUTTON,	1,	10,			309,	196,	213,	STR_MUSIC_ACKNOWLEDGEMENTS_ELLIPSIS,	STR_NONE },				// music credits button
	{ WWT_DROPDOWN_BUTTON,	1,	10,			309,	217,	234,	STR_INFOGRAMES_INTERACTIVE_CREDITS,		STR_NONE },				// infogrames credits button
	{ WIDGETS_END },
};

static void window_about_mouseup(rct_window *w, int widgetIndex);
static void window_about_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height);
static void window_about_paint(rct_window *w, rct_drawpixelinfo *dpi);
static void window_about_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex);

static rct_window_event_list window_about_events = {
	NULL,
	window_about_mouseup,
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
	window_about_scrollgetsize,
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
	window_about_paint,
	window_about_scrollpaint
};

/**
 *
 *  rct2: 0x0066D2AC
 */
void window_about_open()
{
	rct_window* window;

	// Check if window is already open
	window = window_bring_to_front_by_class(WC_ABOUT);
	if (window != NULL)
		return;

	window = window_create_centred(
		320,
		240,
		&window_about_events,
		WC_ABOUT,
		0
	);
	window->widgets = window_about_widgets;
	window->enabled_widgets = (1 << WIDX_CLOSE) | (1 << WIDX_MUSIC_CREDITS) | (1 << WIDX_PUBLISHER_CREDITS);

	window_init_scroll_widgets(window);
	window->colours[0] = COLOUR_LIGHT_BLUE;
	window->colours[1] = COLOUR_LIGHT_BLUE;
	window->colours[2] = COLOUR_LIGHT_BLUE;
}

/**
 *
 *  rct2: 0x0066D4D5
 */
static void window_about_mouseup(rct_window *w, int widgetIndex)
{
	switch (widgetIndex) {
	case WIDX_CLOSE:
		window_close(w);
		break;
	case WIDX_MUSIC_CREDITS:
		window_music_credits_open();
		break;
	case WIDX_PUBLISHER_CREDITS:
		window_publisher_credits_open();
		break;
	}
}

// n3ds port: the credits laid out once per language (drawing.h n3ds_text_block: drawing wrapped
// strings every repaint made scrolling drop to a few frames a second through the Korean font),
// one under the other, centred, wrapped to the content's width. The two pictures' rows are
// kept beside the lines
static n3ds_text_block _credits = { -1 };
static int _chrisSawyerY;
static int _infogramesY;

static void window_about_lay_out_credits()
{
	if (_credits.language == gCurrentLanguage)
		return;
	n3ds_text_block_reset(&_credits);
	int x = N3DS_ABOUT_CONTENT_WIDTH / 2;
	int y = 2;

	// Version
	y += n3ds_text_block_add(&_credits, STR_VERSION_X, x, y, N3DS_ABOUT_CONTENT_WIDTH);

	// Credits
	y += n3ds_text_block_add(&_credits, STR_COPYRIGHT_CS, x, y, N3DS_ABOUT_CONTENT_WIDTH);
	y += 3;
	_chrisSawyerY = y;
	y += 53 + 6;
	y += n3ds_text_block_add(&_credits, STR_DESIGNED_AND_PROGRAMMED_BY_CS, x, y, N3DS_ABOUT_CONTENT_WIDTH);
	y += n3ds_text_block_add(&_credits, STR_GRAPHICS_BY_SF, x, y, N3DS_ABOUT_CONTENT_WIDTH);
	y += n3ds_text_block_add(&_credits, STR_SOUND_AND_MUSIC_BY_AB, x, y, N3DS_ABOUT_CONTENT_WIDTH);
	y += n3ds_text_block_add(&_credits, STR_ADDITIONAL_SOUNDS_RECORDED_BY_DE, x, y, N3DS_ABOUT_CONTENT_WIDTH);
	y += 3;
	y += n3ds_text_block_add(&_credits, STR_REPRESENTATION_BY_JL, x, y, N3DS_ABOUT_CONTENT_WIDTH);
	y += 15;
	y += n3ds_text_block_add(&_credits, STR_THANKS_TO, x, y, N3DS_ABOUT_CONTENT_WIDTH);
	y += n3ds_text_block_add(&_credits, STR_THANKS_TO_PEOPLE, x, y, N3DS_ABOUT_CONTENT_WIDTH);
	// (the original's seven spare credit lines, STR_CREDIT_SPARE_1..7, are empty: not drawn)

	// Licence
	y += 12;
	_infogramesY = y;
	y += 78 + 3;
	y += n3ds_text_block_add(&_credits, STR_LICENSED_TO_INFOGRAMES_INTERACTIVE_INC, x, y, N3DS_ABOUT_CONTENT_WIDTH);
	_credits.height = y + 2;
	_credits.language = gCurrentLanguage;
}

static void window_about_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height)
{
	window_about_lay_out_credits();
	*height = _credits.height;
}

/**
 *
 *  rct2: 0x0066D321
 */
static void window_about_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	window_draw_widgets(w, dpi);
}

static void window_about_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex)
{
	window_about_lay_out_credits();
	int x = N3DS_ABOUT_CONTENT_WIDTH / 2;
	gfx_draw_sprite(dpi, SPR_CREDITS_CHRIS_SAWYER_SMALL, x - 108, _chrisSawyerY, 0);
	gfx_draw_sprite(dpi, SPR_CREDITS_INFOGRAMES, x - 49, _infogramesY, 0);
	n3ds_text_block_draw(&_credits, dpi, COLOUR_BLACK);
}
