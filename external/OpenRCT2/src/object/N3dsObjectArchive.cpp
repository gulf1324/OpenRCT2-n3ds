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

// n3ds port: see N3dsObjectArchive.h

#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include "N3dsObjectArchive.h"
#include "Object.h"
#include "ObjectFactory.h"
#include "ObjectRepository.h"

extern "C"
{
    #include "../platform/platform.h"
    #include "../util/util.h"
}

// For the object manager's log (ObjectFactory.cpp)
extern unsigned int gN3DSObjectReadTicks;
extern unsigned int gN3DSObjectArchiveCount;
extern unsigned int gN3DSObjectArchiveReads;

constexpr uint32 ARCHIVE_MAGIC = 0x414F334E;    // "N3OA"
constexpr uint32 ARCHIVE_VERSION = 1;           // to be raised when the format changes
constexpr uint32 ARCHIVE_MAX_COUNT = 65536;

// An object's file begins with its entry and the header of its one chunk (encoding, length)
constexpr uint32 OBJECT_FILE_HEADER_SIZE = sizeof(rct_object_entry) + 5;

// An object of up to this size that Expect has not planned for is read whole with one request
constexpr uint32 HEAD_BUFFER_SIZE = 64 * 1024;

// What Expect plans. A request costs about as much as reading 50 KB (5 ms at 10 MB/s), so the
// bytes between two objects are read along when they are fewer than that. A large object gains
// nothing from it and would need its bytes in memory twice: it is read by itself.
constexpr uint32 SPAN_MAX_GAP = 32 * 1024;
constexpr uint32 SPAN_MAX_SIZE = 1024 * 1024;
constexpr uint32 SPAN_MAX_OBJECT_SIZE = 256 * 1024;

// Reads with read, which asks the SD card for all of it in one request. (fread would go through
// its buffer, 64 KB at a time whatever is asked for: n3ds.c __wrap_fopen.)
static bool ReadFileAt(int file, uint32 offset, void * buffer, size_t size)
{
    if (lseek(file, (off_t)offset, SEEK_SET) != (off_t)offset) return false;

    gN3DSObjectArchiveReads++;
    size_t done = 0;
    while (done < size)
    {
        ssize_t count = read(file, (uint8 *)buffer + done, size - done);
        if (count <= 0) return false;
        done += (size_t)count;
    }
    return true;
}

#pragma region The bytes of one object in the file, as a stream

// What CreateObjectFromFile hands to the object factory, which reads an object from an
// SDL_RWops: the object's bytes in the archive. Their first part is in memory (Head), the rest
// is read from the archive's file, straight into the reader's buffer.
struct RegionStream
{
    int             File;
    uint32          FileOffset;     // of the object's bytes in the file
    uint32          Size;
    uint32          Position;
    const uint8 *   Head;
    uint32          HeadSize;
};

static RegionStream * GetRegion(SDL_RWops * context)
{
    return (RegionStream *)context->hidden.unknown.data1;
}

static Sint64 SDLCALL RegionSize(SDL_RWops * context)
{
    return GetRegion(context)->Size;
}

static Sint64 SDLCALL RegionSeek(SDL_RWops * context, Sint64 offset, int whence)
{
    RegionStream * region = GetRegion(context);
    Sint64 position;
    switch (whence) {
    case RW_SEEK_SET: position = offset; break;
    case RW_SEEK_CUR: position = (Sint64)region->Position + offset; break;
    case RW_SEEK_END: position = (Sint64)region->Size + offset; break;
    default: return SDL_SetError("Unknown value for 'whence'");
    }
    if (position < 0 || position > (Sint64)region->Size)
    {
        return SDL_Error(SDL_EFSEEK);
    }
    region->Position = (uint32)position;
    return position;
}

static size_t SDLCALL RegionRead(SDL_RWops * context, void * buffer, size_t size, size_t maxnum)
{
    RegionStream * region = GetRegion(context);
    if (size == 0 || maxnum == 0) return 0;

    size_t bytes = size * maxnum;
    size_t available = region->Size - region->Position;
    if (bytes > available)
    {
        bytes = available;
    }

    size_t fromHead = 0;
    if (region->Position < region->HeadSize)
    {
        fromHead = std::min(bytes, (size_t)(region->HeadSize - region->Position));
        memcpy(buffer, region->Head + region->Position, fromHead);
    }
    if (fromHead < bytes &&
        !ReadFileAt(region->File, region->FileOffset + region->Position + (uint32)fromHead,
                    (uint8 *)buffer + fromHead, bytes - fromHead))
    {
        bytes = fromHead;
    }
    region->Position += (uint32)bytes;
    return bytes / size;
}

