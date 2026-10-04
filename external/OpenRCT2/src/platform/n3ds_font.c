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
 * n3ds port: the text of the languages that the game draws with a TrueType font (Korean),
 * without TrueType. The 3DS build has no SDL2_ttf and no FreeType; the game uses seven
 * functions of SDL_ttf (drawing/string.c: a line of text becomes an 8 bit image that is kept in
 * a cache and copied to the screen), and they are here, over bitmap fonts that are part of the
 * program: Galmuri, a pixel font after the fonts of the Nintendo DS, in three sizes with all
 * 11,172 Hangul syllables (SIL Open Font License, n3ds/galmuri-OFL.txt). So the game's own
 * text code runs as it does with a TrueType font on a PC, and no glyph is scaled or smoothed.
 *
 * n3ds/galmuri.n3kf is made by scripts/n3ds_korean_font.py of the port's repository, which
 * describes the format: fonts, each a few runs of consecutive codepoints, each run with one
 * record size, so that a glyph is found with one multiplication.
 */

#include "../common.h"

#ifdef __3DS__

#include <SDL.h>
#include <SDL_ttf.h>
#include "../localisation/localisation.h"

static const uint8 _fontFile[] = {
#embed "n3ds/galmuri.n3kf"
};

// The name the game's font descriptors give as the file of these fonts (interface/Fonts.cpp)
#define N3DS_FONT_NAME "galmuri"
#define N3DS_FONT_MAX_FONTS 4
#define N3DS_FONT_MAX_RUNS 12

typedef struct FontRun {
	uint32 first;			// codepoint of the first record
	uint32 count;
	int left, top;			// of the cell: columns from the pen, rows from the top of the line
	int width, height;		// of the cell
	int recordSize;
	const uint8 *records;	// each: the advance (0 = no such glyph), then the cell's bits
} FontRun;

struct _TTF_Font {
	int size;				// its pixel size, which TTF_OpenFont is asked for
	int height;				// of a line: the height of the image of a text
	int numRuns;
	FontRun runs[N3DS_FONT_MAX_RUNS];
};

static struct _TTF_Font _fonts[N3DS_FONT_MAX_FONTS];
static int _numFonts = -1;	// -1: the file is not read yet

static uint32 read_u16(const uint8 *p) { return p[0] | (p[1] << 8); }
static uint32 read_u32(const uint8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32)p[3] << 24); }

static void read_fonts()
{
	_numFonts = 0;
	if (sizeof(_fontFile) < 8 || memcmp(_fontFile, "N3KF", 4) != 0 || read_u16(_fontFile + 4) != 1)
		return;

	int count = read_u16(_fontFile + 6);
	for (int i = 0; i < count && i < N3DS_FONT_MAX_FONTS; i++) {
		const uint8 *entry = _fontFile + 8 + i * 8;
		struct _TTF_Font *font = &_fonts[i];
		font->size = read_u16(entry);
		font->height = entry[2];
		font->numRuns = min(entry[3], N3DS_FONT_MAX_RUNS);
		const uint8 *table = _fontFile + read_u32(entry + 4);
		for (int r = 0; r < font->numRuns; r++, table += 16) {
			FontRun *run = &font->runs[r];
			run->first = read_u32(table);
			run->records = _fontFile + read_u32(table + 4);
			run->count = read_u16(table + 8);
			run->left = (sint8)table[10];
			run->top = (sint8)table[11];
			run->width = table[12];
			run->height = table[13];
			run->recordSize = table[14];
		}
		_numFonts++;
	}
}

// The record of a codepoint and its run; NULL if the font has no such glyph
static const uint8 *find_glyph(const TTF_Font *font, uint32 codepoint, const FontRun **outRun)
{
	for (int i = 0; i < font->numRuns; i++) {
		const FontRun *run = &font->runs[i];
		if (codepoint - run->first < run->count) {
			const uint8 *record = run->records + (codepoint - run->first) * run->recordSize;
			if (record[0] == 0)
				return NULL;
			*outRun = run;
			return record;
		}
	}
	return NULL;
}

