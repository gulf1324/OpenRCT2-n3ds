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
#include <array>
#include <memory>
#include <unordered_set>
#include <vector>
#include "../core/Console.hpp"
#include "../core/Memory.hpp"
#include "FootpathItemObject.h"
#include "LargeSceneryObject.h"
#include "Object.h"
#include "ObjectManager.h"
#include "ObjectRepository.h"
#ifdef __3DS__
#include "N3dsObjectPack.h"
#endif
#include "SceneryGroupObject.h"
#include "SmallSceneryObject.h"
#include "WallObject.h"

extern "C"
{
    #include "../object_list.h"
#ifdef __3DS__
    #include "../platform/platform.h"
#endif
}

#ifdef __3DS__
// n3ds port: where the time of loading a park's objects goes (ObjectFactory.cpp)
extern unsigned int gN3DSObjectOpenTicks;
extern unsigned int gN3DSObjectReadTicks;
extern unsigned int gN3DSObjectArchiveCount;
extern unsigned int gN3DSObjectArchiveReads;
static unsigned int _n3dsObjectRegisterTicks;

bool gN3dsLoadingTitleObjects = false;
// The pack is of use from this many objects on: reading all of it takes as long as opening
// and reading about 35 files (going back to the menu from a park that shares most of its
// objects with the title's loads only a few)
constexpr size_t N3DS_PACK_MIN_OBJECTS = 40;
#endif

class ObjectManager : public IObjectManager
{
private:
    IObjectRepository * _objectRepository;
    Object * *          _loadedObjects = nullptr;
#ifdef __3DS__
    // n3ds port: where GetOrLoadObject takes the objects' files from during LoadObjects, if set
    N3dsObjectPack *    _n3dsPack = nullptr;
#endif

public:
    ObjectManager(IObjectRepository * objectRepository)
    {
        Guard::ArgumentNotNull(objectRepository);

        _objectRepository = objectRepository;
        _loadedObjects = Memory::AllocateArray<Object *>(OBJECT_ENTRY_COUNT);
        for (size_t i = 0; i < OBJECT_ENTRY_COUNT; i++)
        {
            _loadedObjects[i] = nullptr;
        }

        UpdateLegacyLoadedObjectList();
        UpdateSceneryGroupIndexes();
        reset_type_to_ride_entry_index_map();
    }

    ~ObjectManager() override
    {
        SetNewLoadedObjectList(nullptr);
    }

    Object * GetLoadedObject(size_t index) override
    {
        if (_loadedObjects == nullptr)
        {
            return nullptr;
        }
        return _loadedObjects[index];
    }

    Object * GetLoadedObject(const rct_object_entry * entry) override
    {
        Object * loadedObject = nullptr;
        const ObjectRepositoryItem * ori = _objectRepository->FindObject(entry);
        if (ori != nullptr)
        {
            loadedObject = ori->LoadedObject;
        }
        return loadedObject;
    }

    uint8 GetLoadedObjectEntryIndex(const Object * object) override
    {
        uint8 result = UINT8_MAX;
        size_t index = GetLoadedObjectIndex(object);
        if (index != SIZE_MAX)
        {
            get_type_entry_index(index, nullptr, &result);
        }
        return result;
    }

    Object * LoadObject(const rct_object_entry * entry) override
    {
        Object * loadedObject = nullptr;
        const ObjectRepositoryItem * ori = _objectRepository->FindObject(entry);
        if (ori != nullptr)
        {
            loadedObject = ori->LoadedObject;
            if (loadedObject == nullptr)
            {
                uint8 objectType = entry->flags & 0x0F;
                sint32 slot = FindSpareSlot(objectType);
                if (slot != -1)
                {
                    loadedObject = GetOrLoadObject(ori);
                    if (loadedObject != nullptr)
                    {
                        _loadedObjects[slot] = loadedObject;
                        UpdateLegacyLoadedObjectList();
                        UpdateSceneryGroupIndexes();
                        reset_type_to_ride_entry_index_map();
                    }
                }
            }
        }
        return loadedObject;
    }

