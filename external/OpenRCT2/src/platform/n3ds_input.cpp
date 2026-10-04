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
 * n3ds port: two-screen presentation and input for the Nintendo 3DS.
 *
 * Screen layout. The game draws one virtual screen (see platform.h):
 *   UI area     (0,0)    640x480  the toolbar, the news and every window except the main view
 *   Status area (0,480)  400x34   the money and date panels (game_bottom_toolbar.c) -> over the
 *                                 bottom of the top screen, at their own size
 *   Park area   (0,576)           the main window (park viewport) only -> top screen; 400x240, or
 *                                 534x320 shrunk to 400x240 at display scale 0.75 (see Zoom)
 * The bottom screen shows a 320x240 page of the UI area:
 *   - with no window open it shows the HUD: the toolbar buttons (top_toolbar.c) and, between
 *     them, the news (game_bottom_toolbar.c)
 *   - every other window opens as a sheet on the page (window.c). Windows fitted to the page
 *     show completely; a larger window can be panned by swiping outside its lists.
 *
 * Touch gestures on the bottom screen:
 *   tap         left click
 *   swipe       on a list: scroll the list; elsewhere: pan a window larger than the screen
 *   long press  press and hold the left button (drag things, hold repeating buttons)
 *
 * Buttons. The game only understands mouse and keyboard:
 *   - keys are fed through SDL's internal SDL_SendKeyboardKey (SDL2 is linked statically), so
 *     SDL_GetKeyboardState and key events behave as on a PC
 *   - the mouse position lives here, not in SDL, because SDL clamps its mouse to the 400x240 SDL
 *     window. Mouse events are pushed with these coordinates and shared.c asks this file for the
 *     cursor position (platform_get_cursor_position / platform_set_cursor_position).
 *   Circle pad  move the cursor on the top screen (park); pushing past the edge moves the view.
 *               While the player is still choosing what to build (the new ride, track design
 *               or scenery window in front, nothing picked yet) it moves the view itself and
 *               there is no cursor (see Browsing)
 *   A / B       left / right click at the top screen cursor (B held + circle pad: drag the view)
 *   D-pad       bottom screen: moves a focus through the buttons and list items of the window in
 *               front (or of the title screen); then A clicks the focused control and B closes
 *               the window (see Button navigation). The circle pad or a touch ends this.
 *               With only the HUD on the bottom screen: scroll view [arrows]
 *   X  rotate view [Enter]   Y  close top window [Backspace]
 *   L / R       zoom out / in, in half steps: besides the game's own 2x zoom levels the park
 *               view can be shown at 0.75 (the park area is drawn 4/3 larger and shrunk)
 *   ZL          rotate object [Z]      ZR held = Shift (with X: rotate anticlockwise)
 *   START       pause [Pause]          SELECT  cancel construction [Esc]
 *   START+SELECT  quit
 *
 * SDL's N3DS driver draws no mouse cursor, so the game's cursor is drawn on the top screen here.
 */

#ifdef __3DS__

#include <3ds.h>
#include <SDL.h>
#include <math.h>
#include <stdlib.h>

#include "../interface/Cursors.h"

extern "C"
{
    #include "../config.h"
    #include "../game.h"
    #include "../input.h"
    #include "../interface/themes.h"
    #include "../intro.h"
    #include "../interface/widget.h"
    #include "../interface/window.h"
    #include "../localisation/language.h"
    #include "../localisation/string_ids.h"
    #include "../title/TitleScreen.h"
    #include "../windows/dropdown.h"
    #include "../world/scenery.h"
    #include "platform.h"

    // SDL internal (src/events/SDL_keyboard_c.h)
    int SDL_SendKeyboardKey(Uint8 state, SDL_Scancode scancode);
    void SDL_SetKeyboardFocus(SDL_Window * window);

    // n3ds.c
    void platform_n3ds_log_memory(const char * where);
}

// Touch gesture thresholds
static const int SwipeStartDistance = 10;    // pixels before a touch counts as a swipe
static const u64 LongPressMs        = 400;   // still touch held this long = press and hold

enum TOUCH_MODE
{
    TOUCH_NONE,
    TOUCH_PENDING,   // finger down, not yet a tap, swipe or long press
    TOUCH_SCROLL,    // swiping a list (scroll widget)
    TOUCH_PAN,       // swiping elsewhere: pans a sheet larger than the screen
    TOUCH_HOLD,      // long press: left button held, follows the finger
};

static SDL_Window * _topWindow    = nullptr;
static SDL_Window * _bottomWindow = nullptr;

// Park view display scale: true = 0.75 (park area 534x320 shown on the 400x240 top screen)
static bool  _parkScaled = true;

// Mouse in virtual screen coordinates; the top screen cursor in top screen pixels
static int   _mouseX = N3DS_PARK_X + N3DS_TOP_WIDTH / 2;
static int   _mouseY = N3DS_PARK_Y + N3DS_TOP_HEIGHT / 2;
static int   _topCursorX = N3DS_TOP_WIDTH / 2;
static int   _topCursorY = N3DS_TOP_HEIGHT / 2;
static float _cursorFracX = 0;
static float _cursorFracY = 0;

// Top-left of the part of the UI area shown on the bottom screen
static int _cameraX = 0;
static int _cameraY = 0;

static u32  _prevKeys  = 0;
static u32  _ignoreKeys = 0;       // held when the system keyboard closed: not presses, until let go
static u32  _keysSent  = 0;        // KeyMap buttons currently reported to SDL as pressed keys
static bool _leftDown  = false;
static bool _rightDown = false;
static bool _tapRelease = false;   // a tap pressed the button last frame; release it now

static bool _focusActive   = false;   // the D-pad focus has the mouse (see Button navigation)
static bool _backHeld      = false;   // B closed a window; it is not a right click until released
static bool _pointerIsTouch = false;  // the mouse was last moved or clicked by a touch

static TOUCH_MODE       _touchMode = TOUCH_NONE;
static int              _touchStartX, _touchStartY;
static int              _touchLastX, _touchLastY;
static u64              _touchStartTime;
static rct_windowclass  _touchWindowClass;
static rct_windownumber _touchWindowNumber;
static int              _touchWidgetIndex = -1;
static bool             _touchMoved = false;   // the swipe scrolled a list or moved the page

// Browsing: the circle pad moves the view, not the cursor (see is_browsing)
static bool _browsing      = false;
static bool _browsePicked  = false;   // something was picked from the window's list
static bool _cursorPending = false;   // browsing ended: put the mouse back on the top screen cursor

static const struct
{
    u32          Key;
    SDL_Scancode Scancode;
} KeyMap[] =
{
    { KEY_DUP,    SDL_SCANCODE_UP        },
    { KEY_DDOWN,  SDL_SCANCODE_DOWN      },
    { KEY_DLEFT,  SDL_SCANCODE_LEFT      },
    { KEY_DRIGHT, SDL_SCANCODE_RIGHT     },
    { KEY_X,      SDL_SCANCODE_RETURN    },
    { KEY_Y,      SDL_SCANCODE_BACKSPACE },
    { KEY_ZL,     SDL_SCANCODE_Z         },
    { KEY_ZR,     SDL_SCANCODE_LSHIFT    },
    { KEY_START,  SDL_SCANCODE_PAUSE     },
    { KEY_SELECT, SDL_SCANCODE_ESCAPE    },
};

template<typename T> static T clamp_value(T value, T low, T high)
{
    return value < low ? low : (value > high ? high : value);
}

#pragma region Input between frames

// Buttons and touch are looked at once per frame, and a frame can take 100 ms and more here: a
// quick press or tap that began and ended between two frames was never seen, and the player
// had to press two or three times. A small thread looks at the HID module's shared memory every
// few milliseconds (where libctru's hidScanInput reads the state from) and notes what went
// down. The next frame takes that as held, so nothing is missed.
static const u32 SampledKeys = KEY_A | KEY_B | KEY_X | KEY_Y | KEY_L | KEY_R | KEY_START | KEY_SELECT |
    KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT;
static u32 _latchedKeys = 0;    // buttons that went down since the last frame
static u32 _latchedTouch = 0;   // a touch began since the last frame: bit 31, and where (touchPosition)
static Thread        _samplerThread = nullptr;
static volatile bool _samplerStop = false;

static void input_sampler(void *)
{
    u32 prevKeys = 0;
    bool prevTouch = false;
    while (!_samplerStop)
    {
        if (hidSharedMem != nullptr)
        {
            // The latest entry of the pad section and of the touch section (libctru hid.c)
            u32 padIndex = hidSharedMem[4];
            if (padIndex > 7) padIndex = 7;
            u32 keys = hidSharedMem[10 + padIndex * 4] & SampledKeys;
            u32 touchIndex = hidSharedMem[42 + 4];
            if (touchIndex > 7) touchIndex = 7;
            u32 position = hidSharedMem[42 + 8 + touchIndex * 2];
            bool touch = hidSharedMem[42 + 8 + touchIndex * 2 + 1] != 0;

            if (keys & ~prevKeys) __atomic_fetch_or(&_latchedKeys, keys & ~prevKeys, __ATOMIC_RELAXED);
            if (touch && !prevTouch) __atomic_store_n(&_latchedTouch, 0x80000000 | position, __ATOMIC_RELAXED);
            prevKeys = keys;
            prevTouch = touch;
        }
        svcSleepThread(4 * 1000 * 1000);
    }
}

#pragma endregion

#pragma region Bottom screen page

static rct_window * front_sheet()
{
    for (rct_window * w = gWindowNextSlot - 1; w >= g_window_list; w--)
    {
        if (window_n3ds_is_sheet(w->classification)) return w;
    }
    return nullptr;
}

// Keeps the camera on the page, or within the front sheet when that is larger than the page.
static void clamp_camera()
{
    rct_window * sheet = front_sheet();
    int minX = 0, maxX = 0, minY = 0, maxY = 0;
    if (sheet != nullptr)
    {
        if (sheet->x + sheet->width > N3DS_BOTTOM_WIDTH)
        {
            minX = 0;
            maxX = sheet->x + sheet->width - N3DS_BOTTOM_WIDTH;
        }
        if (sheet->y + sheet->height > N3DS_BOTTOM_HEIGHT)
        {
            minY = 0;
            maxY = sheet->y + sheet->height - N3DS_BOTTOM_HEIGHT;
        }
    }
    _cameraX = clamp_value(_cameraX, minX, maxX);
    _cameraY = clamp_value(_cameraY, minY, maxY);
}

