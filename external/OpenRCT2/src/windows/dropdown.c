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

#include "../input.h"
#include "../interface/widget.h"
#include "../interface/window.h"
#include "../localisation/localisation.h"
#include "../rct2.h"
#include "../scenario/scenario.h"
#include "../sprites.h"
#include "dropdown.h"

int gAppropriateImageDropdownItemsPerRow[] = {
	1, 1, 1, 1, 2, 2, 3, 3, 4,
	3, 5, 4, 4, 5, 5, 5, 4, 5,
	6, 5, 5, 7, 4, 5, 6, 5, 6,
	6, 6, 6, 6, 8, 8, 0
};

enum {
	WIDX_BACKGROUND,
#ifdef __3DS__
	WIDX_N3DS_LIST,
#endif
};

static rct_widget window_dropdown_widgets[] = {
	{ WWT_IMGBTN, 0, 0, 0, 0, 0, SPR_NONE, STR_NONE },
#ifdef __3DS__
	// n3ds port: the list of a menu that is too long for the bottom screen (WWT_SCROLL then,
	// see window_dropdown_show_text_custom_width)
	{ WWT_EMPTY, 0, 0, 0, 0, 0, SCROLL_VERTICAL, STR_NONE },
#endif
	{ WIDGETS_END },
};

int _dropdown_num_columns;
int _dropdown_num_rows;
int _dropdown_item_width;
int _dropdown_item_height;
#ifdef __3DS__
// n3ds port: how far down in its row the text of an item is drawn (rows are taller, see
// window_dropdown_show_text_custom_width)
static int _n3dsTextOffsetY;
bool gDropdownN3dsTapTakesDefault;
// Small pictures (the 12x12 colours of the colour dropdown) are shown this many times their
// size, to be hit with a finger
static int _n3dsImageScale = 1;
#endif

int gDropdownNumItems;
rct_string_id gDropdownItemsFormat[64];
sint64 gDropdownItemsArgs[64];
uint64 gDropdownItemsChecked;
uint64 gDropdownItemsDisabled;
bool gDropdownIsColour;
int gDropdownLastColourHover;
int gDropdownHighlightedIndex;
int gDropdownDefaultIndex;

bool dropdown_is_checked(int index)
{
	return gDropdownItemsChecked & (1ULL << index);
}

bool dropdown_is_disabled(int index)
{
	return gDropdownItemsDisabled & (1ULL << index);
}

void dropdown_set_checked(int index, bool value)
{
	if (value) {
		gDropdownItemsChecked |= 1ULL << index;
	} else {
		gDropdownItemsChecked &= ~(1ULL << index);
	}
}

void dropdown_set_disabled(int index, bool value)
{
	if (value) {
		gDropdownItemsDisabled |= 1ULL << index;
	} else {
		gDropdownItemsDisabled &= ~(1ULL << index);
	}
}

static void window_dropdown_paint(rct_window *w, rct_drawpixelinfo *dpi);
#ifdef __3DS__
static void window_dropdown_n3ds_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height);
static void window_dropdown_n3ds_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex);
#endif

static rct_window_event_list window_dropdown_events = {
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
#ifdef __3DS__
	window_dropdown_n3ds_scrollgetsize,
#else
	NULL,
#endif
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
	window_dropdown_paint,
#ifdef __3DS__
	window_dropdown_n3ds_scrollpaint
#else
	NULL
#endif
};

/**
 * Shows a text dropdown menu.
 *  rct2: 0x006ECFB9
 *
 * @param x (cx)
 * @param y (dx)
 * @param extray (di)
 * @param flags (bh)
 * @param num_items (bx)
 * @param colour (al)
 */
void window_dropdown_show_text(int x, int y, int extray, uint8 colour, uint8 flags, int num_items)
{
	int i, string_width, max_string_width;
	char buffer[256];

	// Calculate the longest string width
	max_string_width = 0;
	for (i = 0; i < num_items; i++) {
		format_string(buffer, 256, gDropdownItemsFormat[i], (void*)(&gDropdownItemsArgs[i]));
		gCurrentFontSpriteBase = FONT_SPRITE_BASE_MEDIUM;
		string_width = gfx_get_string_width(buffer);
		max_string_width = max(string_width, max_string_width);
	}

	window_dropdown_show_text_custom_width(x, y, extray, colour, flags, num_items, max_string_width + 3);
}