    bool LoadObjects(const rct_object_entry * entries, size_t count) override
    {
        // Find all the required objects
        size_t numRequiredObjects;
        auto requiredObjects = new const ObjectRepositoryItem *[OBJECT_ENTRY_COUNT];
        if (!GetRequiredObjects(entries, requiredObjects, &numRequiredObjects))
        {
            delete[] requiredObjects;
            return false;
        }

        // Create a new list of loaded objects
        size_t numNewLoadedObjects;
        Object * * loadedObjects = LoadObjects(requiredObjects, &numNewLoadedObjects);

        delete[] requiredObjects;

        if (loadedObjects == nullptr)
        {
            UnloadAll();
            return false;
        }
        else
        {
            SetNewLoadedObjectList(loadedObjects);
            UpdateLegacyLoadedObjectList();
            UpdateSceneryGroupIndexes();
            reset_type_to_ride_entry_index_map();
            log_verbose("%u / %u new objects loaded", numNewLoadedObjects, numRequiredObjects);
            return true;
        }
    }

    void UnloadObjects(const rct_object_entry * entries, size_t count) override
    {
        // TODO there are two performance issues here:
        //        - FindObject for every entry which is a dictionary lookup
        //        - GetLoadedObjectIndex for every entry which enumerates _loadedList

        size_t numObjectsUnloaded = 0;
        for (size_t i = 0; i < count; i++)
        {
            const rct_object_entry * entry = &entries[i];
            const ObjectRepositoryItem * ori = _objectRepository->FindObject(entry);
            if (ori != nullptr)
            {
                Object * loadedObject = ori->LoadedObject;
                if (loadedObject != nullptr)
                {
                    UnloadObject(loadedObject);
                    numObjectsUnloaded++;
                }
            }
        }

        if (numObjectsUnloaded > 0)
        {
            UpdateLegacyLoadedObjectList();
            UpdateSceneryGroupIndexes();
            reset_type_to_ride_entry_index_map();
        }
    }

    void UnloadAll() override
    {
        if (_loadedObjects != nullptr)
        {
            for (int i = 0; i < OBJECT_ENTRY_COUNT; i++)
            {
                UnloadObject(_loadedObjects[i]);
            }
        }
        UpdateLegacyLoadedObjectList();
        UpdateSceneryGroupIndexes();
        reset_type_to_ride_entry_index_map();
    }

    void ResetObjects() override
    {
        if (_loadedObjects != nullptr)
        {
            for (size_t i = 0; i < OBJECT_ENTRY_COUNT; i++)
            {
                Object * loadedObject = _loadedObjects[i];
                if (loadedObject != nullptr)
                {
                    loadedObject->Unload();
                    loadedObject->Load();
                }
            }
            UpdateLegacyLoadedObjectList();
            UpdateSceneryGroupIndexes();
            reset_type_to_ride_entry_index_map();
        }
    }

private:
    sint32 FindSpareSlot(uint8 objectType)
    {
        if (_loadedObjects != nullptr)
        {
            sint32 firstIndex = GetIndexFromTypeEntry(objectType, 0);
            sint32 endIndex = firstIndex + object_entry_group_counts[objectType];
            for (sint32 i = firstIndex; i < endIndex; i++)
            {
                if (_loadedObjects[i] == nullptr)
                {
                    return i;
                }
            }
        }
        return -1;
    }

    size_t GetLoadedObjectIndex(const Object * object)
    {
        Guard::ArgumentNotNull(object, GUARD_LINE);

        size_t result = SIZE_MAX;
        if (_loadedObjects != nullptr)
        {
            for (size_t i = 0; i < OBJECT_ENTRY_COUNT; i++)
            {
                if (_loadedObjects[i] == object)
                {
                    result = i;
                    break;
                }
            }
        }
        return result;
    }

