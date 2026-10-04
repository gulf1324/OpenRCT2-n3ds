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

#ifdef __3DS__

// n3ds port: see N3dsObjectPack.h

#include <fcntl.h>
#include <unistd.h>
#include "N3dsObjectPack.h"
#include "Object.h"
#include "ObjectFactory.h"
#include "ObjectRepository.h"

extern "C"
{
    #include "../platform/platform.h"
    #include "../util/util.h"
}

// The times of the object manager's log (ObjectFactory.cpp)
extern unsigned int gN3DSObjectOpenTicks;
extern unsigned int gN3DSObjectReadTicks;

constexpr uint32 PACK_MAGIC = 0x504F334E;   // "N3OP"
constexpr uint32 PACK_VERSION = 1;          // to be raised when the format changes

static void GetPackPath(utf8 * buffer, size_t bufferSize)
{
    platform_get_user_directory(buffer, NULL, bufferSize);
    safe_strcat_path(buffer, "title_objects.pak", bufferSize);
}

/**
 * A number for the list of objects: which objects, in which slots. A pack is used only for the
 * list it was written for. (An object is told from another by its name and checksum, which are
 * in its entry: the same list means the same files.)
 */
uint32 N3dsObjectPack::GetKey(const ObjectRepositoryItem * const * required, uint32 * outCount)
{
    uint32 hash = 2166136261u;
    uint32 count = 0;
    for (int i = 0; i < OBJECT_ENTRY_COUNT; i++)
    {
        if (required[i] == nullptr) continue;

        count++;
        const uint8 slot[2] = { (uint8)(i & 0xFF), (uint8)(i >> 8) };
        const uint8 * entry = (const uint8 *)&required[i]->ObjectEntry;
        for (size_t j = 0; j < sizeof(slot); j++)
        {
            hash = (hash ^ slot[j]) * 16777619u;
        }
        for (size_t j = 0; j < sizeof(rct_object_entry); j++)
        {
            hash = (hash ^ entry[j]) * 16777619u;
        }
    }
    *outCount = count;
    return hash;
}

N3dsObjectPack * N3dsObjectPack::Open(const ObjectRepositoryItem * const * required)
{
    utf8 path[MAX_PATH];
    GetPackPath(path, sizeof(path));
    SDL_RWops * file = SDL_RWFromFile(path, "rb");
    if (file == nullptr)
    {
        return nullptr;
    }

    N3dsObjectPack * pack = nullptr;
    uint32 count;
    uint32 key = GetKey(required, &count);
    Header header;
    if (count != 0 &&
        SDL_RWread(file, &header, sizeof(header), 1) == 1 &&
        header.Magic == PACK_MAGIC &&
        header.Version == PACK_VERSION &&
        header.Key == key &&
        header.Count == count &&
        SDL_RWsize(file) == (Sint64)(sizeof(Header) + (uint64)count * sizeof(Entry) + header.DataSize))
    {
        std::vector<Entry> entries(count);
        if (SDL_RWseek(file, sizeof(Header), RW_SEEK_SET) == (Sint64)sizeof(Header) &&
            SDL_RWread(file, entries.data(), sizeof(Entry), count) == count)
        {
            // The entries are those of the objects, in their order, and lie within the data
            bool valid = true;
            size_t next = 0;
            for (int i = 0; i < OBJECT_ENTRY_COUNT && valid; i++)
            {
                if (required[i] == nullptr) continue;

                const Entry &entry = entries[next++];
                valid = memcmp(&entry.ObjectEntry, &required[i]->ObjectEntry, sizeof(rct_object_entry)) == 0 &&
                        entry.Offset <= header.DataSize &&
                        entry.Size <= header.DataSize - entry.Offset;
            }
            if (valid)
            {
                pack = new N3dsObjectPack();
                safe_strcpy(pack->_path, path, sizeof(pack->_path));
                pack->_entries = std::move(entries);
                pack->_dataSize = header.DataSize;
            }
        }
    }

    SDL_RWclose(file);
    return pack;
}

N3dsObjectPack::~N3dsObjectPack()
{
    if (_data != nullptr)
    {
        platform_n3ds_temp_free(_data);
    }
}

