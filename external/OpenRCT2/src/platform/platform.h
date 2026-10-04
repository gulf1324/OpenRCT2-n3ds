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

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "../common.h"

#ifdef __WINDOWS__
#include "../rct2.h"
	#ifndef HAVE_MATH_H
		#define HAVE_MATH_H
	#endif
#endif // __WINDOWS__

#include <SDL.h>

#include "../core/textinputbuffer.h"
#include "../drawing/font.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifdef __MACOSX__
#define KEYBOARD_PRIMARY_MODIFIER KMOD_GUI
#else
#define KEYBOARD_PRIMARY_MODIFIER KMOD_CTRL
#endif

#define INVALID_HANDLE -1

#define TOUCH_DOUBLE_TIMEOUT 300

#ifdef __WINDOWS__
#define PATH_SEPARATOR "\\"
#define PLATFORM_NEWLINE "\r\n"
#else
#define PATH_SEPARATOR "/"
#define PLATFORM_NEWLINE "\n"
#endif

#define SHIFT 0x100
#define CTRL 0x200
#define ALT 0x400
#define CMD 0x800
#ifdef __MACOSX__
	#define PLATFORM_MODIFIER CMD
#else
	#define PLATFORM_MODIFIER CTRL
#endif

typedef struct resolution {
	int width, height;
} resolution;

typedef struct file_info {
	const char *path;
	uint64 size;
	uint64 last_modified;
} file_info;

typedef struct rct2_date {
	sint16 day;
	sint16 month;
	sint16 year;
	sint16 day_of_week;
} rct2_date;

typedef struct rct2_time {
	sint16 hour;
	sint16 minute;
	sint16 second;
} rct2_time;

typedef struct openrct2_cursor {
	int x, y;
	unsigned char left, middle, right, any;
	int wheel;
	int old;
	bool touch, touchIsDouble;
	unsigned int touchDownTimestamp;
} openrct2_cursor;

enum {
	CURSOR_UP = 0,
	CURSOR_DOWN = 1,
	CURSOR_CHANGED = 2,
	CURSOR_RELEASED = CURSOR_UP | CURSOR_CHANGED,
	CURSOR_PRESSED = CURSOR_DOWN | CURSOR_CHANGED,
};

typedef enum {FD_OPEN, FD_SAVE} filedialog_type;

typedef struct file_dialog_desc {
	uint8 type;
	const utf8 *title;
	const utf8 *initial_directory;
	const utf8 *default_filename;
	struct {
		const utf8 *name;			// E.g. "Image Files"
		const utf8 *pattern;		// E.g. "*.png;*.jpg;*.gif"
	} filters[8];
} file_dialog_desc;

extern openrct2_cursor gCursorState;
extern const unsigned char *gKeysState;
extern unsigned char *gKeysPressed;
extern unsigned int gLastKeyPressed;

extern textinputbuffer gTextInput;
extern bool gTextInputCompositionActive;
extern utf8 gTextInputComposition[32];
extern int gTextInputCompositionStart;
extern int gTextInputCompositionLength;

extern int gResolutionsAllowAnyAspectRatio;
extern int gNumResolutions;
extern resolution *gResolutions;
extern SDL_Window *gWindow;

extern SDL_Color gPalette[256];

extern bool gSteamOverlayActive;

// Platform shared definitions
void platform_update_fullscreen_resolutions();
void platform_get_closest_resolution(int inWidth, int inHeight, int *outWidth, int *outHeight);
void platform_init();
void platform_draw();
void platform_draw_require_end();
void platform_free();
void platform_trigger_resize();
void platform_update_palette(const uint8 *colours, int start_index, int num_colours);
void platform_set_fullscreen_mode(int mode);
void platform_toggle_windowed_mode();
void platform_set_cursor(uint8 cursor);
void platform_refresh_video();
void platform_process_messages();
int platform_scancode_to_rct_keycode(int sdl_key);
void platform_start_text_input(utf8 *buffer, int max_length);
void platform_stop_text_input();
bool platform_is_input_active();
void platform_get_date_utc(rct2_date *out_date);
void platform_get_time_utc(rct2_time *out_time);
void platform_get_date_local(rct2_date *out_date);
void platform_get_time_local(rct2_time *out_time);