    void SetNewLoadedObjectList(Object * * newLoadedObjects)
    {
        if (newLoadedObjects == nullptr)
        {
            UnloadAll();
        }
        else
        {
            UnloadObjectsExcept(newLoadedObjects);
        }
        Memory::Free(_loadedObjects);
        _loadedObjects = newLoadedObjects;
    }

    void UnloadObject(Object * object)
    {
        if (object != nullptr)
        {
            // TODO try to prevent doing a repository search
            const ObjectRepositoryItem * ori = _objectRepository->FindObject(object->GetObjectEntry());
            if (ori != nullptr)
            {
                _objectRepository->UnregisterLoadedObject(ori, object);
            }

            object->Unload();
            delete object;

            // Because its possible to have the same loaded object for multiple
            // slots, we have to make sure find and set all of them to nullptr
            if (_loadedObjects != nullptr)
            {
                for (size_t i = 0; i < OBJECT_ENTRY_COUNT; i++)
                {
                    if (_loadedObjects[i] == object)
                    {
                        _loadedObjects[i] = nullptr;
                    }
                }
            }
        }
    }

    void UnloadObjectsExcept(Object * * newLoadedObjects)
    {
        if (_loadedObjects == nullptr)
        {
            return;
        }

        // Build a hash set for quick checking
        auto exceptSet = std::unordered_set<Object *>();
        for (int i = 0; i < OBJECT_ENTRY_COUNT; i++)
        {
            Object * object = newLoadedObjects[i];
            if (object != nullptr)
            {
                exceptSet.insert(object);
            }
        }

        // Unload objects that are not in the hash set
        size_t totalObjectsLoaded = 0;
        size_t numObjectsUnloaded = 0;
        for (int i = 0; i < OBJECT_ENTRY_COUNT; i++)
        {
            Object * object = _loadedObjects[i];
            if (object != nullptr)
            {
                totalObjectsLoaded++;
                if (exceptSet.find(object) == exceptSet.end())
                {
                    UnloadObject(object);
                    numObjectsUnloaded++;
                }
            }
        }

        log_verbose("%u / %u objects unloaded", numObjectsUnloaded, totalObjectsLoaded);
    }

    void UpdateLegacyLoadedObjectList()
    {
        for (int i = 0; i < OBJECT_ENTRY_COUNT; i++)
        {
            Object * loadedObject = nullptr;
            if (_loadedObjects != nullptr)
            {
                loadedObject = _loadedObjects[i];
            }

            uint8 objectType, entryIndex;
            get_type_entry_index(i, &objectType, &entryIndex);

            rct_object_entry_extended * legacyEntry = &object_entry_groups[objectType].entries[entryIndex];
            void * * legacyChunk = &object_entry_groups[objectType].chunks[entryIndex];
            if (loadedObject == nullptr)
            {
                Memory::Set(legacyEntry, 0xFF, sizeof(rct_object_entry_extended));
                *legacyChunk = (void *)-1;
            }
            else
            {
                legacyEntry->entry = *loadedObject->GetObjectEntry();
                legacyEntry->chunk_size = 0;
                *legacyChunk = loadedObject->GetLegacyData();
            }
        }
    }

