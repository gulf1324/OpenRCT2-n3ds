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

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../common.h"
#include "../core/Console.hpp"
#include "../core/FileScanner.h"
#include "../core/FileStream.hpp"
#include "../core/Guard.hpp"
#include "../core/IStream.hpp"
#include "../core/Memory.hpp"
#include "../core/MemoryStream.h"
#include "../core/Path.hpp"
#include "../core/Stopwatch.hpp"
#include "../core/String.hpp"
#include "../localisation/LanguagePack.h"
#include "../PlatformEnvironment.h"
#include "../scenario/ScenarioRepository.h"
#include "Object.h"
#include "ObjectFactory.h"
#include "ObjectManager.h"
#include "ObjectRepository.h"
#ifdef __3DS__
#include "N3dsObjectArchive.h"
#include "N3dsObjectPack.h"
#endif
#include "RideObject.h"
#include "StexObject.h"

extern "C"
{
    #include "../config.h"
    #include "../localisation/localisation.h"
    #include "../object.h"
    #include "../object_list.h"
    #include "../platform/platform.h"
    #include "../rct2.h"
    #include "../util/sawyercoding.h"
    #include "../util/util.h"
}

#ifdef __3DS__
// n3ds port: the index of the 3DS does not depend on the language. The original holds each
// object's name in the language the index was made in and scans every object file again when
// the game starts in another: 50 s on a 3DS, and as long again for the title's object pack,
// which goes with a scan. The only thing in the index that the language changes is that name,
// and only the object selection of the scenario editor shows it. So an item of this index has
// the names of the object's file in all its languages (ObjectRepositoryItem::N3dsNames), and
// the name is picked from them, or taken from the language file, when the index is read and
// when the language changes (N3dsPickName).
constexpr uint16 OBJECT_REPOSITORY_VERSION = 11;
#else
constexpr uint16 OBJECT_REPOSITORY_VERSION = 10;
#endif

#pragma pack(push, 1)
struct ObjectRepositoryHeader
{
    uint16  Version;
    uint16  LanguageId;
    uint32  TotalFiles;
    uint64  TotalFileSize;
    uint32  FileDateModifiedChecksum;
    uint32  PathChecksum;
    uint32  NumItems;
};
assert_struct_size(ObjectRepositoryHeader, 28);
#pragma pack(pop)

struct ObjectEntryHash
{
    size_t operator()(const rct_object_entry &entry) const
    {
        uint32 hash = 5381;
        for (int i = 0; i < 8; i++)
        {
            hash = ((hash << 5) + hash) + entry.name[i];
        }
        return hash;
    }
};

struct ObjectEntryEqual
{
    bool operator()(const rct_object_entry &lhs, const rct_object_entry &rhs) const
    {
        return memcmp(&lhs.name, &rhs.name, 8) == 0;
    }
};

using ObjectEntryMap = std::unordered_map<rct_object_entry, size_t, ObjectEntryHash, ObjectEntryEqual>;

static void ReportMissingObject(const rct_object_entry * entry);

class ObjectRepository : public IObjectRepository
{
    IPlatformEnvironment *              _env = nullptr;
    std::vector<ObjectRepositoryItem>   _items;
    QueryDirectoryResult                _queryDirectoryResult = { 0 };
    ObjectEntryMap                      _itemMap;
    uint16                              _languageId = 0;
#ifdef __3DS__
    // n3ds port: the archive of all object files, if the SD card has one: GetN3dsArchive
    N3dsObjectArchive *                 _n3dsArchive = nullptr;
    bool                                _n3dsArchiveOpened = false;
#endif

public:
    ObjectRepository(IPlatformEnvironment * env)
    {
        _env = env;
    }

    ~ObjectRepository()
    {
        ClearItems();
#ifdef __3DS__
        delete _n3dsArchive;
#endif
    }