/**
 * Shows a text dropdown menu.
 *  rct2: 0x006ECFB9, although 0x006ECE50 is real version
 *
 * @param x (cx)
 * @param y (dx)
 * @param extray (di)
 * @param flags (bh)
 * @param num_items (bx)
 * @param colour (al)
 */
void window_dropdown_show_text_custom_width(int x, int y, int extray, uint8 colour, uint8 flags, int num_items, int width)
{
	rct_window* w;

	gInputFlags &= ~(INPUT_FLAG_DROPDOWN_STAY_OPEN | INPUT_FLAG_DROPDOWN_MOUSE_UP);
	if (flags & DROPDOWN_FLAG_STAY_OPEN)
		gInputFlags |= INPUT_FLAG_DROPDOWN_STAY_OPEN;

	window_dropdown_close();
	_dropdown_num_columns = 1;
	_dropdown_item_width = width;
	_dropdown_item_height = 10;
	if (flags & 0x40)
		_dropdown_item_height = flags & 0x3F;
#ifdef __3DS__
	// n3ds port: rows of 10 pixels are too small to hit with a finger. Up to 18, as far as the
	// menu still fits the bottom screen, with the text in the middle of the row.
	// A menu too long for the bottom screen even with its original rows (the 29 music styles of
	// a ride) is a list that scrolls in a window of the screen's height (user's request), and
	// has the full 18.
	_n3dsTextOffsetY = 0;
	_n3dsImageScale = 1;
	bool n3dsScrolls = num_items * _dropdown_item_height > N3DS_BOTTOM_HEIGHT - 4;
	if (num_items > 0) {
		int rowHeight = n3dsScrolls ? 18 : min(18, (N3DS_BOTTOM_HEIGHT - 4) / num_items);
		if (rowHeight > _dropdown_item_height) {
			_n3dsTextOffsetY = (rowHeight - _dropdown_item_height) / 2;
			_dropdown_item_height = rowHeight;
		}
	}
	width += 8;
	// The list's border and scrollbar (13) are beside the items: no wider than the screen
	if (n3dsScrolls)
		width = min(width, N3DS_BOTTOM_WIDTH - 13);
	_dropdown_item_width = width;
#endif

	// Set the widgets
	gDropdownNumItems = num_items;
	_dropdown_num_rows = num_items;

	width = _dropdown_item_width * _dropdown_num_columns + 3;
	int height = _dropdown_item_height * _dropdown_num_rows + 3;
	if (x + width > gScreenWidth)
		x = max(0, gScreenWidth - width);
	if (y + height > gScreenHeight)
		y = max(0, gScreenHeight - height);

	window_dropdown_widgets[WIDX_BACKGROUND].bottom = _dropdown_item_height * num_items + 3;
	window_dropdown_widgets[WIDX_BACKGROUND].right = _dropdown_item_width + 3;
#ifdef __3DS__
	window_dropdown_widgets[WIDX_N3DS_LIST].type = WWT_EMPTY;
	if (n3dsScrolls) {
		// The list fills the window. What it shows (inside its border of 1, beside its
		// scrollbar of 11) is one item wide and as many whole rows high as fit the screen.
		rct_widget *list = &window_dropdown_widgets[WIDX_N3DS_LIST];
		list->type = WWT_SCROLL;
		list->right = _dropdown_item_width + 12;
		list->bottom = _dropdown_item_height * ((N3DS_BOTTOM_HEIGHT - 4) / _dropdown_item_height) + 1;
		window_dropdown_widgets[WIDX_BACKGROUND].right = list->right;
		window_dropdown_widgets[WIDX_BACKGROUND].bottom = list->bottom;
	}
#endif

	// Create the window
	w = window_create(
		x, y + extray,
		window_dropdown_widgets[WIDX_BACKGROUND].right + 1,
		window_dropdown_widgets[WIDX_BACKGROUND].bottom + 1,
		&window_dropdown_events,
		WC_DROPDOWN,
		WF_STICK_TO_FRONT
	);
	w->widgets = window_dropdown_widgets;
	if (colour & COLOUR_FLAG_TRANSLUCENT)
		w->flags |= WF_TRANSPARENT;
	w->colours[0] = colour;
#ifdef __3DS__
	window_init_scroll_widgets(w);
#endif

	// Input state
	gDropdownHighlightedIndex = -1;
	gDropdownItemsDisabled = 0;
	gDropdownItemsChecked = 0;
	gDropdownIsColour = false;
	gDropdownDefaultIndex = -1;
#ifdef __3DS__
	gDropdownN3dsTapTakesDefault = false;
#endif
	gInputState = INPUT_STATE_DROPDOWN_ACTIVE;
}