static bool camera_pannable()
{
    rct_window * sheet = front_sheet();
    return sheet != nullptr &&
        (sheet->x + sheet->width > N3DS_BOTTOM_WIDTH || sheet->y + sheet->height > N3DS_BOTTOM_HEIGHT);
}

// Windows are identified by class and number; their slots in g_window_list move around.
struct WindowId
{
    rct_windowclass  Class;
    rct_windownumber Number;
};
static WindowId _knownWindows[WINDOW_LIMIT_MAX + WINDOW_LIMIT_RESERVED];
static int      _knownWindowCount = 0;
static WindowId _frontSheet = { 255, 0 };

static bool was_known(const rct_window * w)
{
    for (int i = 0; i < _knownWindowCount; i++)
    {
        if (_knownWindows[i].Class == w->classification && _knownWindows[i].Number == w->number)
            return true;
    }
    return false;
}

// A resizable window that opens larger than the page is shrunk to fit it, as far as the
// window's own minimum size allows (lists then scroll inside the window).
static void fit_resizable_window(rct_window * w)
{
    if (!(w->flags & WF_RESIZABLE)) return;
    int dw = 0, dh = 0;
    if (w->width > N3DS_BOTTOM_WIDTH) dw = N3DS_BOTTOM_WIDTH - w->width;
    if (w->height > N3DS_BOTTOM_HEIGHT) dh = N3DS_BOTTOM_HEIGHT - w->height;
    if (dw == 0 && dh == 0) return;
    window_resize(w, dw, dh);
}

static void set_default_zoom();

static void update_windows()
{
    bool parkLoaded = false;
    for (rct_window * w = g_window_list; w < gWindowNextSlot; w++)
    {
        if (w->classification == WC_TOP_TOOLBAR && !was_known(w)) parkLoaded = true;
        // The main view has an area of its own; the status bar's window reaches from the news on
        // the HUD into the status area
        if (w->classification == WC_MAIN_WINDOW || w->classification == WC_BOTTOM_TOOLBAR) continue;
        if (!was_known(w) && window_n3ds_is_sheet(w->classification))
        {
            // window_create placed it already, but some windows set their own position right
            // after (e.g. the park objective window centres itself on the whole virtual screen)
            fit_resizable_window(w);
            window_n3ds_place_sheet(w);
        }
        // Nothing but the main view may reach into the park area (the top screen)
        if (w->y + w->height > N3DS_UI_HEIGHT)
        {
            window_set_position(w, w->x, clamp_value(N3DS_UI_HEIGHT - w->height, 0, N3DS_UI_HEIGHT));
        }
    }

    if (parkLoaded)
    {
        set_default_zoom();
    }

    // A different sheet in front starts at its top-left corner
    rct_window * sheet = front_sheet();
    WindowId front = { 255, 0 };
    if (sheet != nullptr)
    {
        front.Class = sheet->classification;
        front.Number = sheet->number;
    }
    if (front.Class != _frontSheet.Class || front.Number != _frontSheet.Number)
    {
        _cameraX = 0;
        _cameraY = 0;
        _frontSheet = front;
    }
    clamp_camera();

    _knownWindowCount = 0;
    for (rct_window * w = g_window_list; w < gWindowNextSlot; w++)
    {
        _knownWindows[_knownWindowCount].Class = w->classification;
        _knownWindows[_knownWindowCount].Number = w->number;
        _knownWindowCount++;
    }
}

void platform_n3ds_get_bottom_camera(int * x, int * y)
{
    *x = _cameraX;
    *y = _cameraY;
}

#pragma endregion

#pragma region Mouse

static void push_mouse_motion()
{
    SDL_Event e = {};
    e.type = SDL_MOUSEMOTION;
    e.motion.windowID = SDL_GetWindowID(_topWindow);
    e.motion.x = _mouseX;
    e.motion.y = _mouseY;
    e.motion.state = (_leftDown ? SDL_BUTTON_LMASK : 0) | (_rightDown ? SDL_BUTTON_RMASK : 0);
    SDL_PushEvent(&e);
}

static void move_mouse(int x, int y)
{
    if (x == _mouseX && y == _mouseY) return;
    _mouseX = x;
    _mouseY = y;
    push_mouse_motion();
}

static void set_mouse_button(bool down, Uint8 button, bool * state)
{
    if (down == *state) return;
    *state = down;
    SDL_Event e = {};
    e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    e.button.windowID = SDL_GetWindowID(_topWindow);
    e.button.button = button;
    e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.button.clicks = 1;
    e.button.x = _mouseX;
    e.button.y = _mouseY;
    SDL_PushEvent(&e);
}

void platform_n3ds_get_park_size(int * width, int * height)
{
    *width = _parkScaled ? N3DS_PARK_MAX_WIDTH : N3DS_TOP_WIDTH;
    *height = _parkScaled ? N3DS_PARK_MAX_HEIGHT : N3DS_TOP_HEIGHT;
}

static bool in_park(int x, int y)
{
    int width, height;
    platform_n3ds_get_park_size(&width, &height);
    return x >= N3DS_PARK_X && x < N3DS_PARK_X + width &&
           y >= N3DS_PARK_Y && y < N3DS_PARK_Y + height;
}

static bool in_status(int x, int y)
{
    return x >= N3DS_STATUS_X && x < N3DS_STATUS_X + N3DS_STATUS_WIDTH &&
           y >= N3DS_STATUS_Y && y < N3DS_STATUS_Y + N3DS_STATUS_HEIGHT;
}

// The status area is shown over the bottom rows of the top screen, from this row down
static const int StatusTopY = N3DS_TOP_HEIGHT - N3DS_STATUS_HEIGHT;

// Top screen pixel <-> virtual screen position. Where the status area has a panel the pixel
// belongs to it, so the cursor can click the panel; elsewhere to the park area.
static void top_to_virtual(int topX, int topY, int * x, int * y)
{
    if (topY >= StatusTopY)
    {
        int statusX = N3DS_STATUS_X + topX;
        int statusY = N3DS_STATUS_Y + topY - StatusTopY;
        rct_window * w = window_find_from_point(statusX, statusY);
        if (w != nullptr && w->classification == WC_BOTTOM_TOOLBAR)
        {
            *x = statusX;
            *y = statusY;
            return;
        }
    }

    int width, height;
    platform_n3ds_get_park_size(&width, &height);
    *x = N3DS_PARK_X + topX * width / N3DS_TOP_WIDTH;
    *y = N3DS_PARK_Y + topY * height / N3DS_TOP_HEIGHT;
}

static void virtual_to_top(int x, int y, int * topX, int * topY)
{
    if (in_status(x, y))
    {
        *topX = x - N3DS_STATUS_X;
        *topY = y - N3DS_STATUS_Y + StatusTopY;
        return;
    }

    int width, height;
    platform_n3ds_get_park_size(&width, &height);
    *topX = (x - N3DS_PARK_X) * N3DS_TOP_WIDTH / width;
    *topY = (y - N3DS_PARK_Y) * N3DS_TOP_HEIGHT / height;
}

void platform_n3ds_get_mouse(int * x, int * y)
{
    *x = _mouseX;
    *y = _mouseY;
}

// Where a tool (placing scenery or a ride, changing the land) works. The game has one mouse:
// with it on the bottom screen, to rotate what is being placed, say, the tool had nowhere on
// the map to be, and what was being placed disappeared from the top screen together with the
// cursor until the circle pad was moved again (user's report). So while the mouse is not on
// the top screen the tool stays where the top screen's cursor was left, and the cursor stays
// drawn there (platform_n3ds_present). Not while browsing: there is no cursor then.
static bool tool_stays_on_top()
{
    return !_browsing && !in_park(_mouseX, _mouseY) && !in_status(_mouseX, _mouseY);
}

void platform_n3ds_get_tool_position(int * x, int * y)
{
    if (tool_stays_on_top())
    {
        top_to_virtual(_topCursorX, _topCursorY, x, y);
    }
}

void platform_n3ds_warp_mouse(int x, int y)
{
    // The game warps the cursor back while the view is dragged with the right button.
    _mouseX = x;
    _mouseY = y;
    if (in_park(x, y))
    {
        virtual_to_top(x, y, &_topCursorX, &_topCursorY);
    }
}

// Moves the main view by a distance given in top screen pixels.
static void scroll_view_by(int topDx, int topDy)
{
    if (topDx == 0 && topDy == 0) return;
    rct_window * w = window_get_main();
    if (w == nullptr || w->viewport == nullptr) return;
    if ((w->flags & WF_NO_SCROLLING) || (gScreenFlags & SCREEN_FLAGS_TITLE_DEMO)) return;
    int width, height;
    platform_n3ds_get_park_size(&width, &height);
    // top pixels -> park area pixels -> map view units at the current zoom
    w->saved_view_x += (topDx * width / N3DS_TOP_WIDTH) << w->viewport->zoom;
    w->saved_view_y += (topDy * height / N3DS_TOP_HEIGHT) << w->viewport->zoom;
    gInputFlags |= INPUT_FLAG_VIEWPORT_SCROLLING;
}

// Circle pad: how far it moves things this frame, in top screen pixels. False if not at all.
static bool read_circle_pad(int * outStepX, int * outStepY)
{
    const float deadZone = 20.0f;
    const float maxRange = 156.0f;
    const float maxSpeed = 9.0f;   // pixels per frame at full tilt

    circlePosition cp;
    hidCircleRead(&cp);
    float dx = (float)cp.dx;
    float dy = -(float)cp.dy;      // circle pad up is positive, screen y grows downwards
    float magnitude = sqrtf(dx * dx + dy * dy);
    if (magnitude <= deadZone)
    {
        _cursorFracX = 0;
        _cursorFracY = 0;
        return false;
    }

    // Quadratic curve: small tilts give precise movement, full tilt crosses the screen quickly.
    float t = (magnitude - deadZone) / (maxRange - deadZone);
    if (t > 1) t = 1;
    float speed = 0.5f + t * t * maxSpeed;
    _cursorFracX += dx / magnitude * speed;
    _cursorFracY += dy / magnitude * speed;
    int stepX = (int)_cursorFracX;
    int stepY = (int)_cursorFracY;
    _cursorFracX -= stepX;
    _cursorFracY -= stepY;
    if (stepX == 0 && stepY == 0) return false;
    *outStepX = stepX;
    *outStepY = stepY;
    return true;
}