bool N3dsObjectPack::ReadData()
{
    if (_data != nullptr) return true;

    unsigned int startTicks = platform_get_ticks();

    // The large temporary memory of loading: from the linear heap where there is room
    // (n3ds.c platform_n3ds_temp_alloc)
    bool read = false;
    uint8 * data = (uint8 *)platform_n3ds_temp_alloc(_dataSize);
    if (data != nullptr)
    {
        // With read, which asks the SD card for all of it in one request. (fread would go
        // through its buffer, 64 KB at a time: n3ds.c __wrap_fopen.)
        int file = open(_path, O_RDONLY);
        if (file >= 0)
        {
            off_t dataOffset = (off_t)(sizeof(Header) + _entries.size() * sizeof(Entry));
            if (lseek(file, dataOffset, SEEK_SET) == dataOffset)
            {
                size_t done = 0;
                while (done < _dataSize)
                {
                    ssize_t count = ::read(file, data + done, _dataSize - done);
                    if (count <= 0) break;
                    done += (size_t)count;
                }
                read = done == _dataSize;
            }
            close(file);
        }
        if (read)
        {
            _data = data;
        }
        else
        {
            platform_n3ds_temp_free(data);
        }
    }

    unsigned int ticks = platform_get_ticks() - startTicks;
    gN3DSObjectOpenTicks += ticks;
    if (read)
    {
        log_warning("n3ds title objects: the pack's %u files, %u KB, read in %u ms",
            (unsigned int)_entries.size(), (unsigned int)(_dataSize / 1024), ticks);
    }
    else
    {
        log_warning("n3ds title objects: the pack's %u KB could not be read (%s), the objects are read from their files",
            (unsigned int)(_dataSize / 1024), data == nullptr ? "no memory" : "read error");
    }
    return read;
}

Object * N3dsObjectPack::CreateObject(const ObjectRepositoryItem * ori)
{
    if (_data == nullptr) return nullptr;

    for (const Entry &entry : _entries)
    {
        if (memcmp(&entry.ObjectEntry, &ori->ObjectEntry, sizeof(rct_object_entry)) != 0) continue;

        unsigned int startTicks = platform_get_ticks();
        Object * object = nullptr;
        SDL_RWops * rw = SDL_RWFromConstMem(_data + entry.Offset, (int)entry.Size);
        if (rw != nullptr)
        {
            object = ObjectFactory::CreateObjectFromLegacyRW(rw, ori->Path);
            SDL_RWclose(rw);
        }
        gN3DSObjectReadTicks += platform_get_ticks() - startTicks;
        return object;
    }
    return nullptr;
}

bool N3dsObjectPack::Write(const ObjectRepositoryItem * const * required)
{
    unsigned int startTicks = platform_get_ticks();

    uint32 count;
    uint32 key = GetKey(required, &count);
    if (count == 0)
    {
        return false;
    }

    utf8 path[MAX_PATH];
    GetPackPath(path, sizeof(path));
    SDL_RWops * out = SDL_RWFromFile(path, "wb");
    if (out == nullptr)
    {
        log_warning("n3ds title objects: %s cannot be written", path);
        return false;
    }

    // The header and the entries are written again at the end, then with the magic number: a
    // pack that was not written to its end (no room, the power cut) is not taken for one
    Header header = { 0, PACK_VERSION, key, count, 0 };
    std::vector<Entry> entries(count);
    bool written = SDL_RWwrite(out, &header, sizeof(header), 1) == 1 &&
                   SDL_RWwrite(out, entries.data(), sizeof(Entry), count) == count;

    void * buffer = nullptr;
    size_t bufferSize = 0;
    size_t next = 0;
    for (int i = 0; i < OBJECT_ENTRY_COUNT && written; i++)
    {
        const ObjectRepositoryItem * ori = required[i];
        if (ori == nullptr) continue;

        written = false;
        SDL_RWops * in = SDL_RWFromFile(ori->Path, "rb");
        if (in == nullptr) continue;

        Sint64 size = SDL_RWsize(in);
        if (size > 0)
        {
            if ((size_t)size > bufferSize)
            {
                free(buffer);
                buffer = malloc((size_t)size);
                bufferSize = buffer == nullptr ? 0 : (size_t)size;
            }
            if (buffer != nullptr &&
                SDL_RWseek(in, 0, RW_SEEK_SET) == 0 &&
                SDL_RWread(in, buffer, (size_t)size, 1) == 1 &&
                SDL_RWwrite(out, buffer, (size_t)size, 1) == 1)
            {
                Entry &entry = entries[next++];
                entry.ObjectEntry = ori->ObjectEntry;
                entry.Offset = header.DataSize;
                entry.Size = (uint32)size;
                header.DataSize += (uint32)size;
                written = true;
            }
        }
        SDL_RWclose(in);
    }
    free(buffer);

    if (written)
    {
        header.Magic = PACK_MAGIC;
        written = SDL_RWseek(out, 0, RW_SEEK_SET) == 0 &&
                  SDL_RWwrite(out, &header, sizeof(header), 1) == 1 &&
                  SDL_RWwrite(out, entries.data(), sizeof(Entry), count) == count;
    }
    if (SDL_RWclose(out) != 0)
    {
        written = false;
    }

    if (written)
    {
        log_warning("n3ds title objects: pack of %u files, %u KB, written in %u ms (once: the next loads of the title read it)",
            (unsigned int)count, (unsigned int)(header.DataSize / 1024), platform_get_ticks() - startTicks);
    }
    else
    {
        platform_file_delete(path);
        log_warning("n3ds title objects: the pack could not be written, the objects of the title are read from their files");
    }
    return written;
}

void N3dsObjectPack::Delete()
{
    utf8 path[MAX_PATH];
    GetPackPath(path, sizeof(path));
    platform_file_delete(path);
}

#endif // __3DS__
