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

#pragma once

#include "common.h"
#include "core/Guard.hpp"

#ifdef __cplusplus
extern "C"
{
#endif
    #include "platform/platform.h"
#ifdef __cplusplus
}
#endif


enum STARTUP_ACTION
{
    STARTUP_ACTION_INTRO,
    STARTUP_ACTION_TITLE,
    STARTUP_ACTION_OPEN,
    STARTUP_ACTION_EDIT
};

#ifdef __cplusplus
extern "C"
{
#endif

    /** The exit code for OpenRCT2 when it exits. */
    extern int gExitCode;

    extern int gOpenRCT2StartupAction;
    extern utf8 gOpenRCT2StartupActionPath[512];
    extern utf8 gExePath[MAX_PATH];
    extern utf8 gCustomUserDataPath[MAX_PATH];
    extern utf8 gCustomOpenrctDataPath[MAX_PATH];
    extern utf8 gCustomRCT2DataPath[MAX_PATH];
    extern utf8 gCustomPassword[MAX_PATH];
    extern bool gOpenRCT2Headless;



    void openrct2_write_full_version_info(utf8 * buffer, size_t bufferSize);
    bool openrct2_initialise();
    void openrct2_launch();
    void openrct2_dispose();
    void openrct2_finish();

    bool openrct2_read_arguments(int argc, const char * * argv);

#ifdef __cplusplus
}
#endif