    void UpdateSceneryGroupIndexes()
    {
        if (_loadedObjects != nullptr)
        {
            for (size_t i = 0; i < OBJECT_ENTRY_COUNT; i++)
            {
                Object * loadedObject = _loadedObjects[i];
                if (loadedObject != nullptr)
                {
                    rct_scenery_entry * sceneryEntry;
                    switch (loadedObject->GetObjectType()) {
                    case OBJECT_TYPE_SMALL_SCENERY:
                        sceneryEntry = (rct_scenery_entry *)loadedObject->GetLegacyData();
                        sceneryEntry->small_scenery.scenery_tab_id = GetPrimarySceneryGroupEntryIndex(loadedObject);
                        break;
                    case OBJECT_TYPE_LARGE_SCENERY:
                        sceneryEntry = (rct_scenery_entry *)loadedObject->GetLegacyData();
                        sceneryEntry->large_scenery.scenery_tab_id = GetPrimarySceneryGroupEntryIndex(loadedObject);
                        break;
                    case OBJECT_TYPE_WALLS:
                        sceneryEntry = (rct_scenery_entry *)loadedObject->GetLegacyData();
                        sceneryEntry->wall.scenery_tab_id = GetPrimarySceneryGroupEntryIndex(loadedObject);
                        break;
                    case OBJECT_TYPE_BANNERS:
                        sceneryEntry = (rct_scenery_entry *)loadedObject->GetLegacyData();
                        sceneryEntry->banner.scenery_tab_id = GetPrimarySceneryGroupEntryIndex(loadedObject);
                        break;
                    case OBJECT_TYPE_PATH_BITS:
                        sceneryEntry = (rct_scenery_entry *)loadedObject->GetLegacyData();
                        sceneryEntry->path_bit.scenery_tab_id = GetPrimarySceneryGroupEntryIndex(loadedObject);
                        break;
                    case OBJECT_TYPE_SCENERY_SETS:
                        auto sgObject = static_cast<SceneryGroupObject *>(loadedObject);
                        sgObject->UpdateEntryIndexes();
                        break;
                    }
                }
            }

            // HACK Scenery window will lose its tabs after changing the the scenery group indexing
            //      for now just close it, but it will be better to later tell it to invalidate the tabs
            window_close_by_class(WC_SCENERY);
        }
    }

    uint8 GetPrimarySceneryGroupEntryIndex(Object * loadedObject)
    {
        auto sceneryObject = static_cast<SceneryObject *>(loadedObject);
        const rct_object_entry * primarySGEntry = sceneryObject->GetPrimarySceneryGroup();
        Object * sgObject = GetLoadedObject(primarySGEntry);

        uint8 entryIndex = 255;
        if (sgObject != nullptr)
        {
            entryIndex = GetLoadedObjectEntryIndex(sgObject);
        }
        return entryIndex;
    }

    bool GetRequiredObjects(const rct_object_entry * entries,
                            const ObjectRepositoryItem * * requiredObjects,
                            size_t * outNumRequiredObjects)
    {
        bool missingObjects = false;
        size_t numRequiredObjects = 0;
        for (int i = 0; i < OBJECT_ENTRY_COUNT; i++)
        {
            const rct_object_entry * entry = &entries[i];
            const ObjectRepositoryItem * ori = nullptr;
            if (!object_entry_is_empty(entry))
            {
                ori = _objectRepository->FindObject(entry);
                if (ori == nullptr)
                {
                    missingObjects = true;
                    ReportMissingObject(entry);
                }
                numRequiredObjects++;
            }
            requiredObjects[i] = ori;
        }

        if (outNumRequiredObjects != nullptr)
        {
            *outNumRequiredObjects = numRequiredObjects;
        }
        return !missingObjects;
    }

