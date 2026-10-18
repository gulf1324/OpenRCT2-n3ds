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

#ifndef _THEMES_H_
#define _THEMES_H_

#include "../common.h"
#include "window.h"

// n3ds port: the three USE_ flags are read from theme files but no longer looked at: the RCT1-style
// lights and font are settings of the options window (gConfigInterface.rct1_*)
enum {
    UITHEME_FLAG_PREDEFINED                            = 1 << 0,
    UITHEME_FLAG_USE_LIGHTS_RIDE                       = 1 << 1,
    UITHEME_FLAG_USE_LIGHTS_PARK                       = 1 << 2,
    UITHEME_FLAG_USE_ALTERNATIVE_SCENARIO_SELECT_FONT  = 1 << 3,
};

void colour_scheme_update(rct_window *window);
void colour_scheme_update_by_class(rct_window *window, rct_windowclass classification);

void         theme_manager_initialise();
size_t       theme_manager_get_num_available_themes();
const utf8 * theme_manager_get_available_theme_name(size_t index);
size_t       theme_manager_get_active_available_theme_index();
void         theme_manager_set_active_available_theme(size_t index);

colour_t theme_get_colour(rct_windowclass wc, uint8 index);

#endif