    void LoadOrConstruct() override
    {
        ClearItems();

        _queryDirectoryResult = { 0 };

        const std::string &rct2Path = _env->GetDirectoryPath(DIRBASE::RCT2, DIRID::OBJECT);
        const std::string &openrct2Path = _env->GetDirectoryPath(DIRBASE::USER, DIRID::OBJECT);
#ifdef __3DS__
        // n3ds port: when there is an index it is taken as it is. The original first lists
        // every object folder to see whether the index is out of date (the number of files,
        // their total size, their paths): 1.3 s of the start-up on a 3DS, where the 2122 files
        // are in 45 subfolders. So object files that are added, removed or moved by hand are
        // not noticed: user/objects.idx is to be deleted then (the scripts that fill the SD
        // card do, sd_sync.py and setup_emulator.py). What the game adds itself is written to
        // the index at once (AddObject), and a file of the index that is not there makes the
        // next start scan again (LoadObject).
        if (Load())
        {
            return;
        }
        // There is no index, or it is of another version, or it cannot be read to its end.
        // Nothing of it is kept for the scan.
        ClearItems();
        _queryDirectoryResult = { 0 };
#endif
        QueryDirectory(&_queryDirectoryResult, rct2Path);
        QueryDirectory(&_queryDirectoryResult, openrct2Path);

#ifdef __3DS__
        {
#else
        if (!Load())
        {
#endif
            _languageId = gCurrentLanguage;

            Construct();
            Save();
        }

        // SortItems();
    }

    size_t GetNumObjects() const override
    {
        return _items.size();
    }

    const ObjectRepositoryItem * GetObjects() const override
    {
        return _items.data();
    }

    const ObjectRepositoryItem * FindObject(const utf8 * name) const override
    {
        rct_object_entry entry = { 0 };
        utf8 entryName[9] = { ' ' };
        String::Set(entryName, sizeof(entryName), name);
        Memory::Copy<void>(entry.name, entryName, 8);

        auto kvp = _itemMap.find(entry);
        if (kvp != _itemMap.end())
        {
            return &_items[kvp->second];
        }
        return nullptr;
    }

    const ObjectRepositoryItem * FindObject(const rct_object_entry * objectEntry) const override
    {
        auto kvp = _itemMap.find(*objectEntry);
        if (kvp != _itemMap.end())
        {
            return &_items[kvp->second];
        }
        return nullptr;
    }

    Object * LoadObject(const ObjectRepositoryItem * ori) override
    {
        Guard::ArgumentNotNull(ori, GUARD_LINE);

#ifdef __3DS__
        // n3ds port: from the archive of all object files if the SD card has one, which saves
        // opening the object's own file (N3dsObjectArchive.h). The object's file is read if the
        // archive does not have the object.
        N3dsObjectArchive * archive = GetN3dsArchive();
        if (archive != nullptr)
        {
            Object * archivedObject = archive->CreateObject(ori);
            if (archivedObject != nullptr)
            {
                return archivedObject;
            }
        }
#endif
        Object * object = ObjectFactory::CreateObjectFromLegacyFile(ori->Path);
#ifdef __3DS__
        // n3ds port: the index is not compared with the folders at start-up (LoadOrConstruct).
        // A file of the index that is not there shows that it is out of date: without the
        // index the next start scans the folders again.
        if (object == nullptr && !platform_file_exists(ori->Path))
        {
            const std::string &indexPath = _env->GetFilePath(PATHID::CACHE_OBJECTS);
            log_warning("n3ds: the object file %s of the index is not there: %s is deleted, the next start scans the objects again",
                ori->Path, indexPath.c_str());
            platform_file_delete(indexPath.c_str());
        }
#endif
        return object;
    }

#ifdef __3DS__
    void N3dsPickNames() override
    {
        for (ObjectRepositoryItem &item : _items)
        {
            Memory::Free(item.Name);
            item.Name = N3dsPickName(item);
        }
    }

    void N3dsExpectLoads(const ObjectRepositoryItem * const * items, size_t count) override
    {
        // Saying that there are no more does not open the archive
        N3dsObjectArchive * archive = count == 0 ? _n3dsArchive : GetN3dsArchive();
        if (archive != nullptr)
        {
            archive->Expect(items, count);
        }
    }
#endif

    void RegisterLoadedObject(const ObjectRepositoryItem * ori, Object * object) override
    {
        ObjectRepositoryItem * item = &_items[ori->Id];

        Guard::Assert(item->LoadedObject == nullptr, GUARD_LINE);
        item->LoadedObject = object;
    }

    void UnregisterLoadedObject(const ObjectRepositoryItem * ori, Object * object) override
    {
        ObjectRepositoryItem * item = &_items[ori->Id];
        if (item->LoadedObject == object)
        {
            item->LoadedObject = nullptr;
        }
    }

    void AddObject(const rct_object_entry * objectEntry, const void * data, size_t dataSize) override
    {
        utf8 objectName[9];
        object_entry_get_name_fixed(objectName, sizeof(objectName), objectEntry);

        // Check that the object is loadable before writing it
        Object * object = ObjectFactory::CreateObjectFromLegacyData(objectEntry, data, dataSize);
        if (object == nullptr)
        {
            Console::Error::WriteLine("[%s] Unable to export object.", objectName);
        }
        else
        {
            utf8 path[MAX_PATH];
            GetPathForNewObject(path, sizeof(path), objectName);

            log_verbose("Adding object: [%s]", objectName);
            try
            {
                SaveObject(path, objectEntry, data, dataSize);
                ScanObject(path);
#ifdef __3DS__
                // n3ds port: the index is not compared with the folders at start-up
                // (LoadOrConstruct), where the original would notice the new file and scan
                // everything again. It is brought up to date here.
                Save();
#endif
            }
            catch (Exception ex)
            {
                Console::Error::WriteLine("Failed saving object: [%s] to '%s'.", objectName, path);
            }
        }
    }

private:
#ifdef __3DS__
    // n3ds port: the archive of all object files, opened when it is first needed. Null if the
    // SD card has none.
    N3dsObjectArchive * GetN3dsArchive()
    {
        if (!_n3dsArchiveOpened)
        {
            _n3dsArchiveOpened = true;
            _n3dsArchive = N3dsObjectArchive::Open();
        }
        return _n3dsArchive;
    }
#endif

    void ClearItems()
    {
        for (uint32 i = 0; i < _items.size(); i++)
        {
            FreeItem(&_items[i]);
        }
        _items.clear();
        _itemMap.clear();
    }

    void QueryDirectory(QueryDirectoryResult * result, const std::string &directory)
    {
        utf8 pattern[MAX_PATH];
        String::Set(pattern, sizeof(pattern), directory.c_str());
        Path::Append(pattern, sizeof(pattern), "*.dat");
        Path::QueryDirectory(result, pattern);
    }

    void Construct()
    {
        utf8 objectDirectory[MAX_PATH];
        Path::GetDirectory(objectDirectory, sizeof(objectDirectory), gRCT2AddressObjectDataPath);

        Console::WriteLine("Scanning %lu objects...", _queryDirectoryResult.TotalFiles);
#ifdef __3DS__
        // n3ds port: the object files may have changed, so the copy of some of them goes
        // too. The next load of the title writes it again (N3dsObjectPack.h).
        N3dsObjectPack::Delete();
#endif

        auto stopwatch = Stopwatch();
        stopwatch.Start();

        const std::string &rct2Path = _env->GetDirectoryPath(DIRBASE::RCT2, DIRID::OBJECT);
        const std::string &openrct2Path = _env->GetDirectoryPath(DIRBASE::USER, DIRID::OBJECT);
        ScanDirectory(rct2Path);
        ScanDirectory(openrct2Path);

        stopwatch.Stop();
        Console::WriteLine("Scanning complete in %.2f seconds.", stopwatch.GetElapsedMilliseconds() / 1000.0f);
    }

    void ScanDirectory(const std::string &directory)
    {
        utf8 pattern[MAX_PATH];
        String::Set(pattern, sizeof(pattern), directory.c_str());
        Path::Append(pattern, sizeof(pattern), "*.dat");

        IFileScanner * scanner = Path::ScanDirectory(pattern, true);
        while (scanner->Next())
        {
            const utf8 * enumPath = scanner->GetPath();
            ScanObject(enumPath);
        }
        delete scanner;
    }

    void ScanObject(const utf8 * path)
    {
        Object * object = ObjectFactory::CreateObjectFromLegacyFile(path);
        if (object != nullptr)
        {
            ObjectRepositoryItem item = { 0 };
            item.ObjectEntry = *object->GetObjectEntry();
            item.Path = String::Duplicate(path);
            item.Name = String::Duplicate(object->GetName());
#ifdef __3DS__
            N3dsSetNames(&item, object->N3dsGetStringTable());
#endif
            object->SetRepositoryItem(&item);
            AddItem(&item);

            delete object;
        }
    }

    bool Load()
    {
        const std::string &path = _env->GetFilePath(PATHID::CACHE_OBJECTS);
        try
        {
            auto fs = FileStream(path, FILE_MODE_OPEN);
            auto header = fs.ReadValue<ObjectRepositoryHeader>();

#ifdef __3DS__
            // n3ds port: not compared with the folders, which are not listed (LoadOrConstruct),
            // nor with the language, which the index does not depend on (ReadItem)
            if (header.Version == OBJECT_REPOSITORY_VERSION)
            {
                // For Save, when the game adds an object (AddObject)
                _languageId = header.LanguageId;
                _queryDirectoryResult.TotalFiles = header.TotalFiles;
                _queryDirectoryResult.TotalFileSize = header.TotalFileSize;
                _queryDirectoryResult.FileDateModifiedChecksum = header.FileDateModifiedChecksum;
                _queryDirectoryResult.PathChecksum = header.PathChecksum;
#else
            if (header.Version == OBJECT_REPOSITORY_VERSION &&
                header.LanguageId == gCurrentLanguage &&
                header.TotalFiles == _queryDirectoryResult.TotalFiles &&
                header.TotalFileSize == _queryDirectoryResult.TotalFileSize &&
                header.FileDateModifiedChecksum == _queryDirectoryResult.FileDateModifiedChecksum &&
                header.PathChecksum == _queryDirectoryResult.PathChecksum)
            {
#endif
                // Header matches, so the index is not out of date

                // Buffer the rest of file into memory to speed up item reading
                size_t dataSize = (size_t)(fs.GetLength() - fs.GetPosition());
                void * data = fs.ReadArray<uint8>(dataSize);
                auto ms = MemoryStream(data, dataSize, MEMORY_ACCESS_READ | MEMORY_ACCESS_OWNER);

                // Read items
                for (uint32 i = 0; i < header.NumItems; i++)
                {
                    ObjectRepositoryItem item = ReadItem(&ms);
                    AddItem(&item);
                }
                return true;
            }
            Console::WriteLine("Object repository is out of date.");
            return false;
        }
        catch (IOException ex)
        {
            return false;
        }
    }

    void Save() const
    {
        const std::string &path = _env->GetFilePath(PATHID::CACHE_OBJECTS);
        try
        {
            auto fs = FileStream(path, FILE_MODE_WRITE);

            // Write header
            ObjectRepositoryHeader header;
            header.Version = OBJECT_REPOSITORY_VERSION;
            header.LanguageId = _languageId;
            header.TotalFiles = _queryDirectoryResult.TotalFiles;
            header.TotalFileSize = _queryDirectoryResult.TotalFileSize;
            header.FileDateModifiedChecksum = _queryDirectoryResult.FileDateModifiedChecksum;
            header.PathChecksum = _queryDirectoryResult.PathChecksum;
            header.NumItems = (uint32)_items.size();
            fs.WriteValue(header);

            // Write items
            for (uint32 i = 0; i < header.NumItems; i++)
            {
                WriteItem(&fs, _items[i]);
            }
        }
        catch (IOException ex)
        {
            log_error("Unable to write object repository index to '%s'.", path.c_str());
        }
    }

    void SortItems()
    {
        std::sort(_items.begin(), _items.end(), [](const ObjectRepositoryItem &a,
                                                   const ObjectRepositoryItem &b) -> bool
        {
            return strcmp(a.Name, b.Name) < 0;
        });

        // Rebuild item map
        _itemMap.clear();
        for (size_t i = 0; i < _items.size(); i++)
        {
            rct_object_entry entry = _items[i].ObjectEntry;
            _itemMap[entry] = i;
        }
    }

    bool AddItem(ObjectRepositoryItem * item)
    {
        const ObjectRepositoryItem * conflict = FindObject(&item->ObjectEntry);
        if (conflict == nullptr)
        {
            size_t index = _items.size();
            item->Id = index;
            _items.push_back(*item);
            _itemMap[item->ObjectEntry] = index;
            return true;
        }
        else
        {
            Console::Error::WriteLine("Object conflict: '%s'", conflict->Path);
            Console::Error::WriteLine("               : '%s'", item->Path);
            return false;
        }
    }

    static ObjectRepositoryItem ReadItem(IStream * stream)
    {
        ObjectRepositoryItem item = { 0 };

        item.ObjectEntry = stream->ReadValue<rct_object_entry>();
        item.Path = stream->ReadString();
#ifdef __3DS__
        // n3ds port: the names of the object's file, and of them the one for the language. A 0
        // after them ends the last text if the file is broken.
        item.N3dsNamesSize = stream->ReadValue<uint16>();
        item.N3dsNames = Memory::Allocate<uint8>(item.N3dsNamesSize + 1);
        stream->Read(item.N3dsNames, item.N3dsNamesSize);
        item.N3dsNames[item.N3dsNamesSize] = 0;
        item.Name = N3dsPickName(item);
#else
        item.Name = stream->ReadString();
#endif

        switch (item.ObjectEntry.flags & 0x0F) {
        case OBJECT_TYPE_RIDE:
            item.RideFlags = stream->ReadValue<uint8>();
            for (int i = 0; i < 2; i++)
            {
                item.RideCategory[i] = stream->ReadValue<uint8>();
            }
            for (int i = 0; i < 3; i++)
            {
                item.RideType[i] = stream->ReadValue<uint8>();
            }
            break;
        case OBJECT_TYPE_SCENERY_SETS:
            item.NumThemeObjects = stream->ReadValue<uint16>();
            item.ThemeObjects = Memory::AllocateArray<rct_object_entry>(item.NumThemeObjects);
            for (uint16 i = 0; i < item.NumThemeObjects; i++)
            {
                item.ThemeObjects[i] = stream->ReadValue<rct_object_entry>();
            }
            break;
        }
        return item;
    }

    static void WriteItem(IStream * stream, const ObjectRepositoryItem &item)
    {
        stream->WriteValue(item.ObjectEntry);
        stream->WriteString(item.Path);
#ifdef __3DS__
        stream->WriteValue<uint16>(item.N3dsNamesSize);
        stream->Write(item.N3dsNames, item.N3dsNamesSize);
#else
        stream->WriteString(item.Name);
#endif

        switch (item.ObjectEntry.flags & 0x0F) {
        case OBJECT_TYPE_RIDE:
            stream->WriteValue<uint8>(item.RideFlags);
            for (int i = 0; i < 2; i++)
            {
                stream->WriteValue<uint8>(item.RideCategory[i]);
            }
            for (int i = 0; i < 3; i++)
            {
                stream->WriteValue<uint8>(item.RideType[i]);
            }
            break;
        case OBJECT_TYPE_SCENERY_SETS:
            stream->WriteValue<uint16>(item.NumThemeObjects);
            for (uint16 i = 0; i < item.NumThemeObjects; i++)
            {
                stream->WriteValue<rct_object_entry>(item.ThemeObjects[i]);
            }
            break;
        }
    }

#ifdef __3DS__
    // n3ds port: keeps the names that the object's file has for it with the item
    // (ObjectRepositoryItem::N3dsNames), in the order of the object's string table
    static void N3dsSetNames(ObjectRepositoryItem * item, const StringTable * stringTable)
    {
        std::vector<uint8> names;
        for (const StringTableEntry &entry : stringTable->N3dsGetEntries())
        {
            if (entry.Id != OBJ_STRING_ID_NAME)
            {
                continue;
            }
            size_t textSize = String::SizeOf(entry.Text) + 1;
            if (names.size() + 1 + textSize > UINT16_MAX)
            {
                break;
            }
            names.push_back(entry.LanguageId);
            names.insert(names.end(), (const uint8 *)entry.Text, (const uint8 *)entry.Text + textSize);
        }
        item->N3dsNamesSize = (uint16)names.size();
        item->N3dsNames = Memory::Allocate<uint8>(names.size() + 1);
        Memory::Copy(item->N3dsNames, names.data(), names.size());
        item->N3dsNames[names.size()] = 0;
    }

    // n3ds port: the name of an item in the current language, as a loaded object has it
    // (Object::GetString): the name the language file has for the object, else one of the
    // names of the object's file, chosen as StringTable::Sort puts one first: the one in the
    // language, else the English one, else the one whose language has the lowest number.
    static utf8 * N3dsPickName(const ObjectRepositoryItem &item)
    {
        char identifier[9] = { 0 };
        Memory::Copy(identifier, item.ObjectEntry.name, 8);
        const utf8 * name = language_get_object_override_string(identifier, OBJ_STRING_ID_NAME);
        if (name == nullptr)
        {
            uint8 currentLanguage = LanguagesDescriptors[gCurrentLanguage].rct2_original_id;
            int bestRank = INT32_MAX;
            const uint8 * next = item.N3dsNames;
            const uint8 * end = next + item.N3dsNamesSize;
            while (next < end)
            {
                uint8 language = *next++;
                const utf8 * text = (const utf8 *)next;
                next += String::SizeOf(text) + 1;

                int rank = 2 + language;
                if (language == currentLanguage) rank = 0;
                else if (language == RCT2_LANGUAGE_ID_ENGLISH_UK) rank = 1;
                if (rank < bestRank)
                {
                    bestRank = rank;
                    name = text;
                }
            }
        }
        return String::Duplicate(name != nullptr ? name : "");
    }
#endif

    static void FreeItem(ObjectRepositoryItem * item)
    {
        Memory::Free(item->Path);
        Memory::Free(item->Name);
        item->Path = nullptr;
        item->Name = nullptr;
#ifdef __3DS__
        Memory::Free(item->N3dsNames);
        item->N3dsNames = nullptr;
#endif

        uint8 objectType = item->ObjectEntry.flags & 0x0F;
        switch (objectType) {
        case OBJECT_TYPE_SCENERY_SETS:
            Memory::Free(item->ThemeObjects);
            item->ThemeObjects = nullptr;
            break;
        }
    }

    static void SaveObject(const utf8 * path,
                           const rct_object_entry * entry,
                           const void * data, size_t dataSize,
                           bool fixChecksum = true)
    {
        if (fixChecksum)
        {
            uint32 realChecksum = object_calculate_checksum(entry, data, dataSize);
            if (realChecksum != entry->checksum)
            {
                char objectName[9];
                object_entry_get_name_fixed(objectName, sizeof(objectName), entry);
                log_verbose("[%s] Incorrect checksum, adding salt bytes...", objectName);

                // Calculate the value of extra bytes that can be appended to the data so that the
                // data is then valid for the object's checksum
                size_t extraBytesCount = 0;
                void * extraBytes = CalculateExtraBytesToFixChecksum(realChecksum, entry->checksum, &extraBytesCount);

                // Create new data blob with appended bytes
                size_t newDataSize = dataSize + extraBytesCount;
                void * newData = Memory::Allocate<void>(newDataSize);
                void * newDataSaltOffset = (void *)((uintptr_t)newData + dataSize);
                Memory::Copy(newData, data, dataSize);
                Memory::Copy(newDataSaltOffset, extraBytes, extraBytesCount);

                try
                {
                    uint32 newRealChecksum = object_calculate_checksum(entry, newData, newDataSize);
                    if (newRealChecksum != entry->checksum)
                    {
                        Guard::Fail("CalculateExtraBytesToFixChecksum failed to fix checksum.", GUARD_LINE);

                        // Save old data form
                        SaveObject(path, entry, data, dataSize, false);
                    }
                    else
                    {
                        // Save new data form
                        SaveObject(path, entry, newData, newDataSize, false);
                    }
                    Memory::Free(newData);
                    Memory::Free(extraBytes);
                }
                catch (Exception ex)
                {
                    Memory::Free(newData);
                    Memory::Free(extraBytes);
                    throw;
                }
                return;
            }
        }

        // Encode data
        uint8 objectType = entry->flags & 0x0F;
        sawyercoding_chunk_header chunkHeader;
        chunkHeader.encoding = object_entry_group_encoding[objectType];
        chunkHeader.length = (uint32)dataSize;
        uint8 * encodedDataBuffer = Memory::Allocate<uint8>(0x600000);
        size_t encodedDataSize = sawyercoding_write_chunk_buffer(encodedDataBuffer, (uint8 *)data, chunkHeader);

        // Save to file
        try
        {
            auto fs = FileStream(path, FILE_MODE_WRITE);
            fs.Write(entry, sizeof(rct_object_entry));
            fs.Write(encodedDataBuffer, encodedDataSize);

            Memory::Free(encodedDataBuffer);
        }
        catch (Exception ex)
        {
            Memory::Free(encodedDataBuffer);
            throw;
        }
    }

    static void * CalculateExtraBytesToFixChecksum(int currentChecksum, int targetChecksum, size_t * outSize)
    {
        // Allocate 11 extra bytes to manipulate the checksum
        uint8 * salt = Memory::Allocate<uint8>(11);
        if (outSize != nullptr) *outSize = 11;

        // Next work out which bits need to be flipped to make the current checksum match the one in the file
        // The bitwise rotation compensates for the rotation performed during the checksum calculation*/
        int bitsToFlip = targetChecksum ^ ((currentChecksum << 25) | (currentChecksum >> 7));

        // Each set bit encountered during encoding flips one bit of the resulting checksum (so each bit of the checksum is an
        // XOR of bits from the file). Here, we take each bit that should be flipped in the checksum and set one of the bits in
        // the data that maps to it. 11 bytes is the minimum needed to touch every bit of the checksum - with less than that,
        // you wouldn't always be able to make the checksum come out to the desired target
        salt[0] = (bitsToFlip & 0x00000001) << 7;
        salt[1] = ((bitsToFlip & 0x00200000) >> 14);
        salt[2] = ((bitsToFlip & 0x000007F8) >> 3);
        salt[3] = ((bitsToFlip & 0xFF000000) >> 24);
        salt[4] = ((bitsToFlip & 0x00100000) >> 13);
        salt[5] = (bitsToFlip & 0x00000004) >> 2;
        salt[6] = 0;
        salt[7] = ((bitsToFlip & 0x000FF000) >> 12);
        salt[8] = (bitsToFlip & 0x00000002) >> 1;
        salt[9] = (bitsToFlip & 0x00C00000) >> 22;
        salt[10] = (bitsToFlip & 0x00000800) >> 11;

        return salt;
    }

    void GetPathForNewObject(utf8 * buffer, size_t bufferSize, const char * name)
    {
        char normalisedName[9] = { 0 };
        for (int i = 0; i < 8; i++)
        {
            if (name[i] != ' ')
            {
                normalisedName[i] = toupper(name[i]);
            }
            else
            {
                normalisedName[i] = '\0';
            }
        }

        const std::string &userObjPath = _env->GetDirectoryPath(DIRBASE::USER, DIRID::OBJECT);
        String::Set(buffer, bufferSize, userObjPath.c_str());
        platform_ensure_directory_exists(buffer);

        Path::Append(buffer, bufferSize, normalisedName);
        String::Append(buffer, bufferSize, ".DAT");

        uint32 counter = 2;
        for (; platform_file_exists(buffer);)
        {
            utf8 counterString[8];
            snprintf(counterString, sizeof(counterString), "-%02X", counter);
            counter++;

            String::Set(buffer, bufferSize, userObjPath.c_str());
            Path::Append(buffer, bufferSize, normalisedName);
            String::Append(buffer, bufferSize, counterString);
            String::Append(buffer, bufferSize, ".DAT");
        }
    }
};

static std::unique_ptr<ObjectRepository> _objectRepository;

IObjectRepository * CreateObjectRepository(IPlatformEnvironment * env)
{
    _objectRepository = std::unique_ptr<ObjectRepository>(new ObjectRepository(env));
    return _objectRepository.get();
}

IObjectRepository * GetObjectRepository()
{
    return _objectRepository.get();
}

static int GetObjectEntryIndex(uint8 objectType, uint8 entryIndex)
{
    int result = 0;
    for (uint8 i = 0; i < objectType; i++)
    {
        result += object_entry_group_counts[i];
    }
    result += entryIndex;
    return result;
}

extern "C"
{
    rct_object_entry * object_list_find(rct_object_entry * entry)
    {
        IObjectRepository * objRepo = GetObjectRepository();
        const ObjectRepositoryItem * item = objRepo->FindObject(entry);
        return (rct_object_entry *)&item->ObjectEntry;
    }

    rct_object_entry * object_list_find_by_name(const char * name)
    {
        IObjectRepository * objRepo = GetObjectRepository();
        const ObjectRepositoryItem * item = objRepo->FindObject(name);
        return (rct_object_entry *)&item->ObjectEntry;
    }

    void object_list_load()
    {
        IObjectRepository * objectRepository = GetObjectRepository();
        objectRepository->LoadOrConstruct();

        IObjectManager * objectManager = GetObjectManager();
        objectManager->UnloadAll();
    }

    bool object_load_entries(rct_object_entry * entries)
    {
        log_verbose("loading required objects");

        IObjectManager * objectManger = GetObjectManager();
        bool result = objectManger->LoadObjects(entries, OBJECT_ENTRY_COUNT);

        log_verbose("finished loading required objects");
        return result;
    }

    void * object_repository_load_object(const rct_object_entry * objectEntry)
    {
        Object * object = nullptr;
        IObjectRepository * objRepository = GetObjectRepository();
        const ObjectRepositoryItem * ori = objRepository->FindObject(objectEntry);
        if (ori != nullptr)
        {
            object = objRepository->LoadObject(ori);
            if (object != nullptr)
            {
                object->Load();
            }
        }
        return (void *)object;
    }

    void scenario_translate(scenario_index_entry * scenarioEntry, const rct_object_entry * stexObjectEntry)
    {
        rct_string_id localisedStringIds[3];
        if (language_get_localised_scenario_strings(scenarioEntry->name, localisedStringIds))
        {
            if (localisedStringIds[0] != STR_NONE)
            {
                String::Set(scenarioEntry->name, sizeof(scenarioEntry->name), language_get_string(localisedStringIds[0]));
            }
            if (localisedStringIds[2] != STR_NONE)
            {
                String::Set(scenarioEntry->details, sizeof(scenarioEntry->details), language_get_string(localisedStringIds[2]));
            }
        }
        else
        {
            // Checks for a scenario string object (possibly for localisation)
            if ((stexObjectEntry->flags & 0xFF) != 255)
            {
                IObjectRepository * objectRepository = GetObjectRepository();
                const ObjectRepositoryItem * ori = objectRepository->FindObject(stexObjectEntry);
                if (ori != nullptr)
                {
                    Object * object = objectRepository->LoadObject(ori);
                    if (object != nullptr)
                    {
                        StexObject * stexObject = static_cast<StexObject*>(object);
                        const utf8 * scenarioName = stexObject->GetScenarioName();
                        const utf8 * scenarioDetails = stexObject->GetScenarioDetails();

                        String::Set(scenarioEntry->name, sizeof(scenarioEntry->name), scenarioName);
                        String::Set(scenarioEntry->details, sizeof(scenarioEntry->details), scenarioDetails);

                        delete object;
                    }
                }
            }
        }
    }

    int object_load_packed(SDL_RWops * rw)
    {
        IObjectRepository * objRepo = GetObjectRepository();

        rct_object_entry entry;
        SDL_RWread(rw, &entry, 16, 1);

        // Check if we already have this object
        if (objRepo->FindObject(&entry) != nullptr)
        {
            sawyercoding_skip_chunk(rw);
        }
        else
        {
            // Read object and save to new file
            uint8 * chunk = Memory::Allocate<uint8>(0x600000);
            if (chunk == nullptr)
            {
                log_error("Failed to allocate buffer for packed object.");
                return 0;
            }

            size_t chunkSize = sawyercoding_read_chunk_with_size(rw, chunk, 0x600000);
            chunk = Memory::Reallocate(chunk, chunkSize);
            if (chunk == nullptr)
            {
                log_error("Failed to reallocate buffer for packed object.");
                return 0;
            }

            objRepo->AddObject(&entry, chunk, chunkSize);

            Memory::Free(chunk);
        }
        return 1;
    }

    bool object_saved_packed(SDL_RWops * rw, const rct_object_entry * entry)
    {
        IObjectRepository * objectRepository = GetObjectRepository();
        const ObjectRepositoryItem * item = objectRepository->FindObject(entry);
        if (item == nullptr)
        {
            return false;
        }

        auto fs = FileStream(item->Path, FILE_MODE_OPEN);
        rct_object_entry fileEntry = fs.ReadValue<rct_object_entry>();
        if (!object_entry_compare(entry, &fileEntry))
        {
            return false;
        }

        sawyercoding_chunk_header chunkHeader = fs.ReadValue<sawyercoding_chunk_header>();
        uint8 * chunkData = fs.ReadArray<uint8>(chunkHeader.length);

        if (SDL_RWwrite(rw, entry, sizeof(rct_object_entry), 1) != 1)
        {
            Memory::Free(chunkData);
            return false;
        }
        if (SDL_RWwrite(rw, &chunkHeader, sizeof(sawyercoding_chunk_header), 1) != 1)
        {
            Memory::Free(chunkData);
            return false;
        }
        if (SDL_RWwrite(rw, chunkData, chunkHeader.length, 1) != 1)
        {
            Memory::Free(chunkData);
            return false;
        }
        Memory::Free(chunkData);

        return true;
    }

    size_t object_repository_get_items_count()
    {
        IObjectRepository * objectRepository = GetObjectRepository();
        return objectRepository->GetNumObjects();
    }

    const ObjectRepositoryItem * object_repository_get_items()
    {
        IObjectRepository * objectRepository = GetObjectRepository();
        return objectRepository->GetObjects();
    }

    const ObjectRepositoryItem * object_repository_find_object_by_entry(const rct_object_entry * entry)
    {
        IObjectRepository * objectRepository = GetObjectRepository();
        return objectRepository->FindObject(entry);
    }

    const ObjectRepositoryItem * object_repository_find_object_by_name(const char * name)
    {
        IObjectRepository * objectRepository = GetObjectRepository();
        return objectRepository->FindObject(name);
    }

    void object_delete(void * object)
    {
        if (object != nullptr)
        {
            Object * baseObject = (Object *)object;
            baseObject->Unload();
            delete baseObject;
        }
    }

    const utf8 * object_get_description(const void * object)
    {
        const Object * baseObject = (const Object *)object;
        switch (baseObject->GetObjectType()) {
        case OBJECT_TYPE_RIDE:
        {
            const RideObject * rideObject = static_cast<const RideObject *>(baseObject);
            return rideObject->GetDescription();
        }
        case OBJECT_TYPE_SCENARIO_TEXT:
        {
            const StexObject * stexObject = static_cast<const StexObject *>(baseObject);
            return stexObject->GetScenarioDetails();
        }
        default:
            return "";
        }
    }

    const utf8 * object_get_capacity(const void * object)
    {
        const Object * baseObject = (const Object *)object;
        switch (baseObject->GetObjectType()) {
        case OBJECT_TYPE_RIDE:
        {
            auto rideObject = static_cast<const RideObject *>(baseObject);
            return rideObject->GetCapacity();
        }
        default:
            return "";
        }
    }

    void object_draw_preview(const void * object, rct_drawpixelinfo * dpi, sint32 width, sint32 height)
    {
        const Object * baseObject = (const Object *)object;
        baseObject->DrawPreview(dpi, width, height);
    }

    bool object_entry_compare(const rct_object_entry * a, const rct_object_entry * b)
    {
        // If an official object don't bother checking checksum
        if ((a->flags & 0xF0) || (b->flags & 0xF0))
        {
            if ((a->flags & 0x0F) != (b->flags & 0x0F))
            {
                return 0;
            }
            int match = memcmp(a->name, b->name, 8);
            if (match)
            {
                return 0;
            }
        }
        else
        {
            if (a->flags != b->flags)
            {
                return 0;
            }
            int match = memcmp(a->name, b->name, 8);
            if (match)
            {
                return 0;
            }
            if (a->checksum != b->checksum)
            {
                return 0;
            }
        }
        return 1;
    }

    int object_calculate_checksum(const rct_object_entry * entry, const void * data, size_t dataLength)
    {
        const uint8 *entryBytePtr = (uint8*)entry;

        uint32 checksum = 0xF369A75B;
        checksum ^= entryBytePtr[0];
        checksum = rol32(checksum, 11);
        for (int i = 4; i < 12; i++)
        {
            checksum ^= entryBytePtr[i];
            checksum = rol32(checksum, 11);
        }

        uint8 * dataBytes = (uint8 *)data;
        for (size_t i = 0; i < dataLength; i++)
        {
            checksum ^= dataBytes[i];
            checksum = rol32(checksum, 11);
        }

        return (int)checksum;
    }
}

static void ReportMissingObject(const rct_object_entry * entry)
{
    utf8 objName[9] = { 0 };
    Memory::Copy(objName, entry->name, 8);
    Console::Error::WriteLine("[%s] Object not found.", objName);
}