static size_t SDLCALL RegionWrite(SDL_RWops * context, const void * buffer, size_t size, size_t num)
{
    SDL_SetError("The object archive is read only");
    return 0;
}

static int SDLCALL RegionClose(SDL_RWops * context)
{
    SDL_FreeRW(context);
    return 0;
}

#pragma endregion

N3dsObjectArchive * N3dsObjectArchive::Open()
{
    utf8 path[MAX_PATH];
    platform_get_user_directory(path, NULL, sizeof(path));
    safe_strcat_path(path, "objdata.pak", sizeof(path));

    int file = open(path, O_RDONLY);
    if (file < 0)
    {
        log_warning("n3ds object archive: none (%s), the objects are read from their own files", path);
        return nullptr;
    }

    N3dsObjectArchive * archive = nullptr;
    Header header;
    if (ReadFileAt(file, 0, &header, sizeof(header)) &&
        header.Magic == ARCHIVE_MAGIC &&
        header.Version == ARCHIVE_VERSION &&
        header.Count != 0 && header.Count <= ARCHIVE_MAX_COUNT)
    {
        uint32 dataOffset = (uint32)(sizeof(Header) + header.Count * sizeof(Entry));
        std::vector<Entry> entries(header.Count);
        if (lseek(file, 0, SEEK_END) == (off_t)((uint64)dataOffset + header.DataSize) &&
            ReadFileAt(file, sizeof(Header), entries.data(), header.Count * sizeof(Entry)))
        {
            // Each entry is the file of an object within the data, and they are in the order
            // that FindEntry searches them in
            bool valid = true;
            for (size_t i = 0; i < entries.size() && valid; i++)
            {
                const Entry &entry = entries[i];
                valid = entry.Size >= OBJECT_FILE_HEADER_SIZE &&
                        entry.Offset <= header.DataSize &&
                        entry.Size <= header.DataSize - entry.Offset &&
                        (i == 0 || memcmp(&entries[i - 1].ObjectEntry, &entry.ObjectEntry, sizeof(rct_object_entry)) < 0);
            }
            uint8 * head = valid ? (uint8 *)malloc(HEAD_BUFFER_SIZE) : nullptr;
            if (head != nullptr)
            {
                archive = new N3dsObjectArchive();
                archive->_file = file;
                archive->_entries = std::move(entries);
                archive->_dataOffset = dataOffset;
                archive->_head = head;
                log_warning("n3ds object archive: %u objects, %u MB (%s)",
                    (unsigned int)header.Count, (unsigned int)(header.DataSize / (1024 * 1024)), path);
            }
        }
    }

    if (archive == nullptr)
    {
        close(file);
        log_warning("n3ds object archive: %s is not a whole archive, the objects are read from their own files", path);
    }
    return archive;
}

N3dsObjectArchive::~N3dsObjectArchive()
{
    Expect(nullptr, 0);
    close(_file);
    free(_head);
}

const N3dsObjectArchive::Entry * N3dsObjectArchive::FindEntry(const rct_object_entry * objectEntry) const
{
    auto found = std::lower_bound(_entries.begin(), _entries.end(), *objectEntry,
        [](const Entry &entry, const rct_object_entry &wanted) -> bool
        {
            return memcmp(&entry.ObjectEntry, &wanted, sizeof(rct_object_entry)) < 0;
        });
    if (found == _entries.end() || memcmp(&found->ObjectEntry, objectEntry, sizeof(rct_object_entry)) != 0)
    {
        return nullptr;
    }
    return &*found;
}