static void move_top_cursor(int stepX, int stepY)
{
    int wantX = _topCursorX + stepX;
    int wantY = _topCursorY + stepY;
    _topCursorX = clamp_value(wantX, 0, N3DS_TOP_WIDTH - 1);
    _topCursorY = clamp_value(wantY, 0, N3DS_TOP_HEIGHT - 1);

    // Pushing the cursor past the edge of the top screen moves the view instead, at the same
    // speed (like the game's edge scrolling, which is off on the 3DS)
    scroll_view_by(wantX - _topCursorX, wantY - _topCursorY);
}

#pragma endregion

#pragma region Browsing

// With the new ride, track design or scenery window in front the player is still choosing what
// to build, and looks around the park for where: the circle pad then moves the view itself and
// there is no cursor. A and B work on the window (the focus stays on it). Once something is
// picked the cursor is back, to say how and where to put it: picking a ride or a design opens
// the window that builds it, and the scenery window stays, so there it is the click on a piece
// of scenery in its list that ends the browsing. (Controls asked for by the user.)
static bool is_browsing()
{
    static WindowId sheetInFront = { 255, 0 };
    rct_window * sheet = front_sheet();
    WindowId id = { 255, 0 };
    if (sheet != nullptr)
    {
        id.Class = sheet->classification;
        id.Number = sheet->number;
    }
    if (id.Class != sheetInFront.Class || id.Number != sheetInFront.Number)
    {
        sheetInFront = id;
        _browsePicked = false;
    }

    switch (id.Class)
    {
    case WC_CONSTRUCT_RIDE:
    case WC_TRACK_DESIGN_LIST:
        return true;
    case WC_SCENERY:
        // The repaint tool also needs the cursor
        return !_browsePicked && gWindowSceneryPaintEnabled != 1;
    default:
        return false;
    }
}

// A click (A or a tap) is going to a widget: one on the window's list picks something
static void note_click_on_widget(rct_window * w, int widgetIndex)
{
    if (_browsing && w != nullptr && widgetIndex >= 0 && w == front_sheet() &&
        w->widgets[widgetIndex].type == WWT_SCROLL)
    {
        _browsePicked = true;
    }
}

#pragma endregion

#pragma region Touch gestures

// Size of the part of a scroll widget that shows its content (inside the border and scrollbars)
static void get_scroll_view_size(rct_window * w, int widgetIndex, int * width, int * height)
{
    rct_widget * widget = &w->widgets[widgetIndex];
    rct_scroll * scroll = &w->scrolls[window_get_scroll_data_index(w, widgetIndex)];
    *width = widget->right - widget->left - 1;
    *height = widget->bottom - widget->top - 1;
    if (scroll->flags & VSCROLLBAR_VISIBLE) *width -= 11;
    if (scroll->flags & HSCROLLBAR_VISIBLE) *height -= 11;
}

// Scrolls a list (scroll widget) by the finger movement. Returns false if it cannot scroll.
static bool scroll_widget_by(rct_window * w, int widgetIndex, int dx, int dy)
{
    rct_widget * widget = &w->widgets[widgetIndex];
    if (widget->type != WWT_SCROLL) return false;
    rct_scroll * scroll = &w->scrolls[window_get_scroll_data_index(w, widgetIndex)];

    int viewWidth, viewHeight;
    get_scroll_view_size(w, widgetIndex, &viewWidth, &viewHeight);
    int maxLeft = (int)scroll->h_right - viewWidth;
    int maxTop = (int)scroll->v_bottom - viewHeight;
    if (maxLeft < 0) maxLeft = 0;
    if (maxTop < 0) maxTop = 0;
    if (maxLeft == 0 && maxTop == 0) return false;

    int left = clamp_value((int)scroll->h_left - dx, 0, maxLeft);
    int top = clamp_value((int)scroll->v_top - dy, 0, maxTop);
    if (left != scroll->h_left || top != scroll->v_top)
    {
        scroll->h_left = left;
        scroll->v_top = top;
        widget_scroll_update_thumbs(w, widgetIndex);
        window_invalidate(w);
    }
    return true;
}

static bool widget_can_scroll(rct_window * w, int widgetIndex)
{
    if (w == nullptr || widgetIndex < 0) return false;
    rct_widget * widget = &w->widgets[widgetIndex];
    if (widget->type != WWT_SCROLL) return false;
    rct_scroll * scroll = &w->scrolls[window_get_scroll_data_index(w, widgetIndex)];
    return (scroll->flags & (VSCROLLBAR_VISIBLE | HSCROLLBAR_VISIBLE)) != 0;
}

static void update_touch(u32 keys)
{
    bool touching = (keys & KEY_TOUCH) != 0;
    // A touch that began since the last frame, even if it is over already (input_sampler)
    u32 latched = __atomic_exchange_n(&_latchedTouch, 0, __ATOMIC_RELAXED);
    bool began = _touchMode == TOUCH_NONE && (latched & 0x80000000) != 0;
    if (!touching && !began)
    {
        bool tap = _touchMode == TOUCH_PENDING;
        if (_touchMode == TOUCH_PAN && !_touchMoved && _touchWidgetIndex >= 0)
        {
            // The finger slid a little with nothing there to move: a tap after all, if it
            // stayed on the control it came down on. (A thumb rarely stays within a few pixels.)
            int x = _cameraX + _touchLastX, y = _cameraY + _touchLastY;
            rct_window * w = window_find_from_point(x, y);
            tap = w != nullptr && w->classification == _touchWindowClass && w->number == _touchWindowNumber &&
                window_find_widget_from_point(w, x, y) == _touchWidgetIndex;
        }
        if (tap)
        {
            // Tap: click where the finger was. The button goes up again next frame.
            _pointerIsTouch = true;
            move_mouse(_cameraX + _touchStartX, _cameraY + _touchStartY);
            set_mouse_button(true, SDL_BUTTON_LEFT, &_leftDown);
            _tapRelease = true;
            note_click_on_widget(window_find_by_number(_touchWindowClass, _touchWindowNumber), _touchWidgetIndex);
        }
        _touchMode = TOUCH_NONE;
        return;
    }

    touchPosition touch;
    if (touching)
    {
        hidTouchRead(&touch);
    }
    else
    {
        // Over already: it begins this frame where it came down, and ends as a tap in the next
        touch.px = latched & 0xFFFF;
        touch.py = (latched >> 16) & 0x7FFF;
    }
    int uiX = _cameraX + touch.px;
    int uiY = _cameraY + touch.py;

    if (_touchMode == TOUCH_NONE)
    {
        _touchMode = TOUCH_PENDING;
        _touchStartX = _touchLastX = touch.px;
        _touchStartY = _touchLastY = touch.py;
        _touchStartTime = osGetTime();
        _touchMoved = false;
        rct_window * w = window_find_from_point(uiX, uiY);
        _touchWidgetIndex = -1;
        if (w != nullptr)
        {
            _touchWindowClass = w->classification;
            _touchWindowNumber = w->number;
            _touchWidgetIndex = window_find_widget_from_point(w, uiX, uiY);
        }
        // Hover first, so things like the highlighted ride in a list update before the click
        _pointerIsTouch = true;
        _focusActive = false;
        move_mouse(uiX, uiY);
        return;
    }

    int dx = touch.px - _touchLastX;
    int dy = touch.py - _touchLastY;
    _touchLastX = touch.px;
    _touchLastY = touch.py;

    if (_touchMode == TOUCH_PENDING)
    {
        int distance = abs(touch.px - _touchStartX) + abs(touch.py - _touchStartY);
        if (distance >= SwipeStartDistance)
        {
            rct_window * w = _touchWidgetIndex >= 0 ? window_find_by_number(_touchWindowClass, _touchWindowNumber) : nullptr;
            _touchMode = widget_can_scroll(w, _touchWidgetIndex) ? TOUCH_SCROLL : TOUCH_PAN;
            dx = touch.px - _touchStartX;
            dy = touch.py - _touchStartY;
        }
        else if (osGetTime() - _touchStartTime >= LongPressMs)
        {
            _touchMode = TOUCH_HOLD;
            set_mouse_button(true, SDL_BUTTON_LEFT, &_leftDown);
            note_click_on_widget(window_find_by_number(_touchWindowClass, _touchWindowNumber), _touchWidgetIndex);
        }
    }

    switch (_touchMode)
    {
    case TOUCH_SCROLL:
    {
        rct_window * w = window_find_by_number(_touchWindowClass, _touchWindowNumber);
        if (w == nullptr || !scroll_widget_by(w, _touchWidgetIndex, dx, dy))
            _touchMode = TOUCH_PAN;
        else
            _touchMoved = true;
        break;
    }
    case TOUCH_PAN:
        if (camera_pannable())
        {
            _cameraX -= dx;
            _cameraY -= dy;
            clamp_camera();
            _touchMoved = true;
        }
        break;
    case TOUCH_HOLD:
        move_mouse(uiX, uiY);
        break;
    default:
        break;
    }
}

#pragma endregion

#pragma region Button navigation

// The bottom screen can be used with the buttons alone. The D-pad moves a focus through the
// controls of the window in front: its buttons and the items of its list. The focus is the mouse
// resting on the control, so the game reacts as to a PC mouse hovering there (list items
// highlight, tooltips appear) and A, the left button, clicks it. B closes the window.

struct FocusTarget
{
    rct_window * Window;
    int          WidgetIndex;
    int          ItemIndex;                  // list item, or -1 for a widget
    int          Left, Top, Right, Bottom;   // UI area, inclusive; list items can lie outside the list's view
};

static const int MaxFocusTargets = 512;
static FocusTarget _targets[MaxFocusTargets];
static int         _targetCount = 0;
static WindowId    _focusWindow = { 255, 0 };   // the (first) window the focus is in
// The window and the control (its middle) the focus was on when a dropdown took it: it goes
// back there when the dropdown closes
static WindowId    _focusReturnWindow = { 255, 0 };
static int         _focusReturnX = 0, _focusReturnY = 0;
static int         _focusLastX = 0, _focusLastY = 0;   // the middle of the control it was last on
static SDL_Rect    _focusRect = { 0, 0, 0, 0 }; // focused control (UI area), drawn as a frame; w == 0: none
static bool        _focusOwnHighlight = false;  // the window shows the focus itself: no frame

static const u32 DpadKeys = KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT;
static const u64 DpadRepeatDelayMs = 350;   // held this long, a D-pad key starts repeating
static const u64 DpadRepeatMs      = 90;

