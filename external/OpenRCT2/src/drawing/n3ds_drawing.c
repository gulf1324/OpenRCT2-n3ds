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
 * n3ds port: drawing at another size for windows that are fitted to the 320x240 bottom screen.
 * The image is drawn at full size into a scratch buffer with the normal drawing functions, then
 * copied at the wanted size (palette index 0 is transparent): at half size, nearest pixel, for
 * the large images of lists and previews; enlarged up to 3x with smoothed outlines for the
 * picture buttons of the windows (1.5x) and of the HUD (35x33 from 30x28).
 */

#include "../common.h"

#include "../platform/platform.h"
#include "../sprites.h"
#include "drawing.h"
#include "n3ds_construction_pictures.h"
#include "n3ds_simulate_picture.h"

// Big enough for the track design preview (370x217)
#define N3DS_SCRATCH_WIDTH  384
#define N3DS_SCRATCH_HEIGHT 224

static uint8 _scratchBits[N3DS_SCRATCH_WIDTH * N3DS_SCRATCH_HEIGHT];

// The smoothed copy handles images up to this size (a toolbar button is 30x28, the largest
// picture button of a window laid out for the bottom screen 46x36)
#define N3DS_SMOOTH_MAX_WIDTH  48
#define N3DS_SMOOTH_MAX_HEIGHT 40

static uint8 _scale3xBits[(N3DS_SMOOTH_MAX_WIDTH * 3) * (N3DS_SMOOTH_MAX_HEIGHT * 3)];

// A mix of colours becomes the closest of the palette colours 10..229. The others are left
// out: 0..9 and 246..255 are outside the game palette (10..245), 230..245 are rewritten every
// frame (update_palette_effects: water, chain lift), so a mix landing on one would flicker.
// For the same reason a pixel with an animated colour among its samples is not mixed.
#define N3DS_MIX_FIRST      10
#define N3DS_MIX_COUNT      220
#define N3DS_ANIMATED_FIRST 230
#define N3DS_ANIMATED_COUNT 16
// Largest error allowed (n3ds_mix_lookup). Beyond it the palette has nothing like the mix and
// the closest colour would be of another hue (dark grey and blue gave purple), so the pixel is
// not mixed.
#define N3DS_MIX_MAX_ERROR  96000
#define N3DS_MIX_NONE       1

// Palette index for a mixed colour, by its RGB at 5 bits each. 0 = not worked out yet.
// Valid for the palette colours in _mixPalette.
static uint8 _mixIndex[32 * 32 * 32];
static SDL_Color _mixPalette[N3DS_MIX_COUNT];

// Starts an empty scratch image. What is drawn at the screen position (x, y) lands in its
// top-left corner, so existing paint code can draw into it unchanged.
rct_drawpixelinfo n3ds_scratch_begin_at(int x, int y, int width, int height)
{
	if (width > N3DS_SCRATCH_WIDTH) width = N3DS_SCRATCH_WIDTH;
	if (height > N3DS_SCRATCH_HEIGHT) height = N3DS_SCRATCH_HEIGHT;

	rct_drawpixelinfo scratch;
	scratch.bits = _scratchBits;
	scratch.x = x;
	scratch.y = y;
	scratch.width = width;
	scratch.height = height;
	scratch.pitch = 0;
	scratch.zoom_level = 0;
	memset(_scratchBits, 0, (size_t)(width * height));
	return scratch;
}

static rct_drawpixelinfo n3ds_scratch_begin(int fullWidth, int fullHeight)
{
	return n3ds_scratch_begin_at(0, 0, fullWidth, fullHeight);
}

// Copies the scratch image (width x height) to (x, y), stretched to scaledWidth x scaledHeight.
// At its own size it is copied without the divisions (function calls on the ARM11): the title
// logo is copied that way every frame (title_logo.c).
void n3ds_scratch_copy_scaled(rct_drawpixelinfo *dpi, int x, int y, int width, int height, int scaledWidth, int scaledHeight)
{
	int stride = dpi->width + dpi->pitch;
	bool sameSize = (scaledWidth == width && scaledHeight == height);
	for (int j = 0; j < scaledHeight; j++) {
		int py = y + j - dpi->y;
		if (py < 0 || py >= dpi->height)
			continue;
		const uint8 *src = _scratchBits + (sameSize ? j : j * height / scaledHeight) * width;
		uint8 *dst = dpi->bits + py * stride;
		for (int i = 0; i < scaledWidth; i++) {
			int px = x + i - dpi->x;
			if (px < 0 || px >= dpi->width)
				continue;
			uint8 colour = src[sameSize ? i : i * width / scaledWidth];
			if (colour != 0)
				dst[px] = colour;
		}
	}
}