    Object * * LoadObjects(const ObjectRepositoryItem * * requiredObjects, size_t * outNewObjectsLoaded)
    {
        size_t newObjectsLoaded = 0;
        Object * * loadedObjects = Memory::AllocateArray<Object *>(OBJECT_ENTRY_COUNT);
#ifdef __3DS__
        // n3ds port: this is most of the time a park takes to load. Report the progress for the
        // loading box and log where the time goes.
        size_t numToLoad = 0;
        for (int i = 0; i < OBJECT_ENTRY_COUNT; i++)
        {
            if (requiredObjects[i] != nullptr && requiredObjects[i]->LoadedObject == nullptr) numToLoad++;
        }
        unsigned int startTicks = platform_get_ticks();
        gN3DSObjectOpenTicks = 0;
        gN3DSObjectReadTicks = 0;
        gN3DSObjectArchiveCount = 0;
        gN3DSObjectArchiveReads = 0;
        _n3dsObjectRegisterTicks = 0;

        // n3ds port: the objects of the title's first park are read from one file, which is
        // written here the first time (N3dsObjectPack.h)
        std::unique_ptr<N3dsObjectPack> pack;
        bool writePack = false;
        if (gN3dsLoadingTitleObjects)
        {
            // The first park only: a sequence that loads another before its first wait would
            // have the pack written again for each of them, at every start
            gN3dsLoadingTitleObjects = false;
            pack.reset(N3dsObjectPack::Open(requiredObjects));
            if (pack == nullptr)
            {
                writePack = true;
            }
            else if (numToLoad < N3DS_PACK_MIN_OBJECTS || !pack->ReadData())
            {
                pack.reset();
            }
        }
        _n3dsPack = pack.get();

        // n3ds port: the objects are loaded in the order in which their files lie in the archive
        // of all object files, which is the order of the bytes of their entries
        // (N3dsObjectArchive.h), and not in the order of the park's slots: the archive can then
        // read those that lie close together with one request. (The numbers that an object gets
        // for its images and strings when it is loaded depend on what was loaded and freed
        // before in any case, and nothing keeps them.)
        int loadOrder[OBJECT_ENTRY_COUNT];
        for (int i = 0; i < OBJECT_ENTRY_COUNT; i++)
        {
            loadOrder[i] = i;
        }
        std::stable_sort(loadOrder, loadOrder + OBJECT_ENTRY_COUNT, [requiredObjects](int a, int b) -> bool
        {
            const ObjectRepositoryItem * first = requiredObjects[a];
            const ObjectRepositoryItem * second = requiredObjects[b];
            if (first == nullptr || second == nullptr)
            {
                return first != nullptr;    // the slots without an object come last
            }
            return memcmp(&first->ObjectEntry, &second->ObjectEntry, sizeof(rct_object_entry)) < 0;
        });

        // The repository is told which objects it is going to be asked for. (Not when the
        // title's pack is where they come from.)
        if (_n3dsPack == nullptr)
        {
            std::vector<const ObjectRepositoryItem *> expected;
            for (int n = 0; n < OBJECT_ENTRY_COUNT; n++)
            {
                const ObjectRepositoryItem * ori = requiredObjects[loadOrder[n]];
                if (ori != nullptr && ori->LoadedObject == nullptr)
                {
                    expected.push_back(ori);
                }
            }
            _objectRepository->N3dsExpectLoads(expected.data(), expected.size());
        }
#endif
        for (int n = 0; n < OBJECT_ENTRY_COUNT; n++)
        {
#ifdef __3DS__
            int i = loadOrder[n];
#else
            int i = n;
#endif
            Object * loadedObject = nullptr;
            const ObjectRepositoryItem * ori = requiredObjects[i];
            if (ori != nullptr)
            {
                loadedObject = ori->LoadedObject;
                if (loadedObject == nullptr)
                {
                    loadedObject = GetOrLoadObject(ori);
                    if (loadedObject == nullptr)
                    {
                        ReportObjectLoadProblem(&ori->ObjectEntry);
                        Memory::Free(loadedObjects);
#ifdef __3DS__
                        _n3dsPack = nullptr;
                        _objectRepository->N3dsExpectLoads(nullptr, 0);
#endif
                        return nullptr;
                    } else {
                        newObjectsLoaded++;
#ifdef __3DS__
                        platform_n3ds_loading_progress((int)newObjectsLoaded, (int)numToLoad);
#endif
                    }
                }
            }
            loadedObjects[i] = loadedObject;
        }
#ifdef __3DS__
        _n3dsPack = nullptr;
        pack.reset();
        _objectRepository->N3dsExpectLoads(nullptr, 0);
        log_warning("n3ds object load: %u objects in %u ms (open files %u ms, read and decode %u ms, register %u ms), %u from the archive with %u reads",
            (unsigned int)newObjectsLoaded, platform_get_ticks() - startTicks,
            gN3DSObjectOpenTicks, gN3DSObjectReadTicks, _n3dsObjectRegisterTicks,
            gN3DSObjectArchiveCount, gN3DSObjectArchiveReads);
        if (writePack)
        {
            N3dsObjectPack::Write(requiredObjects);
        }
#endif
        if (outNewObjectsLoaded != nullptr)
        {
            *outNewObjectsLoaded = newObjectsLoaded;
        }
        return loadedObjects;
    }

