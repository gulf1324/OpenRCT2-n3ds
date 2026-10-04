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
 * n3ds port: all the object files in one file on the SD card (user/objdata.pak), with an index.
 *
 * A scenario needs 220 to 540 of the 2122 object files (11 to 46 MB of them). On a 3DS what
 * that costs, besides reading the data (about 10 MB/s), is the number of requests to the SD
 * card: opening a file takes about 9 ms and every read about 5 ms, whatever its size and
 * wherever it is in a file. With a file for each object that is an opening and, through
 * stdio's buffer, a read for every 64 KB. The archive is opened once, and of the objects that
 * a park needs, those that lie close together in it are read with one request (Expect).
 *
 * The archive holds the object files byte for byte, in the order of the bytes of their entries.
 * It is made on the PC and put on the SD card with the game's data
 * (scripts/n3ds_object_archive.py, sd_sync.py); the game only reads it. The object files stay
 * where they are and remain what counts: an object that is not in the archive, or whose bytes
 * there do not begin with the entry that the object index has for it, is read from its own
 * file, as every object is when there is no archive.
 */
class N3dsObjectArchive final
{
public:
    // The archive on the SD card. Null if there is none or the file is not a whole archive.
    static N3dsObjectArchive * Open();

    ~N3dsObjectArchive();

    // Tells the archive which objects CreateObject is going to be asked for, in the order of
    // their entries' bytes: those of them that lie close together are then read with one
    // request. With no items: there are no more of them (the memory for it is freed).
    void Expect(const ObjectRepositoryItem * const * items, size_t count);

    // The object read from the archive. Null if it is not in it or cannot be read from it.
    Object * CreateObject(const ObjectRepositoryItem * ori);

private:
#pragma pack(push, 1)
    struct Header
    {
        uint32 Magic;
        uint32 Version;
        uint32 Count;
        uint32 DataSize;
    };

    struct Entry
    {
        rct_object_entry ObjectEntry;   // the first 16 bytes of the object's file
        uint32           Offset;        // of the file's bytes in the data, which follows the entries
        uint32           Size;
    };
#pragma pack(pop)

    // A stretch of the data that is read with one request
    struct Span
    {
        uint32 Offset;
        uint32 Size;

        bool Holds(const Entry * entry) const
        {
            return entry->Offset >= Offset && entry->Offset + entry->Size <= Offset + Size;
        }
    };

    N3dsObjectArchive() { }

    const Entry * FindEntry(const rct_object_entry * objectEntry) const;
    const uint8 * GetFromSpan(const Entry * entry);
    Object * CreateObjectFromFile(const Entry * entry, const ObjectRepositoryItem * ori);

    int                 _file = -1;
    std::vector<Entry>  _entries;           // in the order of the bytes of their object entries
    uint32              _dataOffset = 0;    // of the data in the file
    uint8 *             _head = nullptr;    // the first bytes of an object, see CreateObjectFromFile

    std::vector<Span>   _spans;             // what Expect has planned, in the order of the data
    uint8 *             _spanData = nullptr;    // room for the largest of them
    Span                _spanRead = { 0, 0 };   // the one that _spanData holds
};

#endif // __3DS__
