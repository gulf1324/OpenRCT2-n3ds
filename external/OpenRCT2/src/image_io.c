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

#include <png.h>

#include "image_io.h"

static void my_png_write_data(png_structp png_ptr, png_bytep data, png_size_t length);
static void my_png_flush(png_structp png_ptr);


bool image_io_png_write(const rct_drawpixelinfo *dpi, const rct_palette *palette, const utf8 *path)
{
	// Get image size
	int stride = dpi->width + dpi->pitch;

	// Setup PNG
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		return false;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		return false;
	}

	png_colorp png_palette = (png_colorp)png_malloc(png_ptr, PNG_MAX_PALETTE_LENGTH * sizeof(png_color));
	for (int i = 0; i < 256; i++) {
		const rct_palette_entry *entry = &palette->entries[i];
		png_palette[i].blue		= entry->blue;
		png_palette[i].green	= entry->green;
		png_palette[i].red		= entry->red;
	}

	png_set_PLTE(png_ptr, info_ptr, png_palette, PNG_MAX_PALETTE_LENGTH);

	// Open file for writing
	SDL_RWops *file = SDL_RWFromFile(path, "wb");
	if (file == NULL) {
		png_free(png_ptr, png_palette);
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		return false;
	}
	png_set_write_fn(png_ptr, file, my_png_write_data, my_png_flush);

	// Set error handler
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_free(png_ptr, png_palette);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		SDL_RWclose(file);
		return false;
	}

	// Write header
	png_set_IHDR(
		png_ptr, info_ptr, dpi->width, dpi->height, 8,
		PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
	);
	png_byte transparentIndex = 0;
	png_set_tRNS(png_ptr, info_ptr, &transparentIndex, 1, NULL);
	png_write_info(png_ptr, info_ptr);

	// Write pixels
	uint8 *bits = dpi->bits;
	for (int y = 0; y < dpi->height; y++) {
		png_write_row(png_ptr, (png_byte *)bits);
		bits += stride;
	}

	// Finish
	png_write_end(png_ptr, NULL);
	SDL_RWclose(file);

	png_free(png_ptr, png_palette);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	return true;
}

static void image_io_png_warning(png_structp png_ptr, const char *b)
{
    log_warning(b);
}

static void image_io_png_error(png_structp png_ptr, const char *b)
{
    log_error(b);
}



static void my_png_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	SDL_RWops *file = (SDL_RWops*)png_get_io_ptr(png_ptr);
	SDL_RWwrite(file, data, length, 1);
}

static void my_png_flush(png_structp png_ptr)
{

}
