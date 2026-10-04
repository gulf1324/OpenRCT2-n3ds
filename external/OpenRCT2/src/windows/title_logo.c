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

static rct_widget window_title_logo_widgets[] = {
	{ WIDGETS_END },
};

static void window_title_logo_paint(rct_window *w, rct_drawpixelinfo *dpi);

static rct_window_event_list window_title_logo_events = {
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
	NULL,
	NULL,
	NULL,
	window_title_logo_paint,
	NULL
};

/**
 * Creates the window containing the logo and the expansion packs on the title screen.
 *  rct2: 0x0066B679 (part of 0x0066B3E8)
 */
void window_title_logo_open()
{
	rct_window *window = window_create(0, 0, 200, 106, &window_title_logo_events, WC_TITLE_LOGO, WF_STICK_TO_BACK | WF_TRANSPARENT);
	window->widgets = window_title_logo_widgets;
	window_init_scroll_widgets(window);
	window->colours[0] = TRANSLUCENT(COLOUR_GREY);
	window->colours[1] = TRANSLUCENT(COLOUR_GREY);
	window->colours[2] = TRANSLUCENT(COLOUR_GREY);
}

#if defined(__3DS__) && defined(N3DS_RCT2_TITLE_LOGO)
// n3ds port: the pixels drawn over the logo, see window_title_logo_paint
#include "n3ds_title_logo_pixels.h"
#endif

/**
*
*  rct2: 0x0066B872
*/
static void window_title_logo_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
#if defined(__3DS__) && defined(N3DS_RCT2_TITLE_LOGO)
	// n3ds port, only in a build configured with -DN3DS_RCT2_TITLE_LOGO=ON (cmake/n3ds.cmake):
	// the logo of RollerCoaster Tycoon 2 itself, from the game's own data, instead of OpenRCT2's
	// logo and title (user's request, for the user's own console: a build that is given to
	// others shows OpenRCT2's). It is the picture this window was made for in the original:
	// 196x100, drawn from (2, 3) in the 200x106 window.
	//
	// The picture is cut off at its right edge: the arm of the last 'r' of RollerCoaster ends
	// in its yellow filling (the last column with pixels) with no outline after it, which
	// looked clipped (user's report). The user finished the letter by hand (the intro's large
	// logo is whole, but reduced to this size it did not suit the title screen): the picture
	// with the changes is edited as an image, and scripts/n3ds_title_logo.py of the port's
	// repository turns the edited image into n3ds_title_logo_pixels.h: pixels drawn over the
	// picture, as columns of one colour, and pixels of the picture that are taken out.
	// A pixel can only be taken out before the picture is on the screen, so the logo is put
	// together in the scratch image and copied from there (index 0 is transparent).
	rct_drawpixelinfo scratch = n3ds_scratch_begin_at(w->x, w->y, w->width, w->height);
	gfx_draw_sprite(&scratch, SPR_MENU_LOGO, w->x, w->y, 0);
	for (int i = 0; i < N3DS_TITLE_LOGO_RUN_COUNT; i++) {
		const uint8 *run = N3dsTitleLogoRuns[i];
		gfx_fill_rect(&scratch, w->x + run[0], w->y + run[1], w->x + run[0], w->y + run[2], run[3]);
	}
	for (int i = 0; i < N3DS_TITLE_LOGO_ERASED_COUNT; i++) {
		const uint8 *pixel = N3dsTitleLogoErased[i];
		scratch.bits[pixel[1] * (scratch.width + scratch.pitch) + pixel[0]] = 0;
	}
	n3ds_scratch_copy_scaled(dpi, w->x, w->y, w->width, w->height, w->width, w->height);
#else
	int x = 2;
	int y = 2;
	gfx_draw_sprite(dpi, SPR_G2_LOGO, w->x + x, w->y + y, 0);
	gfx_draw_sprite(dpi, SPR_G2_TITLE, w->x + x + 104, w->y + y + 18, 0);
#endif
}
