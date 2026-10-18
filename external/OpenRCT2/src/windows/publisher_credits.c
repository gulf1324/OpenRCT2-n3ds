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
#include "../interface/widget.h"
#include "../interface/window.h"

enum WINDOW_PUBLISHER_CREDITS_WIDGET_IDX {
	WIDX_BACKGROUND,
	WIDX_TITLE,
	WIDX_CLOSE
};

// n3ds port: one page of the bottom screen (320x240); the lines are centred in the 296-px content
// and wrapped where wider
#define N3DS_CREDITS_CONTENT_WIDTH 296

rct_widget window_publisher_credits_widgets[] = {
	{WWT_FRAME,    0, 0,   319, 0,  239, 0xFFFFFFFF, STR_NONE},                // panel / background
	{WWT_CAPTION,  0, 1,   318, 1,  14,  STR_ROLLERCOASTER_TYCOON_2,	STR_WINDOW_TITLE_TIP },	// title bar
	{WWT_CLOSEBOX, 0, 307, 317, 2,  13,  STR_CLOSE_X,				STR_CLOSE_WINDOW_TIP },	// close x button
	{WWT_SCROLL,   0, 4,   315, 18, 235, SCROLL_VERTICAL,         STR_NONE },				// scroll
	{ WIDGETS_END },
};

static void window_publisher_credits_mouseup(rct_window *w, int widgetIndex);
static void window_publisher_credits_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height);
static void window_publisher_credits_paint(rct_window *w, rct_drawpixelinfo *dpi);
static void window_publisher_credits_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex);
static void window_publisher_credits_lay_out();
static n3ds_text_block _lines;

static rct_window_event_list window_publisher_credits_events = {
	NULL,
	window_publisher_credits_mouseup,
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
	window_publisher_credits_scrollgetsize,
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
	window_publisher_credits_paint,
	window_publisher_credits_scrollpaint
};

/**
*
*  rct2: 0x0066D4EC
*/
void window_publisher_credits_open()
{
	rct_window* window;

	// Check if window is already open
	window = window_bring_to_front_by_class(WC_PUBLISHER_CREDITS);
	if (window != NULL)
		return;

	window = window_create_centred(
		320,
		240,
		&window_publisher_credits_events,
		WC_PUBLISHER_CREDITS,
		0
	);

	window->widgets = window_publisher_credits_widgets;
	window->enabled_widgets = 1 << WIDX_CLOSE;

	window_init_scroll_widgets(window);
	window->colours[0] = COLOUR_LIGHT_BLUE;
	window->colours[1] = COLOUR_LIGHT_BLUE;
	window->colours[2] = COLOUR_LIGHT_BLUE;

}

/**
*
*  rct2: 0x0066D7A8
*/
static void window_publisher_credits_mouseup(rct_window *w, int widgetIndex)
{
	switch (widgetIndex) {
	case WIDX_CLOSE:
		window_close(w);
		break;
	}
}

/**
*
*  rct2: 0x0066D7B3
*/
static void window_publisher_credits_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height)
{
	window_publisher_credits_lay_out();
	*height = _lines.height;
}

/**
*
*  rct2: 0x0066D5CB
*/
static void window_publisher_credits_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	window_draw_widgets(w, dpi);
}

static const rct_string_id credits_order[] = {
	STR_CREDITS_PUBLISHER_LINE_0,
	STR_CREDITS_PUBLISHER_LINE_1,
	STR_CREDITS_PUBLISHER_LINE_2,
	STR_CREDITS_PUBLISHER_LINE_3,
	STR_CREDITS_PUBLISHER_LINE_4,
	STR_CREDITS_PUBLISHER_LINE_5,
	STR_CREDITS_PUBLISHER_LINE_6,
	STR_CREDITS_PUBLISHER_LINE_7,
	STR_CREDITS_PUBLISHER_LINE_8,
	STR_CREDITS_PUBLISHER_LINE_9,
	STR_CREDITS_PUBLISHER_LINE_10,
	STR_CREDITS_PUBLISHER_LINE_11,
	STR_CREDITS_PUBLISHER_LINE_12,
	STR_CREDITS_PUBLISHER_LINE_13,
	STR_CREDITS_PUBLISHER_LINE_14,
	STR_CREDITS_PUBLISHER_LINE_15,
	STR_CREDITS_PUBLISHER_LINE_16,
	STR_CREDITS_PUBLISHER_LINE_17,
	STR_CREDITS_PUBLISHER_LINE_18,
	STR_CREDITS_PUBLISHER_LINE_19,
	STR_CREDITS_PUBLISHER_LINE_20,
	STR_CREDITS_PUBLISHER_LINE_21,
};

// n3ds port: the lines laid out once per language (drawing.h n3ds_text_block: drawing wrapped
// strings every repaint made scrolling slow through the Korean font), one under the other,
// centred, wrapped to the content's width
static n3ds_text_block _lines = { -1 };   // declared above
#define N3DS_PUBLISHER_TITLE_Y 88

static void window_publisher_credits_lay_out()
{
	if (_lines.language == gCurrentLanguage)
		return;
	n3ds_text_block_reset(&_lines);
	int x = N3DS_CREDITS_CONTENT_WIDTH / 2;
	int y = N3DS_PUBLISHER_TITLE_Y + 14;

	for (int i = 0; i < countof(credits_order); i++) {
		y += n3ds_text_block_add(&_lines, credits_order[i], x, y, N3DS_CREDITS_CONTENT_WIDTH) + 1;
	}
	_lines.height = y + 2;
	_lines.language = gCurrentLanguage;
}

/**
*
*  rct2: 0x0066D5D1
*/
static void window_publisher_credits_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex)
{
	window_publisher_credits_lay_out();
	int x = N3DS_CREDITS_CONTENT_WIDTH / 2;
	gfx_draw_sprite(dpi, SPR_CREDITS_INFOGRAMES, x - 49, 2, 0);
	if (dpi->y < N3DS_PUBLISHER_TITLE_Y + 13 && dpi->y + dpi->height > N3DS_PUBLISHER_TITLE_Y)
		draw_string_centred_underline(dpi, STR_CREDITS_PUBLISHER_TILE, NULL, COLOUR_BLACK, x, N3DS_PUBLISHER_TITLE_Y);
	n3ds_text_block_draw(&_lines, dpi, COLOUR_BLACK);
}