// Platform specific definitions
void platform_get_exe_path(utf8 *outPath, size_t outSize);
bool platform_file_exists(const utf8 *path);
bool platform_directory_exists(const utf8 *path);
bool platform_original_game_data_exists(const utf8 *path);
time_t platform_file_get_modified_time(const utf8* path);
bool platform_ensure_directory_exists(const utf8 *path);
bool platform_directory_delete(const utf8 *path);
bool platform_lock_single_instance();
int platform_enumerate_files_begin(const utf8 *pattern);
bool platform_enumerate_files_next(int handle, file_info *outFileInfo);
void platform_enumerate_files_end(int handle);
int platform_enumerate_directories_begin(const utf8 *directory);
bool platform_enumerate_directories_next(int handle, utf8 *path);
void platform_enumerate_directories_end(int handle);
void platform_init_window_icon();

// Returns the bitmask of the GetLogicalDrives function for windows, 0 for other systems
int platform_get_drives();

bool platform_file_copy(const utf8 *srcPath, const utf8 *dstPath, bool overwrite);
bool platform_file_move(const utf8 *srcPath, const utf8 *dstPath);
bool platform_file_delete(const utf8 *path);
void platform_hide_cursor();
void platform_show_cursor();
void platform_get_cursor_position(int *x, int *y);
void platform_get_cursor_position_scaled(int *x, int *y);
void platform_set_cursor_position(int x, int y);
unsigned int platform_get_ticks();
void platform_resolve_user_data_path();
void platform_resolve_openrct_data_path();
void platform_get_openrct_data_path(utf8 *outPath, size_t outSize);
void platform_get_user_directory(utf8 *outPath, const utf8 *subDirectory, size_t outSize);
utf8* platform_get_username();
void platform_show_messagebox(utf8 *message);
bool platform_open_common_file_dialog(utf8 *outFilename, file_dialog_desc *desc, size_t outSize);
utf8 *platform_open_directory_browser(utf8 *title);
uint8 platform_get_locale_currency();
uint8 platform_get_currency_value(const char *currencyCode);
uint16 platform_get_locale_language();
uint8 platform_get_locale_measurement_format();
uint8 platform_get_locale_temperature_format();
bool platform_get_font_path(TTFFontDescriptor *font, utf8 *buffer, size_t size);

bool platform_check_steam_overlay_attached();

datetime64 platform_get_datetime_now_utc();

// Called very early in the program before parsing commandline arguments.
void core_init();