// Scale3x (AdvMAME3x): every pixel becomes 3x3 pixels. Where two neighbouring sides of a pixel
// have the same colour, that colour is carried into the corner between them, which turns the
// steps of diagonal and curved outlines into slopes. Compares palette indices only. Pixels
// outside the image count as a copy of the edge pixel.
static void n3ds_scale3x(const uint8 *src, int width, int height, uint8 *dst)
{
	int dstStride = width * 3;
	for (int y = 0; y < height; y++) {
		const uint8 *row = src + y * width;
		const uint8 *rowUp = (y > 0) ? row - width : row;
		const uint8 *rowDown = (y < height - 1) ? row + width : row;
		uint8 *out = dst + (y * 3) * dstStride;
		for (int x = 0; x < width; x++, out += 3) {
			int xl = (x > 0) ? x - 1 : x;
			int xr = (x < width - 1) ? x + 1 : x;
			uint8 A = rowUp[xl], B = rowUp[x], C = rowUp[xr];
			uint8 D = row[xl], E = row[x], F = row[xr];
			uint8 G = rowDown[xl], H = rowDown[x], I = rowDown[xr];

			uint8 *out1 = out + dstStride;
			uint8 *out2 = out1 + dstStride;
			out[0] = out[1] = out[2] = E;
			out1[0] = out1[1] = out1[2] = E;
			out2[0] = out2[1] = out2[2] = E;
			if (B != H && D != F) {
				if (D == B) out[0] = D;
				if ((D == B && E != C) || (B == F && E != A)) out[1] = B;
				if (B == F) out[2] = F;
				if ((D == B && E != G) || (D == H && E != A)) out1[0] = D;
				if ((B == F && E != I) || (H == F && E != C)) out1[2] = F;
				if (D == H) out2[0] = D;
				if ((D == H && E != I) || (H == F && E != G)) out2[1] = H;
				if (H == F) out2[2] = F;
			}
		}
	}
}

// Returns the palette index closest to a colour (key: 5 bits each of red, green, blue), or
// N3DS_MIX_NONE if the palette has nothing close. Each colour is searched for once and kept.
static uint8 n3ds_mix_lookup(unsigned int key)
{
	uint8 mixed = _mixIndex[key];
	if (mixed != 0)
		return mixed;

	// The middle of the 8x8x8 cell of colours that share the key
	int r = ((key >> 10) & 31) * 8 + 4;
	int g = ((key >> 5) & 31) * 8 + 4;
	int b = (key & 31) * 8 + 4;
	int bestError = N3DS_MIX_MAX_ERROR + 1;
	mixed = N3DS_MIX_NONE;
	for (int i = N3DS_MIX_FIRST; i < N3DS_MIX_FIRST + N3DS_MIX_COUNT; i++) {
		int dr = gPalette[i].r - r;
		int dg = gPalette[i].g - g;
		int db = gPalette[i].b - b;
		// Difference in brightness (8x) and in the two colour axes R-Y and B-Y. The colour
		// axes weigh more: a mix slightly too dark or light is not noticed, one of the
		// wrong hue is.
		int luma = 2 * dr + 5 * dg + db;
		int cr = 8 * dr - luma;
		int cb = 8 * db - luma;
		int error = 2 * luma * luma + 3 * (cr * cr + cb * cb);
		if (error < bestError) {
			bestError = error;
			mixed = (uint8)i;
		}
	}
	_mixIndex[key] = mixed;
	return mixed;
}