    Object * GetOrLoadObject(const ObjectRepositoryItem * ori)
    {
        Object * loadedObject = ori->LoadedObject;
        if (loadedObject == nullptr)
        {
            // Try to load object
#ifdef __3DS__
            // n3ds port: from the title's pack if LoadObjects has one, else (or if it cannot be
            // read from there) from its file
            if (_n3dsPack != nullptr)
            {
                loadedObject = _n3dsPack->CreateObject(ori);
            }
            if (loadedObject == nullptr)
#endif
            loadedObject = _objectRepository->LoadObject(ori);
            if (loadedObject != nullptr)
            {
#ifdef __3DS__
                unsigned int loadTicks = platform_get_ticks();
                loadedObject->Load();
                _n3dsObjectRegisterTicks += platform_get_ticks() - loadTicks;
#else
                loadedObject->Load();
#endif

                // Connect the ori to the registered object
                _objectRepository->RegisterLoadedObject(ori, loadedObject);
            }
        }
        return loadedObject;
    }

    static void ReportMissingObject(const rct_object_entry * entry)
    {
        utf8 objName[9] = { 0 };
        Memory::Copy(objName, entry->name, 8);
        Console::Error::WriteLine("[%s] Object not found.", objName);
    }

    static void ReportObjectLoadProblem(const rct_object_entry * entry)
    {
        utf8 objName[9] = { 0 };
        Memory::Copy(objName, entry->name, 8);
        Console::Error::WriteLine("[%s] Object could not be loaded.", objName);
    }

    static sint32 GetIndexFromTypeEntry(uint8 objectType, uint8 entryIndex)
    {
        int result = 0;
        for (uint8 i = 0; i < objectType; i++)
        {
            result += object_entry_group_counts[i];
        }
        result += entryIndex;
        return result;
    }
};

static std::unique_ptr<ObjectManager> _objectManager;

IObjectManager * GetObjectManager()
{
    if (_objectManager == nullptr)
    {
        IObjectRepository * objectRepository = GetObjectRepository();
        if (objectRepository != nullptr)
        {
            _objectManager = std::unique_ptr<ObjectManager>(new ObjectManager(objectRepository));
        }
    }
    return _objectManager.get();
}

extern "C"
{
    void * object_manager_get_loaded_object_by_index(size_t index)
    {
        IObjectManager * objectManager = GetObjectManager();
        Object * loadedObject = objectManager->GetLoadedObject(index);
        return (void *)loadedObject;
    }

    void * object_manager_get_loaded_object(const rct_object_entry * entry)
    {
        IObjectManager * objectManager = GetObjectManager();
        Object * loadedObject = objectManager->GetLoadedObject(entry);
        return (void *)loadedObject;
    }

    uint8 object_manager_get_loaded_object_entry_index(const void * loadedObject)
    {
        IObjectManager * objectManager = GetObjectManager();
        const Object * object = (const Object *)loadedObject;
        uint8 entryIndex = objectManager->GetLoadedObjectEntryIndex(object);
        return entryIndex;
    }

    void * object_manager_load_object(const rct_object_entry * entry)
    {
        IObjectManager * objectManager = GetObjectManager();
        Object * loadedObject = objectManager->LoadObject(entry);
        return (void *)loadedObject;
    }

    void object_manager_unload_objects(const rct_object_entry * entries, size_t count)
    {
        IObjectManager * objectManager = GetObjectManager();
        objectManager->UnloadObjects(entries, count);
    }

    void object_manager_unload_all_objects()
    {
        IObjectManager * objectManager = GetObjectManager();
        if (objectManager != nullptr)
        {
            objectManager->UnloadAll();
        }
    }
}
