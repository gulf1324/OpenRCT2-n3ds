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

#ifdef __3DS__

#include <vector>
#include "../common.h"

extern "C"
{
    #include "../object.h"
}

class Object;
struct ObjectRepositoryItem;

/**
 * n3ds port: the object files of the title's first park, one after the other in one file on
 * the SD card (user/title_objects.pak).
 *
 * On a 3DS opening a file takes about 9 ms whatever its size, and the park of the title
 * screen needs 162 object files (7.3 MB): 1.4 s to open them and 2.0 s to read them in small
 * requests, of the 4.1 s that the title takes to load at every start of the game. Read from
 * one file in one request they take about 0.7 s.
 *
 * The pack is a cache of those files, byte for byte, made by the game the first time. It is
 * used for the load of the title only (the start of the game, going back to the menu): a park
 * that is played is loaded from the object files as before. It is made again when it does not
 * fit the park's list of objects, and deleted when the object index is made again
 * (ObjectRepository.cpp Construct). An object that cannot be read from it is read from its file.
 */
class N3dsObjectPack final
{
public:
    // The pack on the SD card, if it holds the files of exactly the objects of 'required' (the
    // OBJECT_ENTRY_COUNT slots of a park, null where the park has no object). Otherwise null.
    static N3dsObjectPack * Open(const ObjectRepositoryItem * const * required);

    // Writes the pack with the files of the objects of 'required'
    static bool Write(const ObjectRepositoryItem * const * required);

    static void Delete();

    ~N3dsObjectPack();

    // Reads the files into memory, all at once. False if there is no memory for them or they
    // cannot be read: the pack is of no use then.
    bool ReadData();

    // The object read from its file in the pack. Null if it is not in the pack or cannot be
    // read from it.
    Object * CreateObject(const ObjectRepositoryItem * ori);

private:
#pragma pack(push, 1)
    struct Header
    {
        uint32 Magic;       // written last: a pack that was not written to its end has none
        uint32 Version;
        uint32 Key;         // of the park's list of objects, see GetKey
        uint32 Count;
        uint32 DataSize;
    };

    struct Entry
    {
        rct_object_entry ObjectEntry;
        uint32           Offset;    // in the data, which follows the entries
        uint32           Size;
    };
#pragma pack(pop)

    N3dsObjectPack() { }

    static uint32 GetKey(const ObjectRepositoryItem * const * required, uint32 * outCount);

    utf8                _path[MAX_PATH];
    std::vector<Entry>  _entries;
    uint32              _dataSize = 0;
    uint8 *             _data = nullptr;
};

#endif // __3DS__