// What is drawn for a codepoint: its glyph, or the question mark for one the font does not have
static const uint8 *glyph_to_draw(const TTF_Font *font, uint32 codepoint, const FontRun **outRun)
{
	const uint8 *record = find_glyph(font, codepoint, outRun);
	if (record == NULL)
		record = find_glyph(font, '?', outRun);
	return record;
}

static int text_width(const TTF_Font *font, const char *text)
{
	int width = 0;
	const utf8 *ch = text;
	uint32 codepoint;
	while ((codepoint = utf8_get_next(ch, &ch)) != 0) {
		const FontRun *run;
		const uint8 *record = glyph_to_draw(font, codepoint, &run);
		if (record != NULL)
			width += record[0];
	}
	return width;
}

int TTF_Init(void)
{
	if (_numFonts < 0)
		read_fonts();
	if (_numFonts == 0) {
		SDL_SetError("The built-in fonts could not be read");
		return -1;
	}
	return 0;
}

void TTF_Quit(void)
{
}

// file: the name the game's font path gives (platform_get_font_path: the descriptor's file
// name as it is); ptsize: the pixel size of one of the built-in fonts. NULL for anything else:
// the font sets of the other TrueType languages (Arial, MS Gothic, ...) fail to open, and the
// game goes back to its sprite font for them.
TTF_Font *TTF_OpenFont(const char *file, int ptsize)
{
	if (strcmp(file, N3DS_FONT_NAME) == 0) {
		for (int i = 0; i < _numFonts; i++) {
			if (_fonts[i].size == ptsize)
				return &_fonts[i];
		}
	}
	SDL_SetError("No built-in font %s of size %d", file, ptsize);
	return NULL;
}

void TTF_CloseFont(TTF_Font *font)
{
	(void)font;
}

int TTF_GlyphIsProvided(const TTF_Font *font, Uint16 ch)
{
	const FontRun *run;
	return find_glyph(font, ch, &run) != NULL;
}

int TTF_SizeUTF8(TTF_Font *font, const char *text, int *w, int *h)
{
	if (w != NULL) *w = text_width(font, text);
	if (h != NULL) *h = font->height;
	return 0;
}

// The text as an 8 bit image: 0 where there is nothing, 1 where the text is (the game takes the
// colour from its own palette). As high as a line of the font, as wide as the text's advances.
SDL_Surface *TTF_RenderUTF8_Solid(TTF_Font *font, const char *text, SDL_Color fg)
{
	(void)fg;
	int width = text_width(font, text);
	if (width <= 0) {
		SDL_SetError("Text has zero width");
		return NULL;
	}
	SDL_Surface *surface = SDL_CreateRGBSurface(0, width, font->height, 8, 0, 0, 0, 0);
	if (surface == NULL)
		return NULL;

	// SDL has cleared the pixels
	uint8 *pixels = (uint8 *)surface->pixels;
	int pen = 0;
	const utf8 *ch = text;
	uint32 codepoint;
	while ((codepoint = utf8_get_next(ch, &ch)) != 0) {
		const FontRun *run;
		const uint8 *record = glyph_to_draw(font, codepoint, &run);
		if (record == NULL)
			continue;

		const uint8 *bits = record + 1;
		int bit = 0;
		for (int row = 0; row < run->height; row++) {
			uint8 *dst = pixels + (run->top + row) * surface->pitch;
			for (int column = 0; column < run->width; column++, bit++) {
				if (bits[bit >> 3] & (0x80 >> (bit & 7))) {
					// A glyph can reach a pixel left of the pen or beyond its advance
					int x = pen + run->left + column;
					if (x >= 0 && x < width)
						dst[x] = 1;
				}
			}
		}
		pen += record[0];
	}
	return surface;
}

#endif