// Copies the scratch image (width x height) to (x, y) at scaledWidth x scaledHeight, anything
// from its own size to three times it, with smoothed outlines. Plain enlarging repeats some of
// the rows and columns, so outlines come out jagged and lines of the same width come out 1 or 2
// pixels wide. Here the image is enlarged 3x with Scale3x and each output pixel is the average
// of the box of those it covers (2x2 at 1.5x; 2 or 3 a side at the 35x33 of the HUD's buttons):
// every line gets the same width (one full pixel and one half-tone) and slopes are antialiased.
// An average is a colour that has to be found in the palette (n3ds_mix_lookup); pixels whose
// samples are all equal, most of the image, are copied as they are. Transparent samples take
// the colour already at the destination, so the edge of the image blends into what is behind
// it; where nothing has been drawn yet (palette index 0: the HUD over the park, n3ds_input.cpp)
// they are left out of the average, so that a pixel is either drawn or left alone.
void n3ds_scratch_copy_smooth_sized(rct_drawpixelinfo *dpi, int x, int y, int width, int height, int scaledWidth, int scaledHeight)
{
	if (width > N3DS_SMOOTH_MAX_WIDTH || height > N3DS_SMOOTH_MAX_HEIGHT ||
		scaledWidth < width || scaledWidth > width * 3 || scaledHeight < height || scaledHeight > height * 3
	) {
		n3ds_scratch_copy_scaled(dpi, x, y, width, height, scaledWidth, scaledHeight);
		return;
	}

	// The game changes the palette (loading a park, lightning, day and night): the colours
	// found for the old one no longer hold. The animated colours are not among those compared.
	if (memcmp(_mixPalette, &gPalette[N3DS_MIX_FIRST], sizeof(_mixPalette)) != 0) {
		memcpy(_mixPalette, &gPalette[N3DS_MIX_FIRST], sizeof(_mixPalette));
		memset(_mixIndex, 0, sizeof(_mixIndex));
	}

	n3ds_scale3x(_scratchBits, width, height, _scale3xBits);

	// Output pixel i covers the Scale3x columns boxLeft[i] to boxLeft[i + 1] - 1, and likewise
	// the rows: at most 3 of each, as the image is not made smaller
	uint8 boxLeft[N3DS_SMOOTH_MAX_WIDTH * 3 + 1], boxTop[N3DS_SMOOTH_MAX_HEIGHT * 3 + 1];
	for (int i = 0; i <= scaledWidth; i++)
		boxLeft[i] = i * (width * 3) / scaledWidth;
	for (int j = 0; j <= scaledHeight; j++)
		boxTop[j] = j * (height * 3) / scaledHeight;

	int srcStride = width * 3;
	int stride = dpi->width + dpi->pitch;
	for (int j = 0; j < scaledHeight; j++) {
		int py = y + j - dpi->y;
		if (py < 0 || py >= dpi->height)
			continue;
		uint8 *dst = dpi->bits + py * stride;
		for (int i = 0; i < scaledWidth; i++) {
			int px = x + i - dpi->x;
			if (px < 0 || px >= dpi->width)
				continue;
			// The samples of this pixel: how many, their sum, whether they differ
			uint8 behind = dst[px];
			uint8 first = 0;
			bool mixed = false, animated = false;
			unsigned int r = 0, g = 0, b = 0, count = 0;
			for (int row = boxTop[j]; row < boxTop[j + 1]; row++) {
				const uint8 *src = _scale3xBits + row * srcStride;
				for (int col = boxLeft[i]; col < boxLeft[i + 1]; col++) {
					uint8 s = src[col];
					if (s == 0) {
						s = behind;
						if (s == 0)
							continue;
					}
					if ((uint8)(s - N3DS_ANIMATED_FIRST) < N3DS_ANIMATED_COUNT)
						animated = true;
					if (count == 0)
						first = s;
					else if (s != first)
						mixed = true;
					r += gPalette[s].r;
					g += gPalette[s].g;
					b += gPalette[s].b;
					count++;
				}
			}
			if (count == 0)
				continue;
			// Not mixed: the first sample, as a nearest pixel copy would give
			uint8 colour = first;
			if (mixed && !animated) {
				// The 5 bits of each average
				uint8 found = n3ds_mix_lookup((((r / count) >> 3) << 10) | (((g / count) >> 3) << 5) | ((b / count) >> 3));
				if (found != N3DS_MIX_NONE)
					colour = found;
			}
			dst[px] = colour;
		}
	}
}