/**
 * Shows an image dropdown menu.
 *  rct2: 0x006ECFB9
 *
 * @param x (cx)
 * @param y (dx)
 * @param extray (di)
 * @param flags (bh)
 * @param numItems (bx)
 * @param colour (al)
 * @param itemWidth (bp)
 * @param itemHeight (ah)
 * @param numColumns (bl)
 */
void window_dropdown_show_image(int x, int y, int extray, uint8 colour, uint8 flags, int numItems, int itemWidth, int itemHeight, int numColumns)
{
	int width, height;
	rct_window* w;

	gInputFlags &= ~(INPUT_FLAG_DROPDOWN_STAY_OPEN | INPUT_FLAG_DROPDOWN_MOUSE_UP);
	if (flags & DROPDOWN_FLAG_STAY_OPEN)
		gInputFlags |= INPUT_FLAG_DROPDOWN_STAY_OPEN;

	// Close existing dropdown
	window_dropdown_close();

	// Set and calculate num items, rows and columns
	_dropdown_item_width = itemWidth;
	_dropdown_item_height = itemHeight;
#ifdef __3DS__
	_n3dsTextOffsetY = 0;
	window_dropdown_widgets[WIDX_N3DS_LIST].type = WWT_EMPTY;
	_n3dsImageScale = (itemWidth <= 16 && itemHeight <= 16) ? 2 : 1;
	_dropdown_item_width *= _n3dsImageScale;
	_dropdown_item_height *= _n3dsImageScale;
#endif
	gDropdownNumItems = numItems;
	_dropdown_num_columns = numColumns;
	_dropdown_num_rows = gDropdownNumItems / _dropdown_num_columns;
	if (gDropdownNumItems % _dropdown_num_columns != 0)
		_dropdown_num_rows++;

	// Calculate position and size
	width = _dropdown_item_width * _dropdown_num_columns + 3;
	height = _dropdown_item_height * _dropdown_num_rows + 3;
	if (x + width > gScreenWidth)
		x = max(0, gScreenWidth - width);
	if (y + height > gScreenHeight)
		y = max(0, gScreenHeight - height);
	window_dropdown_widgets[WIDX_BACKGROUND].right = width;
	window_dropdown_widgets[WIDX_BACKGROUND].bottom = height;

	// Create the window
	w = window_create(
		x, y + extray,
		window_dropdown_widgets[WIDX_BACKGROUND].right + 1,
		window_dropdown_widgets[WIDX_BACKGROUND].bottom + 1,
		&window_dropdown_events,
		WC_DROPDOWN,
		WF_STICK_TO_FRONT
	);
	w->widgets = window_dropdown_widgets;
	if (colour & COLOUR_FLAG_TRANSLUCENT)
		w->flags |= WF_TRANSPARENT;
	w->colours[0] = colour;

	// Input state
	gDropdownHighlightedIndex = -1;
	gDropdownItemsDisabled = 0;
	gDropdownItemsChecked = 0;
	gDropdownIsColour = false;
	gDropdownDefaultIndex = -1;
#ifdef __3DS__
	gDropdownN3dsTapTakesDefault = false;
#endif
	gInputState = INPUT_STATE_DROPDOWN_ACTIVE;
}

void window_dropdown_close()
{
	window_close_by_class(WC_DROPDOWN);
}

/**
 * Draws the items. x, y: where the window is in the dpi.
 * n3ds port: taken out of window_dropdown_paint, as the items of a list that scrolls are drawn
 * into the list's dpi; what was w->x and w->y in there is x and y here.
 */
