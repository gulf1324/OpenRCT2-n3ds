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

#include <cassert>
#include <stdarg.h>
#include <stdio.h>

#include "Console.hpp"
#include "Diagnostics.hpp"
#include "Guard.hpp"

extern "C"
{
    #include "../OpenRCT2.h"

    void openrct2_assert(bool expression, const char * message, ...)
    {
        va_list args;
        va_start(args, message);
        Guard::Assert_VA(expression, message, args);
        va_end(args);
    }

}

namespace Guard
{
    void Assert(bool expression, const char * message, ...)
    {
        va_list args;
        va_start(args, message);
        Assert_VA(expression, message, args);
        va_end(args);
    }

    void Assert_VA(bool expression, const char * message, va_list args)
    {
        if (expression) return;

        if (message != nullptr)
        {
            Console::Error::WriteLine("Assertion failed:");
            Console::Error::WriteLine_VA(message, args);
        }

#ifdef DEBUG
        Debug::Break();
#endif
        assert(false);
    }

    void Fail(const char * message, ...)
    {
        va_list args;
        va_start(args, message);
        Assert_VA(false, message, args);
        va_end(args);
    }

    void Fail_VA(const char * message, va_list args)
    {
        Assert_VA(false, message, args);
    }
}
