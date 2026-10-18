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

enum WINDOW_MUSIC_CREDITS_WIDGET_IDX {
	WIDX_BACKGROUND,
	WIDX_TITLE,
	WIDX_CLOSE
};

// n3ds port: one page of the bottom screen (320x240); the lines (up to 480 px wide on PC) are
// centred in the 296-px content and wrapped
#define N3DS_CREDITS_CONTENT_WIDTH 296

rct_widget window_music_credits_widgets[] = {
	{ WWT_FRAME,	0,	0,		319,	0,	239,	0xFFFFFFFF,					STR_NONE },				// panel / background
	{ WWT_CAPTION,	0,	1,		318,	1,	14,		STR_MUSIC_ACKNOWLEDGEMENTS,	STR_WINDOW_TITLE_TIP },	// title bar
	{ WWT_CLOSEBOX,	0,	307,	317,	2,	13,		STR_CLOSE_X,				STR_CLOSE_WINDOW_TIP },	// close x button
	{ WWT_SCROLL,	0,	4,		315,	18,	235,	SCROLL_VERTICAL,							STR_NONE },				// scroll
	{ WIDGETS_END },
};

static const rct_string_id music_credits[] = {
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_MARCH_CHILDREN_OF_THE_REGIMENT,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_HEYKENS_SERENADE,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_IN_CONTINENTAL_MOOD,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_WEDDING_JOURNEY,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_TALES_FROM_THE_VIENNA_WOODS,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_SLAVONIC_DANCE,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_DAS_ALPENHORN,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_THE_BLOND_SAILOR,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_OVERTURE_POET_AND_PEASANT,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_WALTZ_MEDLEY,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_BELLA_BELLA_BIMBA,
};

static const rct_string_id music_credits_rct2[] = {
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_RCT2_TITLE_MUSIC,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_DODGEMS_BEAT,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_MIS_SUMMERS_HEAT,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_PHARAOS_TOMB,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_CAESARS_MARCH,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_DRIFTING_TO_HEAVEN,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_INVADERS,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_ETERNAL_TOYBOX,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_JUNGLE_JUICE,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_NINJAS_NOODLES,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_VOYAGE_TO_ANDROMEDA,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_BRIMBLES_BEAT,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_ATLANTIS,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_WILD_WEST_KID,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_VAMPIRES_LAIR,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_BLOCKUBSTER,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_AIRTIME_ROCK,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_SEARCHLIGHT_RAG,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_FLIGHT_OF_FANTASY,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_BIG_ROCK,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_HYPOTHERMIA,
	STR_MUSIC_ACKNOWLEDGEMENTS_SAMPLES_COURTESY,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_LAST_SLEIGH_RIDE,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_PIPES_OF_GLENCAIRN,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_TRAFFIC_JAM,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_TOCCATA,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_SPACE_ROCK,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_MANIC_MECHANIC,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_TECHNO_TORTURE,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_SWEET_DREAMS,
	STR_MUSIC_ACKNOWLEDGEMENTS_TRACK_WHAT_SHALL_WE_DO_WITH_THE_DRUNKEN_SAILOR,
};

static void window_music_credits_mouseup(rct_window *w, int widgetIndex);
static void window_music_credits_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height);
static void window_music_credits_paint(rct_window *w, rct_drawpixelinfo *dpi);
static void window_music_credits_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex);
static void window_music_credits_lay_out();
static n3ds_text_block _lines;
static int _separatorY;

static rct_window_event_list window_music_credits_events = {
	NULL,
	window_music_credits_mouseup,
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
	window_music_credits_scrollgetsize,
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
	window_music_credits_paint,
	window_music_credits_scrollpaint
};

/**
*
*  rct2: 0x0066D55B
*/
void window_music_credits_open()
{
	rct_window* window;

	// Check if window is already open
	window = window_bring_to_front_by_class(WC_MUSIC_CREDITS);
	if (window != NULL)
		return;

	window = window_create_centred(
		320,
		240,
		&window_music_credits_events,
		WC_MUSIC_CREDITS,
		0
	);

	window->widgets = window_music_credits_widgets;
	window->enabled_widgets = 1 << WIDX_CLOSE;

	window_init_scroll_widgets(window);
	window->colours[0] = COLOUR_LIGHT_BLUE;
	window->colours[1] = COLOUR_LIGHT_BLUE;
	window->colours[2] = COLOUR_LIGHT_BLUE;

}

/**
*
*  rct2: 0x0066DB2C
*/
static void window_music_credits_mouseup(rct_window *w, int widgetIndex)
{
	switch (widgetIndex) {
	case WIDX_CLOSE:
		window_close(w);
		break;
	}
}

/**
 *
 *  rct2: 0x0066DB37
 */
static void window_music_credits_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height)
{
	window_music_credits_lay_out();
	*height = _lines.height;
}

/**
*
*  rct2: 0x0066D7B9
*/
static void window_music_credits_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	window_draw_widgets(w, dpi);
}

/**
 *
 *  rct2: 0x0066D7BF
 */
static void window_music_credits_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex)
{
	window_music_credits_lay_out();
	if (dpi->y < _separatorY + 2 && dpi->y + dpi->height > _separatorY)
		gfx_fill_rect_inset(dpi, 4, _separatorY, N3DS_CREDITS_CONTENT_WIDTH - 4, _separatorY + 1, w->colours[1], INSET_RECT_FLAG_BORDER_INSET);
	n3ds_text_block_draw(&_lines, dpi, COLOUR_BLACK);
}

// n3ds port: the lines laid out once per language (drawing.h n3ds_text_block: drawing wrapped
// strings every repaint made scrolling drop to a few frames a second through the Korean font),
// one under the other, centred, wrapped to the content's width. The separator's row is kept
static n3ds_text_block _lines = { -1 };   // declared above

static void window_music_credits_lay_out()
{
	if (_lines.language == gCurrentLanguage)
		return;
	n3ds_text_block_reset(&_lines);
	int x = N3DS_CREDITS_CONTENT_WIDTH / 2;
	int y = 2;

	for (int i = 0; i < countof(music_credits); i++) {
		y += n3ds_text_block_add(&_lines, music_credits[i], x, y, N3DS_CREDITS_CONTENT_WIDTH);
	}

	// Add 4 more space before "Original recordings ...".
	y += 4;
	y += n3ds_text_block_add(&_lines, STR_MUSIC_ACKNOWLEDGEMENTS_ORIGINAL_RECORDINGS, x, y, N3DS_CREDITS_CONTENT_WIDTH);

	// The separator
	y += 5;
	_separatorY = y;
	y += 11;

	for (int i = 0; i < countof(music_credits_rct2); i++) {
		y += n3ds_text_block_add(&_lines, music_credits_rct2[i], x, y, N3DS_CREDITS_CONTENT_WIDTH);
	}
	_lines.height = y + 2;
	_lines.language = gCurrentLanguage;
}