static void window_dropdown_draw_items(rct_window *w, rct_drawpixelinfo *dpi, int x, int y)
{
	int cell_x, cell_y, l, t, r, b, item, image, colour;

	int highlightedIndex = gDropdownHighlightedIndex;
	for (int i = 0; i < gDropdownNumItems; i++) {
		cell_x = i % _dropdown_num_columns;
		cell_y = i / _dropdown_num_columns;

		if (gDropdownItemsFormat[i] == DROPDOWN_SEPARATOR) {
			l = x + 2 + (cell_x * _dropdown_item_width);
			t = y + 2 + (cell_y * _dropdown_item_height);
			r = l + _dropdown_item_width - 1;
			t += (_dropdown_item_height / 2);
			b = t;

			if (w->colours[0] & COLOUR_FLAG_TRANSLUCENT) {
				translucent_window_palette palette = TranslucentWindowPalettes[BASE_COLOUR(w->colours[0])];
				gfx_filter_rect(dpi, l, t, r, b, palette.highlight);
				gfx_filter_rect(dpi, l, t + 1, r, b + 1, palette.shadow);
			} else {
				gfx_fill_rect(dpi, l, t, r, b, ColourMapA[w->colours[0]].mid_dark);
				gfx_fill_rect(dpi, l, t + 1, r, b + 1, ColourMapA[w->colours[0]].lightest);
			}
		} else {
			//
			if (i == highlightedIndex) {
				l = x + 2 + (cell_x * _dropdown_item_width);
				t = y + 2 + (cell_y * _dropdown_item_height);
				r = l + _dropdown_item_width - 1;
				b = t + _dropdown_item_height - 1;
				gfx_filter_rect(dpi, l, t, r, b, PALETTE_DARKEN_3);
			}

			item = gDropdownItemsFormat[i];
			if (item == (uint16)-1 || item == (uint16)-2) {
				// Image item
				image = (uint32)gDropdownItemsArgs[i];
				if (item == (uint16)-2 && highlightedIndex == i)
					image++;

#ifdef __3DS__
				if (_n3dsImageScale > 1) {
					int imageWidth = _dropdown_item_width / _n3dsImageScale;
					int imageHeight = _dropdown_item_height / _n3dsImageScale;
					rct_drawpixelinfo scratch = n3ds_scratch_begin_at(0, 0, imageWidth, imageHeight);
					gfx_draw_sprite(&scratch, image, 0, 0, 0);
					n3ds_scratch_copy_scaled(
						dpi,
						x + 2 + (cell_x * _dropdown_item_width),
						y + 2 + (cell_y * _dropdown_item_height),
						imageWidth, imageHeight, _dropdown_item_width, _dropdown_item_height
					);
				} else
#endif
				gfx_draw_sprite(
					dpi,
					image,
					x + 2 + (cell_x * _dropdown_item_width),
					y + 2 + (cell_y * _dropdown_item_height), 0
				);
			} else {
				// Text item
				if (i < 64) {
					if (dropdown_is_checked(i)) {
						item++;
					}
				}

				// Calculate colour
				colour = NOT_TRANSLUCENT(w->colours[0]);
				if (i == highlightedIndex)
					colour = COLOUR_WHITE;
				if (dropdown_is_disabled(i))
					if (i < 64)
						colour = NOT_TRANSLUCENT(w->colours[0]) | COLOUR_FLAG_INSET;

				// Draw item string
				gfx_draw_string_left_clipped(
					dpi,
					item,
					(void*)(&gDropdownItemsArgs[i]), colour,
					x + 2 + (cell_x * _dropdown_item_width),
#ifdef __3DS__
					y + 1 + (cell_y * _dropdown_item_height) + _n3dsTextOffsetY,
#else
					y + 1 + (cell_y * _dropdown_item_height),
#endif
					w->width - 5
				);
			}
		}
	}
}

static void window_dropdown_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	window_draw_widgets(w, dpi);
#ifdef __3DS__
	// n3ds port: a list that scrolls has drawn them (window_dropdown_n3ds_scrollpaint)
	if (window_dropdown_widgets[WIDX_N3DS_LIST].type == WWT_SCROLL)
		return;
#endif
	window_dropdown_draw_items(w, dpi, w->x, w->y);
}

#ifdef __3DS__
// n3ds port: the dropdown as a list that scrolls. Its content is the items, the first at (0, 0).
static void window_dropdown_n3ds_scrollgetsize(rct_window *w, int scrollIndex, int *width, int *height)
{
	*height = _dropdown_num_rows * _dropdown_item_height;
}

static void window_dropdown_n3ds_scrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int scrollIndex)
{
	// The items are drawn 2 from the window's corner
	window_dropdown_draw_items(w, dpi, -2, -2);
}
#endif