// At 1.5 times the size: the picture buttons of the windows laid out for the bottom screen
// (window_n3ds_place_picture)
void n3ds_scratch_copy_smooth(rct_drawpixelinfo *dpi, int x, int y, int width, int height)
{
	n3ds_scratch_copy_smooth_sized(dpi, x, y, width, height, width * 3 / 2, height * 3 / 2);
}

// Copies the scratch image (width x height) to (x, y) at scaledWidth x scaledHeight with its
// frame kept crisp: the outer `left`, `top`, `right` and `bottom` columns and rows of pixels are
// copied one for one (stretched along their length, nearest pixel), and only the picture inside
// them is enlarged smoothly (n3ds_scratch_copy_smooth_sized). For the HUD's toolbar buttons
// (top_toolbar.c): their one-pixel edges, enlarged 1.17x with the picture, came out a pixel and
// a half wide and uneven (user's report).
void n3ds_scratch_copy_framed(rct_drawpixelinfo *dpi, int x, int y, int width, int height, int scaledWidth, int scaledHeight, int left, int top, int right, int bottom)
{
	int innerWidth = width - left - right, innerHeight = height - top - bottom;
	int scaledInnerWidth = scaledWidth - left - right, scaledInnerHeight = scaledHeight - top - bottom;
	if (innerWidth <= 0 || innerHeight <= 0 || scaledInnerWidth <= 0 || scaledInnerHeight <= 0 ||
		width > N3DS_SMOOTH_MAX_WIDTH || height > N3DS_SMOOTH_MAX_HEIGHT
	) {
		n3ds_scratch_copy_scaled(dpi, x, y, width, height, scaledWidth, scaledHeight);
		return;
	}

	// The frame, from a copy of the image: the inside goes through the scratch next
	static uint8 image[N3DS_SMOOTH_MAX_WIDTH * N3DS_SMOOTH_MAX_HEIGHT];
	memcpy(image, _scratchBits, (size_t)(width * height));
	int stride = dpi->width + dpi->pitch;
	for (int j = 0; j < scaledHeight; j++) {
		int py = y + j - dpi->y;
		if (py < 0 || py >= dpi->height)
			continue;
		bool frameRow = j < top || j >= scaledHeight - bottom;
		int sj = j < top ? j : j >= scaledHeight - bottom ? height - (scaledHeight - j) : top + (j - top) * innerHeight / scaledInnerHeight;
		uint8 *dst = dpi->bits + py * stride;
		for (int i = 0; i < scaledWidth; i++) {
			bool frameColumn = i < left || i >= scaledWidth - right;
			if (!frameRow && !frameColumn)
				continue;
			int px = x + i - dpi->x;
			if (px < 0 || px >= dpi->width)
				continue;
			int si = i < left ? i : i >= scaledWidth - right ? width - (scaledWidth - i) : left + (i - left) * innerWidth / scaledInnerWidth;
			uint8 colour = image[sj * width + si];
			if (colour != 0)
				dst[px] = colour;
		}
	}

	// The inside, as an image of its own in the scratch
	for (int r = 0; r < innerHeight; r++)
		memcpy(_scratchBits + r * innerWidth, image + (top + r) * width + left, (size_t)innerWidth);
	n3ds_scratch_copy_smooth_sized(dpi, x + left, y + top, innerWidth, innerHeight, scaledInnerWidth, scaledInnerHeight);
}

// Pictures drawn for a larger size, used instead of a sprite enlarged where enlarging does not
// look right. Made by scripts, by the rules or in the style of the originals:
// - the picture buttons of the ride construction window at 1.25x, a size for which there is
//   no clean way to enlarge a sprite. scripts/n3ds_construction_pictures.py makes
//   n3ds_construction_pictures.h
// - its simulate button, OpenRCT2's icon of 2019 (not in this g2.dat) at 1.5x, as
//   SPR_N3DS_SIMULATE. scripts/n3ds_simulate_picture.py makes n3ds_simulate_picture.h
static const struct {
	uint16 sprite;
	uint8 width, height;
	const uint8 *pixels;	// palette indices, 0 = transparent; the whole widget
} _n3dsPictures[] = {
	N3DS_CONSTRUCTION_PICTURES
	// the simulate button of the ride construction window (scripts/n3ds_simulate_picture.py)
	{ SPR_N3DS_SIMULATE, 30, 30, N3dsSimulatePicture },
};