void N3dsObjectArchive::Expect(const ObjectRepositoryItem * const * items, size_t count)
{
    _spans.clear();
    _spanRead = { 0, 0 };
    if (_spanData != nullptr)
    {
        platform_n3ds_temp_free(_spanData);
        _spanData = nullptr;
    }
    if (count == 0) return;

    // Where the expected objects lie in the data, in its order
    std::vector<const Entry *> expected;
    expected.reserve(count);
    for (size_t i = 0; i < count; i++)
    {
        const Entry * entry = FindEntry(&items[i]->ObjectEntry);
        if (entry != nullptr && entry->Size <= SPAN_MAX_OBJECT_SIZE)
        {
            expected.push_back(entry);
        }
    }
    std::sort(expected.begin(), expected.end(), [](const Entry * a, const Entry * b) -> bool
    {
        return a->Offset < b->Offset;
    });

    // An object joins the span before it if it lies close behind it and the span stays small
    uint32 largest = 0;
    for (const Entry * entry : expected)
    {
        if (!_spans.empty())
        {
            Span &span = _spans.back();
            uint32 spanEnd = span.Offset + span.Size;
            if (entry->Offset < spanEnd) continue;      // an object that is expected twice
            if (entry->Offset - spanEnd <= SPAN_MAX_GAP &&
                entry->Offset + entry->Size - span.Offset <= SPAN_MAX_SIZE)
            {
                span.Size = entry->Offset + entry->Size - span.Offset;
                largest = std::max(largest, span.Size);
                continue;
            }
        }
        _spans.push_back({ entry->Offset, entry->Size });
        largest = std::max(largest, entry->Size);
    }

    // The large temporary memory of loading: from the linear heap where there is room (n3ds.c
    // platform_n3ds_temp_alloc). Without it the objects are read one by one.
    if (largest != 0)
    {
        _spanData = (uint8 *)platform_n3ds_temp_alloc(largest);
    }
    if (_spanData == nullptr)
    {
        _spans.clear();
    }
}

// The object's bytes in memory, if it is in a span that Expect has planned: the span is read
// when the first of its objects is asked for.
const uint8 * N3dsObjectArchive::GetFromSpan(const Entry * entry)
{
    if (_spanRead.Size == 0 || !_spanRead.Holds(entry))
    {
        auto next = std::upper_bound(_spans.begin(), _spans.end(), entry->Offset,
            [](uint32 offset, const Span &span) -> bool
            {
                return offset < span.Offset;
            });
        if (next == _spans.begin()) return nullptr;

        const Span &span = *(next - 1);
        if (!span.Holds(entry)) return nullptr;

        _spanRead = { 0, 0 };
        if (!ReadFileAt(_file, _dataOffset + span.Offset, _spanData, span.Size)) return nullptr;
        _spanRead = span;
    }
    return _spanData + (entry->Offset - _spanRead.Offset);
}

// The object read by itself from the file. The first bytes are read into memory: all of a small
// object, and of a large one what the factory reads in small pieces, its entry and the header
// of its chunk. (The chunk's data it then reads with one request, into the buffer it has for it.)
Object * N3dsObjectArchive::CreateObjectFromFile(const Entry * entry, const ObjectRepositoryItem * ori)
{
    RegionStream region;
    region.File = _file;
    region.FileOffset = _dataOffset + entry->Offset;
    region.Size = entry->Size;
    region.Position = 0;
    region.Head = _head;
    region.HeadSize = entry->Size <= HEAD_BUFFER_SIZE ? entry->Size : OBJECT_FILE_HEADER_SIZE;
    if (!ReadFileAt(_file, region.FileOffset, _head, region.HeadSize) ||
        memcmp(_head, &ori->ObjectEntry, sizeof(rct_object_entry)) != 0)
    {
        return nullptr;
    }

    SDL_RWops * stream = SDL_AllocRW();
    if (stream == nullptr) return nullptr;

    stream->type = SDL_RWOPS_UNKNOWN;
    stream->size = RegionSize;
    stream->seek = RegionSeek;
    stream->read = RegionRead;
    stream->write = RegionWrite;
    stream->close = RegionClose;
    stream->hidden.unknown.data1 = &region;
    Object * object = ObjectFactory::CreateObjectFromLegacyRW(stream, ori->Path);
    SDL_RWclose(stream);
    return object;
}

Object * N3dsObjectArchive::CreateObject(const ObjectRepositoryItem * ori)
{
    const Entry * entry = FindEntry(&ori->ObjectEntry);
    if (entry == nullptr) return nullptr;

    unsigned int startTicks = platform_get_ticks();

    Object * object = nullptr;
    const uint8 * bytes = GetFromSpan(entry);
    if (bytes == nullptr)
    {
        object = CreateObjectFromFile(entry, ori);
    }
    else if (memcmp(bytes, &ori->ObjectEntry, sizeof(rct_object_entry)) == 0)
    {
        SDL_RWops * stream = SDL_RWFromConstMem(bytes, (int)entry->Size);
        if (stream != nullptr)
        {
            object = ObjectFactory::CreateObjectFromLegacyRW(stream, ori->Path);
            SDL_RWclose(stream);
        }
    }

    gN3DSObjectReadTicks += platform_get_ticks() - startTicks;
    if (object != nullptr)
    {
        gN3DSObjectArchiveCount++;
    }
    return object;
}

#endif // __3DS__