// The windows the focus moves through: an open dropdown, else the sheet in front, or the
// buttons of the title screen. In a game with only the HUD on the bottom screen there are none
// and the D-pad scrolls the view.
static int get_focus_windows(rct_window ** windows)
{
    rct_window * dropdown = window_find_by_class(WC_DROPDOWN);
    if (dropdown != nullptr)
    {
        windows[0] = dropdown;
        return 1;
    }
    rct_window * sheet = front_sheet();
    if (sheet != nullptr)
    {
        windows[0] = sheet;
        return 1;
    }
    int count = 0;
    if (gScreenFlags & SCREEN_FLAGS_TITLE_DEMO)
    {
        static const rct_windowclass titleClasses[] = { WC_TITLE_MENU, WC_TITLE_EXIT };
        for (rct_windowclass cls : titleClasses)
        {
            rct_window * w = window_find_by_class(cls);
            if (w != nullptr) windows[count++] = w;
        }
    }
    return count;
}

// The part of a scroll widget that shows its content, in the UI area (see widget_scroll_get_part)
static void get_list_view(rct_window * w, int widgetIndex, SDL_Rect * view)
{
    int width, height;
    get_scroll_view_size(w, widgetIndex, &width, &height);
    view->x = w->x + w->widgets[widgetIndex].left + 1;
    view->y = w->y + w->widgets[widgetIndex].top + 1;
    view->w = width;
    view->h = height;
}

static void add_target(rct_window * w, int widgetIndex, int itemIndex, int left, int top, int right, int bottom)
{
    if (_targetCount >= MaxFocusTargets) return;
    FocusTarget * target = &_targets[_targetCount++];
    target->Window = w;
    target->WidgetIndex = widgetIndex;
    target->ItemIndex = itemIndex;
    target->Left = left;
    target->Top = top;
    target->Right = right;
    target->Bottom = bottom;
}

// Lists every control the focus can be on. The order only depends on the windows' contents.
static void collect_targets(rct_window ** windows, int numWindows)
{
    _targetCount = 0;
    for (int i = 0; i < numWindows; i++)
    {
        rct_window * w = windows[i];
        if (w->classification == WC_DROPDOWN)
        {
            // The items of the dropdown that can be chosen. A long dropdown is a list that
            // scrolls (dropdown.c): its items are list items, which the focus scrolls into view.
            int listWidget = window_dropdown_n3ds_list_widget();
            int top = w->y - (listWidget >= 0 ? w->scrolls[0].v_top : 0);
            int x, y, width, height;
            bool selectable;
            for (int item = 0; window_dropdown_n3ds_get_item(item, &x, &y, &width, &height, &selectable); item++)
            {
                if (!selectable) continue;
                add_target(w, listWidget >= 0 ? listWidget : 0, listWidget >= 0 ? item : -1,
                    w->x + x, top + y, w->x + x + width - 1, top + y + height - 1);
            }
            continue;
        }
        int widgetIndex = 0;
        for (rct_widget * widget = w->widgets; widget->type != WWT_LAST; widget++, widgetIndex++)
        {
            switch (widget->type)
            {
            case WWT_EMPTY:
            case WWT_FRAME:
            case WWT_RESIZE:
            case WWT_CAPTION:
            case WWT_VIEWPORT:
                break;
            case WWT_CLOSEBOX:
                // B closes the window. Other buttons of this widget type are controls like any.
                if (widget->text == STR_CLOSE_X) break;
                if (widget_is_enabled(w, widgetIndex) && !widget_is_disabled(w, widgetIndex))
                {
                    add_target(w, widgetIndex, -1, w->x + widget->left, w->y + widget->top, w->x + widget->right, w->y + widget->bottom);
                }
                break;
            case WWT_SCROLL:
            {
                // The items of the window's list, if the window tells where they are
                if (window_get_scroll_data_index(w, widgetIndex) != 0) break;
                SDL_Rect view;
                get_list_view(w, widgetIndex, &view);
                int originX = view.x - w->scrolls[0].h_left;
                int originY = view.y - w->scrolls[0].v_top;
                int x, y, width, height;
                for (int item = 0; window_n3ds_get_list_item(w, item, &x, &y, &width, &height); item++)
                {
                    // Rows are reported wider than the list; none of these lists scrolls sideways
                    if (width > view.w - x) width = view.w - x;
                    add_target(w, widgetIndex, item, originX + x, originY + y, originX + x + width - 1, originY + y + height - 1);
                }
                break;
            }
            default:
                // What a click can press (input_widget_left)
                if (widget_is_enabled(w, widgetIndex) && !widget_is_disabled(w, widgetIndex))
                {
                    add_target(w, widgetIndex, -1, w->x + widget->left, w->y + widget->top, w->x + widget->right, w->y + widget->bottom);
                }
                break;
            }
        }
    }

    // A control around other controls is not a stop: the box of a spinner or a dropdown, where
    // the window has it enabled, with its buttons inside. The focus goes by where the mouse
    // rests, and on a button the mouse is on the box as well: found first, the box had the
    // focus again and the D-pad never got past it sideways (user's report: from the number of
    // trains to the cars per train). A click on the box does what its button does, or nothing.
    static bool container[MaxFocusTargets];
    for (int i = 0; i < _targetCount; i++)
    {
        const FocusTarget & outer = _targets[i];
        container[i] = false;
        for (int j = 0; j < _targetCount && !container[i] && outer.ItemIndex < 0; j++)
        {
            const FocusTarget & inner = _targets[j];
            container[i] = j != i && inner.Window == outer.Window && inner.ItemIndex < 0 &&
                inner.Left >= outer.Left && inner.Right <= outer.Right &&
                inner.Top >= outer.Top && inner.Bottom <= outer.Bottom &&
                (inner.Right - inner.Left < outer.Right - outer.Left || inner.Bottom - inner.Top < outer.Bottom - outer.Top);
        }
    }
    int kept = 0;
    for (int i = 0; i < _targetCount; i++)
    {
        if (!container[i]) _targets[kept++] = _targets[i];
    }
    _targetCount = kept;
}

// The part of a target that shows (a list item is cut off by its list's view). False if none.
static bool get_visible_rect(const FocusTarget * target, SDL_Rect * rect)
{
    int left = target->Left, top = target->Top, right = target->Right, bottom = target->Bottom;
    if (target->ItemIndex >= 0)
    {
        SDL_Rect view;
        get_list_view(target->Window, target->WidgetIndex, &view);
        if (left < view.x) left = view.x;
        if (top < view.y) top = view.y;
        if (right > view.x + view.w - 1) right = view.x + view.w - 1;
        if (bottom > view.y + view.h - 1) bottom = view.y + view.h - 1;
    }
    if (left > right || top > bottom) return false;
    rect->x = left;
    rect->y = top;
    rect->w = right - left + 1;
    rect->h = bottom - top + 1;
    return true;
}

static int find_target_at(int x, int y)
{
    for (int i = 0; i < _targetCount; i++)
    {
        SDL_Rect rect;
        if (get_visible_rect(&_targets[i], &rect) &&
            x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h)
        {
            return i;
        }
    }
    return -1;
}

// Where the focus starts in a window: the first item of its list, otherwise its first control
static int find_default_target()
{
    for (int i = 0; i < _targetCount; i++)
    {
        if (_targets[i].ItemIndex == 0) return i;
    }
    return _targetCount > 0 ? 0 : -1;
}