// Draws the picture that stands for a sprite shown at width x height, at (x, y), as
// gfx_draw_sprite draws the sprite: image with its colour bits (the buttons of a tool's size
// take the window's colour). With a solid colour (not -1) it is drawn as a shape of that colour
// only, as gfx_draw_sprite_solid does for a disabled button. False if there is no picture for
// this sprite at this size.
bool n3ds_draw_picture(rct_drawpixelinfo *dpi, int image, int x, int y, int width, int height, int solidColour)
{
	int sprite = image & 0x7FFFF;
	const uint8 *src = NULL;
	for (int i = 0; i < (int)countof(_n3dsPictures); i++) {
		if (_n3dsPictures[i].sprite == sprite && _n3dsPictures[i].width == width && _n3dsPictures[i].height == height) {
			src = _n3dsPictures[i].pixels;
			break;
		}
	}
	if (src == NULL)
		return false;

	const uint8 *palette = gfx_draw_sprite_get_palette(image, 0);
	int stride = dpi->width + dpi->pitch;
	for (int j = 0; j < height; j++, src += width) {
		int py = y + j - dpi->y;
		if (py < 0 || py >= dpi->height)
			continue;
		uint8 *dst = dpi->bits + py * stride;
		for (int i = 0; i < width; i++) {
			int px = x + i - dpi->x;
			if (px < 0 || px >= dpi->width)
				continue;
			uint8 colour = src[i];
			if (colour == 0)
				continue;
			if (solidColour != -1)
				colour = (uint8)solidColour;
			else if (palette != NULL)
				colour = palette[colour];
			dst[px] = colour;
		}
	}
	return true;
}

void n3ds_scratch_copy_half(rct_drawpixelinfo *dpi, int x, int y, int fullWidth, int fullHeight)
{
	int stride = dpi->width + dpi->pitch;
	for (int j = 0; j < fullHeight / 2; j++) {
		int py = y + j - dpi->y;
		if (py < 0 || py >= dpi->height)
			continue;
		const uint8 *src = _scratchBits + (2 * j) * fullWidth;
		uint8 *dst = dpi->bits + py * stride;
		for (int i = 0; i < fullWidth / 2; i++) {
			int px = x + i - dpi->x;
			if (px < 0 || px >= dpi->width)
				continue;
			uint8 colour = src[2 * i];
			if (colour != 0)
				dst[px] = colour;
		}
	}
}

void n3ds_draw_sprite_raw_masked_half(rct_drawpixelinfo *dpi, int x, int y, int maskImage, int colourImage, int fullWidth, int fullHeight)
{
	if (fullWidth > N3DS_SCRATCH_WIDTH) fullWidth = N3DS_SCRATCH_WIDTH;
	if (fullHeight > N3DS_SCRATCH_HEIGHT) fullHeight = N3DS_SCRATCH_HEIGHT;
	rct_drawpixelinfo scratch = n3ds_scratch_begin(fullWidth, fullHeight);
	gfx_draw_sprite_raw_masked(&scratch, 0, 0, maskImage, colourImage);
	n3ds_scratch_copy_half(dpi, x, y, fullWidth, fullHeight);
}

void n3ds_draw_sprite_half(rct_drawpixelinfo *dpi, int image, int x, int y, uint32 tertiaryColour, int fullWidth, int fullHeight, int originX, int originY)
{
	if (fullWidth > N3DS_SCRATCH_WIDTH) fullWidth = N3DS_SCRATCH_WIDTH;
	if (fullHeight > N3DS_SCRATCH_HEIGHT) fullHeight = N3DS_SCRATCH_HEIGHT;
	rct_drawpixelinfo scratch = n3ds_scratch_begin(fullWidth, fullHeight);
	gfx_draw_sprite(&scratch, image, originX, originY, tertiaryColour);
	n3ds_scratch_copy_half(dpi, x, y, fullWidth, fullHeight);
}

