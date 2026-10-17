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
#include "../drawing/drawing.h"
#include "../game.h"
#include "../image_io.h"
#include "../intro.h"
#include "../localisation/localisation.h"
#include "../OpenRCT2.h"
#include "../platform/platform.h"
#include "../rct2.h"
#include "../util/util.h"
#include "../windows/error.h"
#include "screenshot.h"
#include "viewport.h"

uint8 gScreenshotCountdown = 0;

/**
 *
 *  rct2: 0x006E3AEC
 */
void screenshot_check()
{
	int screenshotIndex;

	if (gScreenshotCountdown != 0) {
		gScreenshotCountdown--;
		if (gScreenshotCountdown == 0) {
			// update_rain_animation();
			screenshotIndex = screenshot_dump();

			if (screenshotIndex != -1) {
				audio_play_sound(SOUND_WINDOW_OPEN, 100, gScreenWidth / 2);
			} else {
				window_error_open(STR_SCREENSHOT_FAILED, STR_NONE);
			}

			// redraw_rain();
		}
	}
}

static void screenshot_get_rendered_palette(rct_palette* palette) {
	for (int i = 0; i < 256; i++) {
		const SDL_Color *renderedEntry = &gPalette[i];
		rct_palette_entry *entry = &palette->entries[i];

		entry->red = renderedEntry->r;
		entry->green = renderedEntry->g;
		entry->blue = renderedEntry->b;
		entry->alpha = renderedEntry->a;
	}
}

static int screenshot_get_next_path(char *path, size_t size)
{
	char screenshotPath[MAX_PATH];

	platform_get_user_directory(screenshotPath, "screenshot", sizeof(screenshotPath));
	if (!platform_ensure_directory_exists(screenshotPath)) {
		log_error("Unable to save screenshots in OpenRCT2 screenshot directory.\n");
		return -1;
	}

	char park_name[128];
	format_string(park_name, 128, gParkName, &gParkNameArgs);

	// retrieve current time
	rct2_date currentDate;
	platform_get_date_local(&currentDate);
	rct2_time currentTime;
	platform_get_time_local(&currentTime);

	// Glue together path and filename
	snprintf(path, size, "%s%s %d-%02d-%02d %02d-%02d-%02d.png", screenshotPath, park_name, currentDate.year, currentDate.month, currentDate.day, currentTime.hour, currentTime.minute, currentTime.second);

	if (!platform_file_exists(path)) {
		return 0; // path ok
	}

	// multiple screenshots with same timestamp
	// might be possible when switching timezones
	// in the unlikely case that this does happen,
	// append (%d) to the filename and increment
	// this int until it doesn't overwrite any
	// other file in the directory.
	int i;
	for (i = 1; i < 1000; i++) {
		// Glue together path and filename
		snprintf(path, size, "%s%s %d-%02d-%02d %02d-%02d-%02d (%d).png", screenshotPath, park_name, currentDate.year, currentDate.month, currentDate.day, currentTime.hour, currentTime.minute, currentTime.second, i);

		if (!platform_file_exists(path)) {
			return i;
		}
	}

	log_error("You have too many saved screenshots saved at exactly the same date and time.\n");
	return -1;
}

int screenshot_dump_png(rct_drawpixelinfo *dpi)
{
	// Get a free screenshot path
	int index;
	char path[MAX_PATH] = "";
	if ((index = screenshot_get_next_path(path, MAX_PATH)) == -1) {
		return -1;
	}

	rct_palette renderedPalette;
	screenshot_get_rendered_palette(&renderedPalette);

	if (image_io_png_write(dpi, &renderedPalette, path)) {
		return index;
	} else {
		return -1;
	}
}