static int find_nearest_target(int x, int y)
{
    int best = -1, bestDistance = 0;
    for (int i = 0; i < _targetCount; i++)
    {
        const FocusTarget * t = &_targets[i];
        int distance = abs((t->Left + t->Right) / 2 - x) + abs((t->Top + t->Bottom) / 2 - y);
        if (best < 0 || distance < bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

static int find_nearest_visible_target(int x, int y)
{
    int best = -1, bestDistance = 0;
    for (int i = 0; i < _targetCount; i++)
    {
        SDL_Rect rect;
        if (!get_visible_rect(&_targets[i], &rect)) continue;
        int distance = abs(rect.x + rect.w / 2 - x) + abs(rect.y + rect.h / 2 - y);
        if (best < 0 || distance < bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

// Distance between two ranges, 0 if they overlap
static int range_gap(int lowA, int highA, int lowB, int highB)
{
    if (lowB > highA) return lowB - highA;
    if (lowA > highB) return lowA - highB;
    return 0;
}

// The scenery window shows the item the mouse is on as a lighter box, the original's own
// highlight (window_scenery_scrollpaint). The focus is the mouse resting on the item, so the
// box follows it; the frame is left out there (user's request).
static bool target_has_own_highlight(const FocusTarget * target)
{
    return target->ItemIndex >= 0 && target->Window->classification == WC_SCENERY;
}

static bool target_is_tab(const FocusTarget * target)
{
    return target->ItemIndex < 0 && target->Window->widgets[target->WidgetIndex].type == WWT_TAB;
}

// The target in the nearest row of controls below (sign 1) or above (-1) a target, the one in
// it closest sideways, or -1. The row: the nearest control that lies wholly beyond the target,
// and what is level with it. Controls that do not show are left out.
static int find_target_in_next_row(int from, int sign)
{
    const FocusTarget * current = &_targets[from];
    int best = -1, bestScore = 0;
    // First round: the nearest control (rowStart). Second: the closest sideways of its row.
    int rowStart = -1;
    for (int round = 0; round < 2; round++)
    {
        for (int i = 0; i < _targetCount; i++)
        {
            const FocusTarget * t = &_targets[i];
            SDL_Rect rect;
            if (i == from || !get_visible_rect(t, &rect)) continue;
            int distance = sign > 0 ? t->Top - current->Bottom : current->Top - t->Bottom;
            if (distance <= 0) continue;

            int score = distance;
            if (round == 1)
            {
                const FocusTarget * start = &_targets[rowStart];
                if (range_gap(start->Top, start->Bottom, t->Top, t->Bottom) != 0) continue;
                score = range_gap(current->Left, current->Right, t->Left, t->Right) * 1024 +
                    abs((t->Left + t->Right) - (current->Left + current->Right));
            }
            if (best < 0 || score < bestScore)
            {
                best = i;
                bestScore = score;
            }
        }
        if (best < 0) return -1;
        if (round == 0)
        {
            rowStart = best;
            best = -1;
        }
    }
    return best;
}

// The target to move to from a target in the direction of a D-pad key, or -1. One straight ahead
// (overlapping the current one sideways) comes first, the nearest of them. Otherwise the nearest
// one no more than 45 degrees off the direction, so controls off to the side can be reached too.
// Up and down, failing those: the nearest row of controls that way.
// Tabs are the headings of the page below them (user's request): down from a tab goes into the
// page at its first row, wherever in that row the controls are (straight below a tab there is
// often nothing, or only something far down the page; a second row of tabs is such a row too),
// and up into the tabs goes to the tab of the page shown.
static int find_target_towards(int from, u32 key)
{
    const FocusTarget * current = &_targets[from];
    bool horizontal = (key & (KEY_DLEFT | KEY_DRIGHT)) != 0;
    int sign = (key & (KEY_DRIGHT | KEY_DDOWN)) ? 1 : -1;
    if (!horizontal && sign > 0 && target_is_tab(current))
    {
        int next = find_target_in_next_row(from, sign);
        if (next >= 0) return next;
    }
    int best = -1, bestScore = 0;
    if (current->ItemIndex >= 0)
    {
        // Within a list the next item straight ahead comes before anything else. The items
        // scrolled out of view are targets too, at the places they would have beyond the list's
        // view, which is where other controls are: going up through a list that was scrolled
        // down, the tabs above it were nearer than the row scrolled out, and the focus went to
        // the tabs from every row (user's report).
        for (int i = 0; i < _targetCount; i++)
        {
            const FocusTarget * t = &_targets[i];
            if (i == from || t->ItemIndex < 0 || t->Window != current->Window || t->WidgetIndex != current->WidgetIndex) continue;
            int along, across;
            if (horizontal)
            {
                along = sign * ((t->Left + t->Right) - (current->Left + current->Right));
                across = range_gap(current->Top, current->Bottom, t->Top, t->Bottom);
            }
            else
            {
                along = sign * ((t->Top + t->Bottom) - (current->Top + current->Bottom));
                across = range_gap(current->Left, current->Right, t->Left, t->Right);
            }
            if (along <= 0 || across != 0) continue;
            if (best < 0 || along < bestScore)
            {
                best = i;
                bestScore = along;
            }
        }
        if (best >= 0) return best;
    }
    for (int i = 0; i < _targetCount; i++)
    {
        if (i == from) continue;
        const FocusTarget * t = &_targets[i];
        // along: distance between the centres in the direction, doubled; across: gap sideways
        int along, across;
        if (horizontal)
        {
            along = sign * ((t->Left + t->Right) - (current->Left + current->Right));
            across = range_gap(current->Top, current->Bottom, t->Top, t->Bottom);
        }
        else
        {
            along = sign * ((t->Top + t->Bottom) - (current->Top + current->Bottom));
            across = range_gap(current->Left, current->Right, t->Left, t->Right);
        }
        if (along <= 0 || across * 2 > along) continue;
        int score = along + (across > 0 ? 100000 + across * 4 : 0);
        if (best < 0 || score < bestScore)
        {
            best = i;
            bestScore = score;
        }
    }
    if (best < 0 && !horizontal)
    {
        best = find_target_in_next_row(from, sign);
    }
    if (best >= 0 && !horizontal && sign < 0 && !target_is_tab(current) && target_is_tab(&_targets[best]))
    {
        for (int i = 0; i < _targetCount; i++)
        {
            const FocusTarget * t = &_targets[i];
            if (target_is_tab(t) && t->Window == _targets[best].Window && widget_is_pressed(t->Window, t->WidgetIndex))
            {
                return i;
            }
        }
    }
    return best;
}

// Puts the focus on a target: scrolls its list to show it, rests the mouse on it.
static void focus_on(rct_window ** windows, int numWindows, int index, bool scrollList = true)
{
    if (index < 0) return;
    FocusTarget target = _targets[index];
    if (scrollList && target.ItemIndex >= 0)
    {
        SDL_Rect view;
        get_list_view(target.Window, target.WidgetIndex, &view);
        int dx = 0, dy = 0;
        if (target.Left < view.x) dx = view.x - target.Left;
        else if (target.Right > view.x + view.w - 1) dx = view.x + view.w - 1 - target.Right;
        if (target.Top < view.y) dy = view.y - target.Top;
        else if (target.Bottom > view.y + view.h - 1) dy = view.y + view.h - 1 - target.Bottom;
        if (dx != 0 || dy != 0)
        {
            scroll_widget_by(target.Window, target.WidgetIndex, dx, dy);
            collect_targets(windows, numWindows);   // same order, new positions
            target = _targets[index];
        }
    }

    SDL_Rect rect;
    if (!get_visible_rect(&target, &rect)) return;
    _focusActive = true;
    _pointerIsTouch = false;
    _focusRect = rect;
    _focusOwnHighlight = target_has_own_highlight(&target);
    _focusLastX = rect.x + rect.w / 2;
    _focusLastY = rect.y + rect.h / 2;
    move_mouse(_focusLastX, _focusLastY);

    // Bring it onto the bottom screen if the window is larger than the page
    if (rect.x < _cameraX) _cameraX = rect.x;
    else if (rect.x + rect.w > _cameraX + N3DS_BOTTOM_WIDTH) _cameraX = rect.x + rect.w - N3DS_BOTTOM_WIDTH;
    if (rect.y < _cameraY) _cameraY = rect.y;
    else if (rect.y + rect.h > _cameraY + N3DS_BOTTOM_HEIGHT) _cameraY = rect.y + rect.h - N3DS_BOTTOM_HEIGHT;
    clamp_camera();
}

// always: the focus shows without a D-pad press (while browsing, when the buttons have nothing
// else to work on)
static void update_focus(u32 keys, u32 pressed, bool touchActive, bool always)
{
    // The D-pad key to act on: a new press, or a repeat while it is held
    static u32 repeatKey = 0;
    static u64 repeatTime = 0;
    u64 now = osGetTime();
    u32 key = 0;
    static const u32 dpadKeys[] = { KEY_DUP, KEY_DDOWN, KEY_DLEFT, KEY_DRIGHT };
    for (u32 dpadKey : dpadKeys)
    {
        if (pressed & dpadKey)
        {
            key = dpadKey;
            repeatKey = dpadKey;
            repeatTime = now + DpadRepeatDelayMs;
        }
    }
    if (key == 0 && repeatKey != 0)
    {
        if (!(keys & repeatKey))
        {
            repeatKey = 0;
        }
        else if (now >= repeatTime)
        {
            key = repeatKey;
            repeatTime = now + DpadRepeatMs;
        }
    }

    rct_window * windows[2];
    int numWindows = get_focus_windows(windows);
    if (numWindows == 0)
    {
        _focusActive = false;
        return;
    }
    if (touchActive || (!_focusActive && key == 0 && !always)) return;

    collect_targets(windows, numWindows);
    _focusRect.w = 0;
    if (_targetCount == 0) return;

    WindowId window = { windows[0]->classification, windows[0]->number };
    if (!_focusActive)
    {
        // Show the focus: on the control under the mouse (after a tap), or the one it was on
        // before the circle pad took the mouse to the top screen
        int index = find_target_at(_mouseX, _mouseY);
        if (index < 0 && window.Class == _focusWindow.Class && window.Number == _focusWindow.Number)
        {
            index = find_target_at(_focusLastX, _focusLastY);
        }
        _focusWindow = window;
        if (key != 0)
        {
            // First press: otherwise on the default
            focus_on(windows, numWindows, index >= 0 ? index : find_default_target());
        }
        else
        {
            // Without a press: otherwise on the nearest control that shows, and without
            // scrolling a list for it (the player may just have swiped it somewhere)
            focus_on(windows, numWindows, index >= 0 ? index : find_nearest_visible_target(_mouseX, _mouseY), false);
        }
        return;
    }

    // While A is held the focus stays put, so that nothing is dragged to another control
    bool canMove = !(keys & KEY_A);
    if (window.Class != _focusWindow.Class || window.Number != _focusWindow.Number)
    {
        // Another window came to the front (A opened one, B closed one): start in it
        if (!canMove) return;
        int index = -1;
        if (window.Class == WC_DROPDOWN)
        {
            _focusReturnWindow = _focusWindow;
            _focusReturnX = _focusLastX;
            _focusReturnY = _focusLastY;
        }
        else if (_focusWindow.Class == WC_DROPDOWN &&
            window.Class == _focusReturnWindow.Class && window.Number == _focusReturnWindow.Number)
        {
            // A dropdown closed, with or without a choice: back on the control that opened it,
            // not on the window's first one (the first tab; user's report)
            index = find_target_at(_focusReturnX, _focusReturnY);
            if (index < 0) index = find_nearest_target(_focusReturnX, _focusReturnY);
        }
        _focusWindow = window;
        focus_on(windows, numWindows, index >= 0 ? index : find_default_target());
        return;
    }

    int current = find_target_at(_mouseX, _mouseY);
    if (current < 0)
    {
        // The window changed under the focus (e.g. the list of another tab)
        if (canMove) focus_on(windows, numWindows, find_nearest_target(_mouseX, _mouseY));
        return;
    }
    if (key != 0 && canMove)
    {
        int next = find_target_towards(current, key);
        if (next >= 0)
        {
            focus_on(windows, numWindows, next);
            return;
        }
    }
    get_visible_rect(&_targets[current], &_focusRect);
    _focusOwnHighlight = target_has_own_highlight(&_targets[current]);
}

// B with a dropdown open: closes it without choosing, as letting the mouse button go beside it
// does (input_state_widget_pressed). False if there is none.
static bool cancel_dropdown()
{
    if (window_find_by_class(WC_DROPDOWN) == nullptr) return false;
    window_dropdown_close();
    if (gInputFlags & INPUT_FLAG_WIDGET_PRESSED)
    {
        gInputFlags &= ~INPUT_FLAG_WIDGET_PRESSED;
        widget_invalidate_by_number(gPressedWidget.window_classification, gPressedWidget.window_number, gPressedWidget.widget_index);
    }
    gInputState = INPUT_STATE_NORMAL;
    return true;
}

// B while the focus is on the bottom screen: back. Presses the window's close button, or its
// button that returns to the previous window (window_n3ds_get_back_widget).
static void close_front_sheet()
{
    rct_window * sheet = front_sheet();
    if (sheet == nullptr) return;
    int widgetIndex = window_n3ds_get_back_widget(sheet);
    if (widgetIndex >= 0) window_event_mouse_up_call(sheet, widgetIndex);
}

bool platform_n3ds_pointer_is_touch()
{
    return _pointerIsTouch;
}

#pragma endregion

#pragma region Zoom

static void set_park_scaled(bool scaled)
{
    if (_parkScaled == scaled) return;
    _parkScaled = scaled;
    window_n3ds_resize_main_window();
}

// direction: -1 zoom in, +1 zoom out. Steps: game zoom z at scale 1, then z at 0.75, then z+1 ...
static void zoom_view(int direction)
{
    rct_window * w = window_get_main();
    if (w == nullptr || w->viewport == nullptr) return;
    int zoom = w->viewport->zoom;
    if (direction < 0)
    {
        if (_parkScaled)
        {
            set_park_scaled(false);
        }
        else if (zoom > 0)
        {
            window_zoom_set(w, zoom - 1);
            set_park_scaled(true);
        }
    }
    else
    {
        if (!_parkScaled)
        {
            set_park_scaled(true);
        }
        else if (zoom < 3)
        {
            window_zoom_set(w, zoom + 1);
            set_park_scaled(false);
        }
    }
    // Its zoom buttons show whether there is a step further
    window_invalidate_by_class(WC_TOP_TOOLBAR);
}

// The zoom buttons of the toolbar take the same steps as L and R (top_toolbar.c)
void platform_n3ds_zoom(int direction)
{
    zoom_view(direction);
}

bool platform_n3ds_can_zoom(int direction)
{
    rct_window * w = window_get_main();
    if (w == nullptr || w->viewport == nullptr) return false;
    return direction < 0 ? (_parkScaled || w->viewport->zoom > 0) : (!_parkScaled || w->viewport->zoom < 3);
}

// Default view when a park is loaded: the game's closest zoom shown at 0.75
static void set_default_zoom()
{
    rct_window * w = window_get_main();
    if (w == nullptr || w->viewport == nullptr) return;
    window_zoom_set(w, 0);
    _parkScaled = false;
    set_park_scaled(true);
}

#pragma endregion

static void clear_screens();

void platform_n3ds_input_init(SDL_Window * topWindow)
{
    _topWindow = topWindow;

    // Touch is handled here. Stop SDL from turning it into mouse input of its own and the game
    // from handling raw finger events, otherwise a tap would be seen more than once.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_EventState(SDL_FINGERDOWN, SDL_IGNORE);
    SDL_EventState(SDL_FINGERUP, SDL_IGNORE);
    SDL_EventState(SDL_FINGERMOTION, SDL_IGNORE);

    // The cursor would scroll the view only at the bottom and left edges of the park area
    // (the virtual screen is bigger than it). The D-pad scrolls instead.
    gConfigGeneral.edge_scrolling = 0;

    // Zoom around the centre of the top screen, not the cursor: after a tap the cursor sits in
    // the UI area (bottom screen) and zooming "to" it moved the view somewhere unexpected.
    // window_zoom_set and the 0.75 display scale step both keep the view centre fixed.
    gConfigGeneral.zoom_to_cursor = 0;

    // No autosave. Saving takes about 14 MB of temporary memory (a copy of the park, the file
    // buffer, a buffer per compressed chunk) and the original code does not check that it got it:
    // with 71.6 of 96 MB in use an autosave wrote through a null pointer (ignored by the
    // emulator, a crash on the 3DS). The player saves when they choose, as in other 3DS games.
    gConfigGeneral.autosave_frequency = AUTOSAVE_NEVER;

    // The publisher, developer and game logos are shown when the game starts (off by default in
    // OpenRCT2; the 3DS build has no options window to turn them on). Any button or a tap skips.
    gConfigGeneral.play_intro = 1;

    // See "Input between frames". It must run before the main thread to interrupt it.
    s32 priority = 0x30;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    _samplerThread = threadCreate(input_sampler, nullptr, 4 * 1024, priority - 1, -2, false);
    if (_samplerThread == nullptr)
    {
        log_warning("n3ds: could not start the input sampler");
    }

    _bottomWindow = SDL_CreateWindow("OpenRCT2 (bottom)",
        SDL_WINDOWPOS_UNDEFINED_DISPLAY(1), SDL_WINDOWPOS_UNDEFINED_DISPLAY(1),
        N3DS_BOTTOM_WIDTH, N3DS_BOTTOM_HEIGHT, SDL_WINDOW_FULLSCREEN);
    if (_bottomWindow == nullptr)
    {
        log_warning("n3ds: could not create the bottom screen window: %s", SDL_GetError());
    }

    // Creating a window moves keyboard focus to it. The game treats every window event as its
    // own (a size change resizes the game, losing focus mutes the audio), so give the focus back
    // and drop the events caused by the bottom window.
    SDL_SetKeyboardFocus(_topWindow);
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_WINDOWEVENT);

    // The first frame comes seconds after this (the title's park is loaded first)
    clear_screens();
}

// Called before SDL shuts down. The sampler must be gone by then: SDL's video driver closes the
// HID service, which takes the shared memory away, and a sampler still running read it and
// crashed the game on its way out (crash dump 11: data abort in input_sampler, FAR 10000010).
void platform_n3ds_input_free()
{
    if (_samplerThread != nullptr)
    {
        _samplerStop = true;
        threadJoin(_samplerThread, U64_MAX);
        threadFree(_samplerThread);
        _samplerThread = nullptr;
    }
}

void platform_n3ds_input_update()
{
    if (_topWindow == nullptr) return;

    // Scan the buttons, the circle pad and the touch screen now. SDL scans them as well, but in
    // its event pump, which runs after this function (platform_process_messages): its state is
    // a frame old here. A frame can take longer than the D-pad's repeat delay (the track design
    // list drawing the preview of a large design: 400 ms), and a key let go during it would
    // still count as held and repeat.
    hidScanInput();

    // What is held now, and what went down since the last frame even if it is up again
    u32 keys = hidKeysHeld() | __atomic_exchange_n(&_latchedKeys, 0, __ATOMIC_RELAXED);
    _ignoreKeys &= hidKeysHeld();
    keys &= ~_ignoreKeys;
    u32 pressed = keys & ~_prevKeys;

    // Buttons that stand for keyboard keys. The D-pad is the arrow keys (scroll the view) only
    // while there is nothing on the bottom screen for it to move through.
    rct_window * focusWindows[2];
    u32 keyboardKeys = keys;
    if (get_focus_windows(focusWindows) > 0) keyboardKeys &= ~DpadKeys;
    for (const auto & map : KeyMap)
    {
        if ((keyboardKeys ^ _keysSent) & map.Key)
        {
            SDL_SendKeyboardKey((keyboardKeys & map.Key) ? SDL_PRESSED : SDL_RELEASED, map.Scancode);
            _keysSent ^= map.Key;
        }
    }

    // L / R: zoom out / in, in half steps
    if (pressed & KEY_L) zoom_view(+1);
    if (pressed & KEY_R) zoom_view(-1);

    // START + SELECT quits
    const u32 quitCombo = KEY_START | KEY_SELECT;
    if ((keys & quitCombo) == quitCombo && (_prevKeys & quitCombo) != quitCombo)
    {
        SDL_Event quit = {};
        quit.type = SDL_QUIT;
        SDL_PushEvent(&quit);
    }

    update_windows();

    // Heap use every 30 seconds, to watch memory while playing
    static u64 lastMemoryLog = 0;
    u64 now = osGetTime();
    if (now - lastMemoryLog >= 30000)
    {
        lastMemoryLog = now;
        platform_n3ds_log_memory("periodic");
    }

    // Release the button of last frame's tap before anything else moves the mouse
    if (_tapRelease)
    {
        set_mouse_button(false, SDL_BUTTON_LEFT, &_leftDown);
        _tapRelease = false;
    }

    update_touch(keys);

    // While the finger is on the bottom screen (or a tap is being released) touch owns the mouse.
    bool touchActive = _touchMode != TOUCH_NONE || _tapRelease;
    bool touchOwnsLeft = _touchMode == TOUCH_HOLD || _tapRelease;

    // Browsing: the circle pad moves the view. Otherwise it moves the top screen cursor.
    bool wasBrowsing = _browsing;
    _browsing = is_browsing();
    if (wasBrowsing && !_browsing) _cursorPending = true;
    int stepX, stepY;
    bool padMoved = read_circle_pad(&stepX, &stepY);
    if (padMoved)
    {
        if (_browsing)
        {
            scroll_view_by(stepX, stepY);
            padMoved = false;
        }
        else
        {
            move_top_cursor(stepX, stepY);
        }
    }

    // A let go: the button goes up where the mouse is now, before the focus takes the mouse
    // elsewhere. The focus stays put while A is held and moves on when it is let go, for one
    // into the dropdown that the press opened: with the button going up only after that (at
    // the end, below) the game saw it let go on the dropdown's first item and chose that.
    if (!touchOwnsLeft && !(keys & KEY_A))
    {
        set_mouse_button(false, SDL_BUTTON_LEFT, &_leftDown);
    }

    // D-pad: the focus on the bottom screen. Moving the cursor goes back to the top screen.
    if (padMoved) _focusActive = false;
    update_focus(keys, pressed, touchActive, _browsing);
    if (_focusActive && (pressed & KEY_A))
    {
        int index = find_target_at(_mouseX, _mouseY);
        if (index >= 0) note_click_on_widget(_targets[index].Window, _targets[index].WidgetIndex);
    }

    // Browsing is over: show the cursor again, once the click that ended it is done
    if (_cursorPending && !touchActive && !(keys & KEY_A))
    {
        _cursorPending = false;
        _focusActive = false;
        int x, y;
        top_to_virtual(_topCursorX, _topCursorY, &x, &y);
        _pointerIsTouch = false;
        move_mouse(x, y);
    }

    if (!(keys & KEY_B)) _backHeld = false;
    if ((pressed & KEY_B) && cancel_dropdown())
    {
        // B closed a dropdown: nothing else for this press
        _backHeld = true;
    }
    else if (_focusActive)
    {
        // The mouse rests on the focused control: A clicks it, B is "back"
        if (pressed & KEY_B)
        {
            close_front_sheet();
            _backHeld = true;
        }
    }
    else if (!touchActive && !_browsing && (padMoved || (pressed & (KEY_A | KEY_B))))
    {
        // Circle pad and A/B work on the top screen: put the mouse back on the park cursor first
        int x, y;
        top_to_virtual(_topCursorX, _topCursorY, &x, &y);
        _pointerIsTouch = false;
        move_mouse(x, y);
    }
    if (!touchOwnsLeft)
    {
        set_mouse_button(!touchActive && (keys & KEY_A) != 0, SDL_BUTTON_LEFT, &_leftDown);
    }
    set_mouse_button(!_focusActive && !_backHeld && (keys & KEY_B) != 0, SDL_BUTTON_RIGHT, &_rightDown);

    _prevKeys = keys;
}

// The system's software keyboard, which takes over both screens until the player is done. It
// starts with the text in the buffer; hint is shown while there is no text. True if the player
// confirmed: the buffer then holds what was entered. (SDL's text input uses this keyboard too,
// but starts it empty and sends what was typed as key presses, which the game adds to the text
// it has: a name could be added to, not changed.)
bool platform_n3ds_software_keyboard(utf8 * buffer, size_t size, const utf8 * hint)
{
    char text[512];
    if (size > sizeof(text)) size = sizeof(text);

    SwkbdState keyboard;
    swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, (int)size - 1);
    swkbdSetInitialText(&keyboard, buffer);
    if (hint != nullptr && hint[0] != '\0') swkbdSetHintText(&keyboard, hint);
    bool confirmed = swkbdInputText(&keyboard, text, size) == SWKBD_BUTTON_CONFIRM;
    if (confirmed) snprintf(buffer, size, "%s", text);

    // What was touched and pressed on the keyboard is not for the game. The sampler thread
    // noted it all the same: the last touch came through as a tap at that place on the bottom
    // screen, on whatever the game had there (a file in the list behind: its overwrite prompt).
    // And a button still held now is not a press either.
    hidScanInput();
    __atomic_store_n(&_latchedKeys, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&_latchedTouch, 0, __ATOMIC_RELAXED);
    _ignoreKeys = hidKeysHeld();
    return confirmed;
}

#pragma region Drawing

// The two screens are drawn directly, from the game's 8-bit virtual screen into the framebuffers.
// Going through SDL (a 32-bit surface per window, which SDL_UpdateWindowSurface copies into the
// sideways framebuffer one pixel at a time, a cache line apart) took 22 ms of every frame.

// The framebuffer of a 3DS screen that the next picture is drawn into. The LCD panels are mounted
// sideways: the buffer holds the picture as columns, from the left, each from the bottom up.
// A pixel is 0xRRGGBBAA (GSP_RGBA8_OES, which SDL's video driver sets for both screens).
struct Frame
{
    gfxScreen_t Screen;
    u32 *       Pixels;
    int         Width;      // of the picture: 400 (top) or 320 (bottom)
    int         Height;     // 240
};

static Frame begin_frame(gfxScreen_t screen)
{
    // A screen has two buffers. The one to draw into now is the one still on show: the picture
    // handed over last takes its place only at the screen's next refresh (60 a second), and
    // what is drawn into it before that is seen. Frames are 25 ms apart, so normally the
    // refresh has long passed; but the frame after a slow one follows at once, and when
    // nothing takes time to draw (zoomed out three times) it gets here within that refresh.
    // The park, drawn where the status panels then go over it, showed as a stripe flickering
    // in the right panel. So wait for the refresh. (Not for ever: in case the picture is never
    // taken, as when the screen is not the game's.)
    for (int i = 0; i < 80 && gspIsPresentPending(screen); i++)
    {
        svcSleepThread(500 * 1000);
    }

    u16 bufferWidth, bufferHeight;   // of the sideways buffer: 240 x 400 or 240 x 320
    u8 * pixels = gfxGetFramebuffer(screen, GFX_LEFT, &bufferWidth, &bufferHeight);
    return { screen, (u32 *)pixels, bufferHeight, bufferWidth };
}

// Shows the frame, the way SDL does after its copy (SDL_n3dsframebuffer.c)
static void end_frame(const Frame & frame)
{
    GSPGPU_FlushDataCache(frame.Pixels, frame.Width * frame.Height * sizeof(u32));
    gfxScreenSwapBuffers(frame.Screen, false);
}

static inline u32 frame_colour(u32 red, u32 green, u32 blue)
{
    return (red << 24) | (green << 16) | (blue << 8) | 0xFF;
}

static inline void put_pixel(const Frame & frame, int x, int y, u32 colour)
{
    if (x < 0 || y < 0 || x >= frame.Width || y >= frame.Height) return;
    frame.Pixels[x * frame.Height + frame.Height - 1 - y] = colour;
}

// Makes both screens black. The screens' buffers are not cleared when the screens are opened:
// they show what the memory held until the game's first frame (noise on both screens, when
// something had used that memory before: user's report).
static void clear_screens()
{
    const gfxScreen_t screens[] = { GFX_TOP, GFX_BOTTOM };
    for (int buffer = 0; buffer < 2; buffer++)     // a screen has two, shown in turn
    {
        for (gfxScreen_t screen : screens)
        {
            Frame frame = begin_frame(screen);
            const u32 black = frame_colour(0, 0, 0);
            for (int i = 0; i < frame.Width * frame.Height; i++)
            {
                frame.Pixels[i] = black;
            }
            end_frame(frame);
        }
    }
}

// The game's virtual screen and its colours as framebuffer pixels, noted by platform_n3ds_present
static const uint8 * _screenBits = nullptr;
static int           _screenPitch = 0;
static u32           _screenColours[256];

// Fills a screen from the virtual screen: pixel (x, y) of the screen is the one at sourceX[x]
// in the row that starts at sourceRow[y].
// The framebuffer is written in its own order, up one column after another. The virtual screen
// is then read down its columns, a different cache line for every pixel, so this goes in tiles
// small enough for the rows read to stay in the CPU's data cache from one column to the next.
static void copy_screen(const Frame & frame, const int * sourceX, const int * sourceRow)
{
    const int TileWidth = 16, TileHeight = 48;
    for (int tileTop = 0; tileTop < frame.Height; tileTop += TileHeight)
    {
        int tileBottom = clamp_value(tileTop + TileHeight, 0, frame.Height);
        for (int tileLeft = 0; tileLeft < frame.Width; tileLeft += TileWidth)
        {
            int tileRight = clamp_value(tileLeft + TileWidth, 0, frame.Width);
            for (int x = tileLeft; x < tileRight; x++)
            {
                const uint8 * column = _screenBits + sourceX[x];
                u32 * dst = frame.Pixels + x * frame.Height + frame.Height - tileBottom;
                for (int y = tileBottom - 1; y >= tileTop; y--)
                {
                    *dst++ = _screenColours[column[sourceRow[y]]];
                }
            }
        }
    }
}

// The park area on the top screen, shrunk to 400x240 (nearest pixel; at display scale 0.75
// every fourth row and column is dropped).
static void draw_park(const Frame & top)
{
    int width, height;
    platform_n3ds_get_park_size(&width, &height);
    static int sourceX[N3DS_TOP_WIDTH], sourceRow[N3DS_TOP_HEIGHT];
    for (int x = 0; x < N3DS_TOP_WIDTH; x++)
    {
        sourceX[x] = N3DS_PARK_X + x * width / N3DS_TOP_WIDTH;
    }
    for (int y = 0; y < N3DS_TOP_HEIGHT; y++)
    {
        sourceRow[y] = (N3DS_PARK_Y + y * height / N3DS_TOP_HEIGHT) * _screenPitch;
    }
    copy_screen(top, sourceX, sourceRow);
}

// The page of the UI area that the bottom screen shows
static void draw_page(const Frame & bottom)
{
    static int sourceX[N3DS_BOTTOM_WIDTH], sourceRow[N3DS_BOTTOM_HEIGHT];
    for (int x = 0; x < N3DS_BOTTOM_WIDTH; x++)
    {
        sourceX[x] = _cameraX + x;
    }
    for (int y = 0; y < N3DS_BOTTOM_HEIGHT; y++)
    {
        sourceRow[y] = (_cameraY + y) * _screenPitch;
    }
    copy_screen(bottom, sourceX, sourceRow);
}

// Shows the status area over the bottom of the top screen: what the widgets of the status bar's
// window cover of it (the panels), pixel for pixel. The park shows around them.
static void draw_status(const Frame & top)
{
    rct_window * w = window_find_by_class(WC_BOTTOM_TOOLBAR);
    if (w == nullptr) return;

    for (rct_widget * widget = w->widgets; widget->type != WWT_LAST; widget++)
    {
        if (widget->type == WWT_EMPTY) continue;
        // The widget's rectangle, cut to the status area
        int left = clamp_value(w->x + widget->left, N3DS_STATUS_X, N3DS_STATUS_X + N3DS_STATUS_WIDTH);
        int right = clamp_value(w->x + widget->right + 1, N3DS_STATUS_X, N3DS_STATUS_X + N3DS_STATUS_WIDTH);
        int upper = clamp_value(w->y + widget->top, N3DS_STATUS_Y, N3DS_STATUS_Y + N3DS_STATUS_HEIGHT);
        int lower = clamp_value(w->y + widget->bottom + 1, N3DS_STATUS_Y, N3DS_STATUS_Y + N3DS_STATUS_HEIGHT);
        for (int x = left; x < right; x++)
        {
            for (int y = upper; y < lower; y++)
            {
                put_pixel(top, x - N3DS_STATUS_X, y - N3DS_STATUS_Y + StatusTopY,
                    _screenColours[_screenBits[y * _screenPitch + x]]);
            }
        }
    }
}

// The version text of the title screen, at the bottom left of the top screen. The game draws
// it into the main view (rct2_draw): there it would be shrunk with the park at display scale
// 0.75, every fourth row and column of the letters dropped, and could not be read. So it is
// drawn here: into a scratch image, and from there onto the screen pixel for pixel.
static void draw_version(const Frame & top)
{
    if (!(gScreenFlags & SCREEN_FLAGS_TITLE_DEMO) || gTitleHideVersionInfo) return;

    // Two lines of text, 28 and 15 pixels above the bottom of the screen as in the original
    const int width = 384, height = 33;
    rct_drawpixelinfo dpi = n3ds_scratch_begin_at(0, 0, width, height);
    DrawOpenRCT2(&dpi, 0, 13);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint8 colour = dpi.bits[y * width + x];
            if (colour != 0) put_pixel(top, x, N3DS_TOP_HEIGHT - height + y, _screenColours[colour]);
        }
    }
}

// Fallback arrow for the cursors the game takes from the operating system (arrow, hand).
// 'X' black, '.' white, ' ' transparent. Hot spot is the top-left pixel.
static const char * const ArrowCursor[] =
{
    "X",
    "XX",
    "X.X",
    "X..X",
    "X...X",
    "X....X",
    "X.....X",
    "X......X",
    "X.......X",
    "X........X",
    "X.....XXXXX",
    "X..X..X",
    "X.X X..X",
    "XX  X..X",
    "X    X..X",
    "     X..X",
    "      XX",
};

// Draws the game's current cursor at (x, y) of the top screen.
static void draw_cursor(const Frame & top, int x, int y, CURSOR_ID cursor)
{
    if (SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE) return;

    const u32 black = frame_colour(0, 0, 0);
    const u32 white = frame_colour(255, 255, 255);

    const Cursors::CursorData * data = Cursors::GetCursorData(cursor);
    if (data == nullptr)
    {
        for (int row = 0; row < (int)(sizeof(ArrowCursor) / sizeof(ArrowCursor[0])); row++)
        {
            for (int col = 0; ArrowCursor[row][col] != '\0'; col++)
            {
                char c = ArrowCursor[row][col];
                if (c == 'X') put_pixel(top, x + col, y + row, black);
                else if (c == '.') put_pixel(top, x + col, y + row, white);
            }
        }
        return;
    }

    // Same bitmap format as SDL_CreateCursor: 32x32, 1 bit per pixel, most significant bit first.
    // data 1 + mask 1 = black, data 0 + mask 1 = white, data 1 + mask 0 = inverted (drawn black).
    int originX = x - data->HotSpot.X;
    int originY = y - data->HotSpot.Y;
    for (int row = 0; row < 32; row++)
    {
        for (int col = 0; col < 32; col++)
        {
            int byte = row * 4 + col / 8;
            int bit = 0x80 >> (col % 8);
            bool d = (data->Data[byte] & bit) != 0;
            bool m = (data->Mask[byte] & bit) != 0;
            if (m) put_pixel(top, originX + col, originY + row, d ? black : white);
            else if (d) put_pixel(top, originX + col, originY + row, black);
        }
    }
}

// Frames the control that has the D-pad focus on the bottom screen: a white frame just outside
// it and a black one around that, so it shows on any background.
static void draw_focus(const Frame & bottom)
{
    if (!_focusActive || _focusRect.w == 0 || _focusOwnHighlight) return;
    const u32 colours[] = { frame_colour(255, 255, 255), frame_colour(0, 0, 0) };
    for (int ring = 0; ring < 2; ring++)
    {
        int left = _focusRect.x - _cameraX - 1 - ring;
        int top = _focusRect.y - _cameraY - 1 - ring;
        int right = _focusRect.x - _cameraX + _focusRect.w + ring;
        int bottomEdge = _focusRect.y - _cameraY + _focusRect.h + ring;
        for (int x = left; x <= right; x++)
        {
            put_pixel(bottom, x, top, colours[ring]);
            put_pixel(bottom, x, bottomEdge, colours[ring]);
        }
        for (int y = top; y <= bottomEdge; y++)
        {
            put_pixel(bottom, left, y, colours[ring]);
            put_pixel(bottom, right, y, colours[ring]);
        }
    }
}

#pragma region Loading progress

// While a park loads nothing is drawn, for up to minutes on a 3DS. The code that starts a load
// calls platform_n3ds_loading_begin and the object loader reports how far it is; the bottom
// screen then shows what it showed last (the virtual screen still holds it) with a box and a
// bar over it.
static bool _loading = false;
static bool _loadingIsSave = false;   // the box is up for a save: "Saving...", and no bar
static u64  _loadingStart = 0;
static u64  _loadingLastDraw = 0;

static void draw_loading(int done, int total)
{
    if (_screenBits == nullptr) return;

    // Drawn with the game's functions into a scratch image: a window-like panel, "Loading..."
    // (STR_3054, which has no name in string_ids.h) and the bar
    const int width = 200, height = 46;
    const int barLeft = 12, barRight = width - 13, barTop = 26, barBottom = 35;
    uint8 colour = theme_get_colour(WC_CONSTRUCT_RIDE, 0) & 0x7F;
    rct_drawpixelinfo dpi = n3ds_scratch_begin_at(0, 0, width, height);
    gfx_fill_rect_inset(&dpi, 0, 0, width - 1, height - 1, colour, 0);
    if (_loadingIsSave)
    {
        // The language files have no string for this: it is here, in English and in Korean (the
        // one other language with a text of its own in the 3DS code). A save does not tell how
        // far it is.
        const utf8 * text = gCurrentLanguage == LANGUAGE_KOREAN ? "저장하는 중..." : "Saving...";
        gfx_draw_string_centred(&dpi, STR_STRING, width / 2, (height - 10) / 2, COLOUR_WHITE, &text);
    }
    else
    {
        gfx_draw_string_centred(&dpi, 3054, width / 2, 9, COLOUR_WHITE, nullptr);
        gfx_fill_rect_inset(&dpi, barLeft - 2, barTop - 2, barRight + 2, barBottom + 2, colour, INSET_RECT_F_60);
        if (done > 0)
        {
            int filled = (barRight - barLeft) * done / total;
            gfx_fill_rect(&dpi, barLeft, barTop, barLeft + filled, barBottom, ColourMapA[COLOUR_BRIGHT_GREEN].mid_light);
        }
    }

    Frame bottom = begin_frame(GFX_BOTTOM);
    draw_page(bottom);
    int originX = (N3DS_BOTTOM_WIDTH - width) / 2;
    int originY = (N3DS_BOTTOM_HEIGHT - height) / 2;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            put_pixel(bottom, originX + x, originY + y, _screenColours[dpi.bits[y * width + x]]);
        }
    }
    end_frame(bottom);
}