/**
 * New function based on 6e914e
 * returns -1 if index is invalid
 */
int dropdown_index_from_point(int x, int y, rct_window *w)
{
#ifdef __3DS__
	// n3ds port: in a list that scrolls. Its view is inside the border; beside it, the scrollbar.
	if (window_dropdown_widgets[WIDX_N3DS_LIST].type == WWT_SCROLL) {
		int viewX = x - w->x - 1;
		int viewY = y - w->y - 1;
		if (viewX < 0 || viewX >= _dropdown_item_width || viewY < 0 || viewY >= w->height - 2)
			return -1;
		int index = (viewY + w->scrolls[0].v_top) / _dropdown_item_height;
		return index < gDropdownNumItems ? index : -1;
	}
#endif
	int top = y - w->y - 2;
	if (top < 0) return -1;

	int left = x - w->x;
	if (left >= w->width) return -1;
	left -= 2;
	if (left < 0) return -1;

	int column_no = left / _dropdown_item_width;
	if (column_no >= _dropdown_num_columns) return -1;

	int row_no = top / _dropdown_item_height;
	if (row_no >= _dropdown_num_rows) return -1;

	int dropdown_index = row_no * _dropdown_num_columns + column_no;
	if (dropdown_index >= gDropdownNumItems) return -1;

	return dropdown_index;
}

#ifdef __3DS__
// n3ds port: the widget of the open dropdown that is its list, if it is one that scrolls (its
// items are then the items of that list, n3ds_input.cpp). -1 if not.
int window_dropdown_n3ds_list_widget()
{
	return window_dropdown_widgets[WIDX_N3DS_LIST].type == WWT_SCROLL ? WIDX_N3DS_LIST : -1;
}

// n3ds port: where the items of the open dropdown are (relative to the window) and which can be
// chosen, for the D-pad focus (n3ds_input.cpp). False if there is no item with this index.
// In a list that scrolls: where they are when it is at its top.
bool window_dropdown_n3ds_get_item(int index, int *x, int *y, int *width, int *height, bool *selectable)
{
	if (index < 0 || index >= gDropdownNumItems)
		return false;
	int origin = window_dropdown_n3ds_list_widget() != -1 ? 1 : 2;
	*x = origin + (index % _dropdown_num_columns) * _dropdown_item_width;
	*y = origin + (index / _dropdown_num_columns) * _dropdown_item_height;
	*width = _dropdown_item_width;
	*height = _dropdown_item_height;
	// What a click can choose (input_state_widget_pressed)
	*selectable = gDropdownItemsFormat[index] != DROPDOWN_SEPARATOR && !(index < 64 && dropdown_is_disabled(index));
	return true;
}
#endif

void window_dropdown_show_colour(rct_window *w, rct_widget *widget, uint8 dropdownColour, uint8 selectedColour)
{
	window_dropdown_show_colour_available(w, widget, dropdownColour, selectedColour, 0xFFFFFFFF);
}

/**
 *
 *  rct2: 0x006ED43D
 * al: dropdown colour
 * ah: selected colour
 * esi: window
 * edi: widget
 * ebp: unknown
 */
void window_dropdown_show_colour_available(rct_window *w, rct_widget *widget, uint8 dropdownColour, uint8 selectedColour,
	uint32 availableColours)
{
	int i, numItems;

	// Count number of available colours
	numItems = 0;
	for (i = 0; i < 32; i++)
		if (availableColours & (1 << i))
			numItems++;

	int defaultIndex = -1;
	// Set items
	for (i = 0; i < 32; i++) {
		if (availableColours & (1 << i)) {
			if (selectedColour == i)
				defaultIndex = i;

			gDropdownItemsFormat[i] = 0xFFFE;
			gDropdownItemsArgs[i] = ((uint64)i << 32) | (0x20000000 | (i << 19) | SPR_PALETTE_BTN);
		}
	}

	// Show dropdown
	window_dropdown_show_image(
		w->x + widget->left,
		w->y + widget->top,
		widget->bottom - widget->top + 1,
		dropdownColour,
		DROPDOWN_FLAG_STAY_OPEN,
		numItems,
		12,
		12,
		gAppropriateImageDropdownItemsPerRow[numItems]
	);

	gDropdownIsColour = true;
	gDropdownLastColourHover = -1;
	gDropdownDefaultIndex = defaultIndex;
}