// Windows specific definitions
#ifdef __WINDOWS__
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	#undef GetMessage

	void platform_windows_open_console();
	void platform_windows_close_console();
	int windows_get_registry_install_info(rct2_install_info *installInfo, char *source, char *font, uint8 charset);
	HWND windows_get_window_handle();
	void platform_setup_file_associations();
	void platform_remove_file_associations();
	// This function cannot be marked as 'static', even though it may seem to be,
	// as it requires external linkage, which 'static' prevents
	__declspec(dllexport) int StartOpenRCT(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
#endif // __WINDOWS__

#ifdef __3DS__
	// n3ds port: the game draws one virtual screen that holds both 3DS screens.
	// UI area: toolbars and all windows except the main view. The bottom screen shows a 320x240
	// page of it (windows open as sheets on that page; larger ones can be panned), so the area is
	// kept larger than the page for windows that do not fit yet.
	// Park area: the main view only, shown on the top screen.
	// Status area: the money and date panels, shown over the bottom of the top screen.
	// From top to bottom: UI area, status area, park area.
	// The park area starts at a multiple of 64, the height of the blocks in which the drawing
	// engine keeps track of what changed: its blocks are then all its own, and it is drawn (or,
	// while the view moves, not drawn) apart from what is above it (SoftwareDrawingEngine.cpp).
	// The rows between the status area and the park area are not used.
	#define N3DS_UI_WIDTH       640
	#define N3DS_UI_HEIGHT      480
	#define N3DS_PARK_X         0
	#define N3DS_PARK_Y         ((N3DS_UI_HEIGHT + N3DS_STATUS_HEIGHT + 63) & ~63)
	// The park area matches the top screen at display scale 1. At display scale 0.75 (a zoom step
	// between the game's own 2x steps) it is 4/3 of it and shrunk when shown.
	#define N3DS_TOP_WIDTH       400
	#define N3DS_TOP_HEIGHT      240
	#define N3DS_PARK_MAX_WIDTH  534
	#define N3DS_PARK_MAX_HEIGHT 320
	#define N3DS_SCREEN_WIDTH    N3DS_UI_WIDTH
	#define N3DS_SCREEN_HEIGHT   (N3DS_PARK_Y + N3DS_PARK_MAX_HEIGHT)
	#define N3DS_BOTTOM_WIDTH   320
	#define N3DS_BOTTOM_HEIGHT  240
	// The status area holds the money and date panels of the status bar (game_bottom_toolbar.c),
	// one in each corner. It is shown over the bottom of the top screen at its own size, whatever
	// the display scale of the park (shrinking the park area's pixels would spoil the text). It
	// lies right below the UI area, so that the status bar's window can reach from the news on
	// the HUD to its panels without covering any of the park area.
	#define N3DS_STATUS_X          0
	#define N3DS_STATUS_Y          N3DS_UI_HEIGHT
	#define N3DS_STATUS_WIDTH      N3DS_TOP_WIDTH
	#define N3DS_STATUS_HEIGHT     34
	// HUD on the bottom screen: the toolbar buttons (top_toolbar.c), shown 1.5x (30x28 on PC), and
	// the news. Layout drawn up by the user:
	//   top     game and view buttons, 6 in the first row, the rest in the second
	//   news    in the middle of the space between, while there is any (game_bottom_toolbar.c)
	//   blocks  3 buttons wide, in the bottom corners: manage on the left, build on the right
	#define N3DS_TOOLBAR_BUTTON_WIDTH      45
	#define N3DS_TOOLBAR_BUTTON_HEIGHT     42
	#define N3DS_HUD_ROW_PITCH             (N3DS_TOOLBAR_BUTTON_HEIGHT + 1)
	#define N3DS_HUD_BLOCKS_Y              (N3DS_BOTTOM_HEIGHT - N3DS_HUD_ROW_PITCH - N3DS_TOOLBAR_BUTTON_HEIGHT)
	#define N3DS_HUD_NEWS_HEIGHT           34
	#define N3DS_HUD_NEWS_Y                ((N3DS_HUD_ROW_PITCH * 2 - 1 + N3DS_HUD_BLOCKS_Y - N3DS_HUD_NEWS_HEIGHT) / 2)

	// The close box of a window is drawn this much wider, to the left (widget.c), and takes
	// taps around it (window_find_widget_from_point)
	#define N3DS_CLOSEBOX_EXTRA_WIDTH      12

	// How far from a small control a tap still counts as on it (window_find_widget_from_point)
	#define N3DS_TOUCH_MARGIN              8

	// Controls of windows laid out for the bottom screen (window_n3ds_place_dropdown, ride.c):
	// the box of a dropdown or spinner is this high and its buttons this wide. The original's
	// boxes are 12 high, a dropdown's button 11 wide, a spinner's two buttons 11 wide and 5 high.
	#define N3DS_CONTROL_HEIGHT            18
	#define N3DS_CONTROL_BUTTON_WIDTH      22
	// Text in a box that high is this far below the box's top (widget.c)
	#define N3DS_CONTROL_TEXT_OFFSET       ((N3DS_CONTROL_HEIGHT - 12) / 2)

	// n3ds.c
	void *platform_n3ds_temp_alloc(size_t size);
	void platform_n3ds_temp_free(void *memory);
	// Performance log: the parts of a frame that are timed
	enum {
		N3DS_PERF_UPDATE,	// input and game logic
		N3DS_PERF_PAINT,	// drawing the changed parts of the virtual screen
		N3DS_PERF_PRESENT,	// putting the virtual screen on the two screens
		// Within the update
		N3DS_PERF_EVENTS,	// SDL events, buttons and touch
		N3DS_PERF_LOGIC,	// the game's logic ticks, of which the next four are parts
		N3DS_PERF_PEEPS,
		N3DS_PERF_VEHICLES,
		N3DS_PERF_RIDES,
		N3DS_PERF_SOUNDS,	// choosing the sounds of vehicles, crowd and weather
		N3DS_PERF_WINDOWS,	// the update events of the windows
		N3DS_PERF_INPUT,	// mouse input, including finding what the cursor is over
		// Within the paint
		N3DS_PERF_VIEWS,	// moving the viewports: shifting their pixels, drawing the new edges
		N3DS_PERF_DIRTY,	// drawing the changed rectangles of the screen
		N3DS_PERF_UI,		// drawing the windows other than the main view (part of the two above)
		N3DS_PERF_GENERATE,	// viewport painting: collecting what is in a column of the map,
		N3DS_PERF_ARRANGE,	// sorting it,
		N3DS_PERF_SPRITES,	// drawing it
		// Wherever they happen
		N3DS_PERF_AUDIO,	// the mixer callback (audio thread; it interrupts the main thread)
		N3DS_PERF_MIXER_WAIT,	// the main thread waiting for the mixer callback to finish
		N3DS_PERF_MUSIC_START,	// starting a ride's music: opening its file
		N3DS_PERF_FOPEN,	// opening files
		N3DS_PERF_STRUCTS_FULL,	// (a count) columns that ran out of paint structs
		N3DS_PERF_COUNT
	};
	uint64 platform_n3ds_perf_begin();
	void platform_n3ds_perf_end(int part, uint64 begin);
	void platform_n3ds_perf_frame();

	// n3ds_input.cpp
	void platform_n3ds_input_init(SDL_Window *topWindow);
	void platform_n3ds_input_update();
	void platform_n3ds_input_free();
	void platform_n3ds_present(const uint8 *bits, int pitch, const SDL_Palette *palette);
	void platform_n3ds_get_mouse(int *x, int *y);
	void platform_n3ds_get_tool_position(int *x, int *y);
	void platform_n3ds_warp_mouse(int x, int y);
	void platform_n3ds_get_bottom_camera(int *x, int *y);
	void platform_n3ds_get_park_size(int *width, int *height);
	void platform_n3ds_zoom(int direction);
	bool platform_n3ds_can_zoom(int direction);
	bool platform_n3ds_pointer_is_touch();
	void platform_n3ds_apt_begin();
	void platform_n3ds_loading_begin();
	void platform_n3ds_loading_progress(int done, int total);
	void platform_n3ds_saving_begin();
	bool platform_n3ds_software_keyboard(utf8 *buffer, size_t size, const utf8 *hint);
	void platform_n3ds_get_rct1_path(utf8 *out, size_t size);
#endif

// n3ds port: times a statement as a part of the 3DS performance log (n3ds.c)
#ifdef __3DS__
	#define N3DS_PERF(part, statement) do { uint64 perfBegin_ = platform_n3ds_perf_begin(); statement; platform_n3ds_perf_end(part, perfBegin_); } while (0)
#else
	#define N3DS_PERF(part, statement) do { statement; } while (0)
#endif

// n3ds port: the 3DS implements these in n3ds.c
#if defined(__LINUX__) || defined(__MACOSX__) || defined(__3DS__)
	void platform_posix_sub_user_data_path(char *buffer, size_t size, const char *homedir);
	void platform_posix_sub_resolve_openrct_data_path(utf8 *out, size_t size);
#endif

#endif