static void show_loading_box(bool isSave)
{
    _loading = true;
    _loadingIsSave = isSave;
    _loadingStart = _loadingLastDraw = osGetTime();
    draw_loading(0, 1);
}

void platform_n3ds_loading_begin()
{
    show_loading_box(false);
}

// The same box for a save, which takes seconds too with nothing drawn (user's report: no
// reaction to "Save game")
void platform_n3ds_saving_begin()
{
    show_loading_box(true);
}

void platform_n3ds_loading_progress(int done, int total)
{
    if (!_loading || total <= 0) return;
    u64 now = osGetTime();
    if (done < total && now - _loadingLastDraw < 100) return;
    _loadingLastDraw = now;
    draw_loading(done, total);
}

#pragma endregion

// The intro (intro.c) draws pictures laid out for a 640x480 screen into the UI area. They are
// shown on the top screen at half size, 320x240 in the middle, each pixel the average of 2x2.
// The bottom screen stays black.
static void present_intro()
{
    Frame top = begin_frame(GFX_TOP);
    const int left = (N3DS_TOP_WIDTH - N3DS_UI_WIDTH / 2) / 2;
    // Beside the picture: the colour of its top-left corner (the background of the logos)
    const u32 border = _screenColours[_screenBits[0]];
    for (int x = 0; x < N3DS_TOP_WIDTH; x++)
    {
        bool inPicture = x >= left && x < left + N3DS_UI_WIDTH / 2;
        for (int y = 0; y < N3DS_TOP_HEIGHT; y++)
        {
            u32 colour = border;
            if (inPicture)
            {
                const uint8 * src = _screenBits + 2 * y * _screenPitch + 2 * (x - left);
                const u32 a = _screenColours[src[0]];
                const u32 b = _screenColours[src[1]];
                const u32 c = _screenColours[src[_screenPitch]];
                const u32 d = _screenColours[src[_screenPitch + 1]];
                // Two of the four 8-bit channels at a time, with room for their sums in between
                const u32 mask = 0x00FF00FF;
                u32 even = ((a & mask) + (b & mask) + (c & mask) + (d & mask)) / 4;
                u32 odd = (((a >> 8) & mask) + ((b >> 8) & mask) + ((c >> 8) & mask) + ((d >> 8) & mask)) / 4;
                colour = ((odd & mask) << 8) | (even & mask);
            }
            put_pixel(top, x, y, colour);
        }
    }
    end_frame(top);

    Frame bottom = begin_frame(GFX_BOTTOM);
    const u32 black = frame_colour(0, 0, 0);
    for (int i = 0; i < bottom.Width * bottom.Height; i++)
    {
        bottom.Pixels[i] = black;
    }
    end_frame(bottom);
}

