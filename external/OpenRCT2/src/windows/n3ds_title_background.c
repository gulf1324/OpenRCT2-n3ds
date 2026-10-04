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

/*
 * n3ds port: the park behind the title windows on the bottom screen. On PC the logo and the menu
 * float over the main view, which fills the screen. On the 3DS the main view is on the top screen
 * only, so this window shows the park on the bottom screen: the part right below the top screen,
 * following the main view (title sequence cuts, zoom). Opened with the title windows
 * (title_create_windows); like them it goes when a game starts (viewport_init_all).
 */

#include "../common.h"

#ifdef __3DS__

#include "../interface/viewport.h"
#include "../interface/widget.h"
#include "../interface/window.h"
#include "../platform/platform.h"

// No widgets: taps on the background do nothing
static rct_widget window_n3ds_title_background_widgets[] = {
	{ WIDGETS_END },
};

// Rotation the view was last drawn with: rotating redraws only the main view (window_rotate_camera)
static uint8 _lastRotation;

static void window_n3ds_title_background_update(rct_window *w);
static void window_n3ds_title_background_paint(rct_window *w, rct_drawpixelinfo *dpi);

// The resize event is the one a window gets just before its view is moved to its saved position
// for the frame (viewport_update_position), after the main window's: following the main view
// there, this view is never a frame behind it. Following only in the update event, it was: the
// frame that ends the intro is drawn without an update, and showed this view where it was
// before the title's park was loaded, empty (user's report: the bottom screen blank for a moment).
static rct_window_event_list window_n3ds_title_background_events = {
	NULL,
	NULL,
	window_n3ds_title_background_update,
	NULL,
	NULL,
	NULL,
	window_n3ds_title_background_update,
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
	window_n3ds_title_background_paint,
	NULL
};

void window_n3ds_title_background_open()
{
	if (window_find_by_class(WC_N3DS_TITLE_BACKGROUND) != NULL)
		return;
	rct_window *mainWindow = window_get_main();
	if (mainWindow == NULL || mainWindow->viewport == NULL)
		return;

	// Fills the bottom screen page, behind the title windows (both stick to the back; this one is
	// opened first). Opaque, so the translucent title menu shows the park through it as on PC.
	rct_window *w = window_create(0, 0, N3DS_BOTTOM_WIDTH, N3DS_BOTTOM_HEIGHT,
		&window_n3ds_title_background_events, WC_N3DS_TITLE_BACKGROUND, WF_STICK_TO_BACK);
	w->widgets = window_n3ds_title_background_widgets;
	// No VIEWPORT_FLAG_SOUND_ON: the main view plays the park sounds
	viewport_create(w, w->x, w->y, w->width, w->height, mainWindow->viewport->zoom, 0, 0, 0, VIEWPORT_FOCUS_TYPE_COORDINATE, -1);
	_lastRotation = get_current_rotation();
	window_n3ds_title_background_update(w);
}

/**
 * Follows the main view. The views are moved to their saved positions when drawing
 * (window_update_all_viewports), the main view first: see the events above.
 */
static void window_n3ds_title_background_update(rct_window *w)
{
	rct_window *mainWindow = window_get_main();
	if (mainWindow == NULL || mainWindow->viewport == NULL || w->viewport == NULL)
		return;
	rct_viewport *mainViewport = mainWindow->viewport;
	rct_viewport *viewport = w->viewport;

	if (viewport->zoom != mainViewport->zoom) {
		viewport->zoom = mainViewport->zoom;
		viewport->view_width = viewport->width << viewport->zoom;
		viewport->view_height = viewport->height << viewport->zoom;
		window_invalidate(w);
	}
	if (_lastRotation != get_current_rotation()) {
		_lastRotation = get_current_rotation();
		window_invalidate(w);
	}

	// Right below the top screen, centred under it
	w->saved_view_x = mainViewport->view_x + (mainViewport->view_width - viewport->view_width) / 2;
	w->saved_view_y = mainViewport->view_y + mainViewport->view_height;
}

static void window_n3ds_title_background_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	window_draw_viewport(dpi, w);
}

#endif