// Called by the software drawing engine once the virtual screen (bits, 8 bits per pixel) is
// drawn: shows the park area on the top screen and the visible page of the UI area on the
// bottom screen.
void platform_n3ds_present(const uint8 * bits, int pitch, const SDL_Palette * palette)
{
    if (_topWindow == nullptr) return;

    _screenBits = bits;
    _screenPitch = pitch;
    for (int i = 0; i < 256; i++)
    {
        const SDL_Color colour = palette->colors[i];
        _screenColours[i] = frame_colour(colour.r, colour.g, colour.b);
    }

    // The first frame after a load ends the loading box. (Also a frame of the intro: the title
    // is loaded before the intro plays over it.)
    if (_loading)
    {
        _loading = false;
        log_warning("n3ds %s: %u ms until the first frame", _loadingIsSave ? "saving" : "loading",
            (unsigned int)(osGetTime() - _loadingStart));
    }

    if (gIntroState != INTRO_STATE_NONE)
    {
        present_intro();
        return;
    }

    Frame top = begin_frame(GFX_TOP);
    draw_park(top);
    draw_status(top);
    draw_version(top);
    if (!_browsing && (in_park(_mouseX, _mouseY) || in_status(_mouseX, _mouseY)))
    {
        int topX, topY;
        virtual_to_top(_mouseX, _mouseY, &topX, &topY);
        draw_cursor(top, topX, topY, Cursors::GetCurrentCursor());
    }
    else if (tool_stays_on_top() && (gInputFlags & INPUT_FLAG_TOOL_ACTIVE))
    {
        // The mouse is on the bottom screen: the tool's cursor where the tool still works
        draw_cursor(top, _topCursorX, _topCursorY, (CURSOR_ID)gCurrentToolId);
    }
    end_frame(top);

    Frame bottom = begin_frame(GFX_BOTTOM);
    draw_page(bottom);
    draw_focus(bottom);
    end_frame(bottom);
}

#pragma endregion

#endif // __3DS__
