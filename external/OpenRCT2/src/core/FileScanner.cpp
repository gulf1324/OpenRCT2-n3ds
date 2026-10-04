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

#include "../common.h"

#ifdef __WINDOWS__
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__3DS__)
    #include <dirent.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
#elif defined(__WINDOWS__)
    extern "C" {
        // Windows needs this for widechar <-> utf8 conversion utils
        #include "../localisation/language.h"
    }
#endif

// n3ds port: the 3DS uses the POSIX scanner (newlib has dirent, stat, fnmatch)
#include <stack>
#include <vector>
#include "FileScanner.h"
#include "Memory.hpp"
#include "Path.hpp"
#include "String.hpp"

extern "C"
{
    #include "../platform/platform.h"
}

enum DIRECTORY_CHILD_TYPE
{
    DCT_DIRECTORY,
    DCT_FILE,
};

struct DirectoryChild
{
    DIRECTORY_CHILD_TYPE Type;
    std::string Name;

    // Files only
    uint64 Size;
    uint64 LastModified;
};

static uint32 GetPathChecksum(const utf8 * path);
static bool MatchWildcard(const utf8 * fileName, const utf8 * pattern);

class FileScannerBase : public IFileScanner
{
private:
    struct DirectoryState
    {
        std::string                 Path;
        std::vector<DirectoryChild> Listing;
        sint32                      Index;
    };

    // Options
    utf8 *      _rootPath;
    utf8 * *    _patterns;
    size_t      _numPatterns;
    bool        _recurse;

    // State
    bool                        _started;
    std::stack<DirectoryState>  _directoryStack;

    // Current
    FileInfo    * _currentFileInfo;
    utf8        * _currentPath;

public:
    FileScannerBase(const utf8 * pattern, bool recurse)
    {
        _rootPath = Memory::Allocate<utf8>(MAX_PATH);
        Path::GetDirectory(_rootPath, MAX_PATH, pattern);
        _recurse = recurse;
        _numPatterns = GetPatterns(&_patterns, Path::GetFileName(pattern));

        _currentPath = Memory::Allocate<utf8>(MAX_PATH);
        _currentFileInfo = Memory::Allocate<FileInfo>();

        Reset();
    }

    ~FileScannerBase() override
    {
        Memory::Free(_rootPath);
        for (size_t i = 0; i < _numPatterns; i++)
        {
            Memory::Free(_patterns[i]);
        }
        Memory::Free(_patterns);
        Memory::Free(_currentPath);
        Memory::Free(_currentFileInfo);
    }

    const FileInfo * GetFileInfo() const override
    {
        return _currentFileInfo;
    }

    const utf8 * GetPath() const override
    {
        return _currentPath;
    }

    const utf8 * GetPathRelative() const override
    {
        // +1 to remove the path separator
        return _currentPath + String::SizeOf(_rootPath) + 1;
    }

    void Reset() override
    {
        _started = false;
        _directoryStack = std::stack<DirectoryState>();
        _currentPath[0] = 0;
    }

    bool Next() override
    {
        if (!_started)
        {
            _started = true;
            PushState(_rootPath);
        }

        while (_directoryStack.size() != 0)
        {
            DirectoryState * state = &_directoryStack.top();
            state->Index++;
            if (state->Index >= (sint32)state->Listing.size())
            {
                _directoryStack.pop();
            }
            else
            {
                const DirectoryChild * child = &state->Listing[state->Index];
                if (child->Type == DCT_DIRECTORY)
                {
                    utf8 childPath[MAX_PATH];
                    String::Set(childPath, sizeof(childPath), state->Path.c_str());
                    Path::Append(childPath, sizeof(childPath), child->Name.c_str());

                    PushState(childPath);
                }
                else if (PatternMatch(child->Name.c_str()))
                {
                    String::Set(_currentPath, MAX_PATH, state->Path.c_str());
                    Path::Append(_currentPath, MAX_PATH, child->Name.c_str());

                    _currentFileInfo->Name = child->Name.c_str();
                    _currentFileInfo->Size = child->Size;
                    _currentFileInfo->LastModified = child->LastModified;
                    return true;
                }
            }
        }
        return false;
    }

private:
    void PushState(const utf8 * directory)
    {
        DirectoryState newState;
        newState.Path = std::string(directory);
        newState.Index = -1;
        GetDirectoryChildren(newState.Listing, directory);
        _directoryStack.push(newState);
    }

    bool PatternMatch(const utf8 * fileName)
    {
        for (size_t i = 0; i < _numPatterns; i++)
        {
            if (MatchWildcard(fileName, _patterns[i]))
            {
                return true;
            }
        }
        return false;
    }

    static size_t GetPatterns(utf8 * * * outPatterns, const utf8 * delimitedPatterns)
    {
        std::vector<utf8 *> patterns;

        const utf8 * start = delimitedPatterns;
        const utf8 * ch = start;
        utf8 c;
        do
        {
            c = *ch;
            if (c == '\0' || c == ';')
            {
                size_t length = (size_t)(ch - start);
                if (length > 0)
                {
                    utf8 * newPattern = Memory::Allocate<utf8>(length + 1);
                    Memory::Copy(newPattern, start, length);
                    newPattern[length] = '\0';
                    patterns.push_back(newPattern);
                }
                start = ch + 1;
            }
            ch++;
        }
        while (c != '\0');

        *outPatterns = Memory::DuplicateArray(patterns.data(), patterns.size());
        return patterns.size();
    }

protected:
    virtual void GetDirectoryChildren(std::vector<DirectoryChild> &children, const utf8 * path) abstract;

};

#ifdef __WINDOWS__

class FileScannerWindows final : public FileScannerBase
{
public:
    FileScannerWindows(const utf8 * pattern, bool recurse)
        : FileScannerBase(pattern, recurse)
    {
    }

protected:
    void GetDirectoryChildren(std::vector<DirectoryChild> &children, const utf8 * path) override
    {
        size_t pathLength = String::SizeOf(path);
        utf8 * pattern = Memory::Duplicate(path, pathLength + 3);
        pattern[pathLength + 0] = '\\';
        pattern[pathLength + 1] = '*';
        pattern[pathLength + 2] = '\0';

        wchar_t * wPattern = utf8_to_widechar(pattern);

        WIN32_FIND_DATAW findData;
        HANDLE hFile = FindFirstFileW(wPattern, &findData);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (lstrcmpW(findData.cFileName, L".") != 0 &&
                    lstrcmpW(findData.cFileName, L"..") != 0)
                {
                    DirectoryChild child = CreateChild(&findData);
                    children.push_back(child);
                }
            }
            while (FindNextFileW(hFile, &findData));
            FindClose(hFile);
        }

        Memory::Free(pattern);
        Memory::Free(wPattern);
    }

private:
    static DirectoryChild CreateChild(const WIN32_FIND_DATAW * child)
    {
        DirectoryChild result;

        utf8 * name = widechar_to_utf8(child->cFileName);
        result.Name = std::string(name);
        Memory::Free(name);

        if (child->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            result.Type = DCT_DIRECTORY;
        }
        else
        {
            result.Type = DCT_FILE;
            result.Size = ((uint64)child->nFileSizeHigh << 32ULL) | (uint64)child->nFileSizeLow;
            result.LastModified = ((uint64)child->ftLastWriteTime.dwHighDateTime << 32ULL) | (uint64)child->ftLastWriteTime.dwLowDateTime;
        }
        return result;
    }
};

#endif // __WINDOWS__

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__3DS__)

#ifdef __3DS__
// n3ds port: not final, FileScannerN3DS below builds on it
class FileScannerUnix : public FileScannerBase
#else
class FileScannerUnix final : public FileScannerBase
#endif
{
public:
    FileScannerUnix(const utf8 * pattern, bool recurse)
        : FileScannerBase(pattern, recurse)
    {
    }

protected:
    void GetDirectoryChildren(std::vector<DirectoryChild> &children, const utf8 * path) override
    {
        struct dirent * * namelist;
        int count = scandir(path, &namelist, FilterFunc, alphasort);
        if (count > 0)
        {
            for (int i = 0; i < count; i++)
            {
                const struct dirent * node = namelist[i];
                if (!String::Equals(node->d_name, ".") &&
                    !String::Equals(node->d_name, ".."))
                {
                    DirectoryChild child = CreateChild(path, node);
                    children.push_back(child);
                }
                free(namelist[i]);
            }
            free(namelist);
        }
    }

private:
    static int FilterFunc(const struct dirent * d)
    {
        return 1;
    }

    static DirectoryChild CreateChild(const utf8 * directory, const struct dirent * node)
    {
        DirectoryChild result;
        result.Name = std::string(node->d_name);
        if (node->d_type & DT_DIR)
        {
            result.Type = DCT_DIRECTORY;
        }
        else
        {
            result.Type = DCT_FILE;

            // Get the full path of the file
            size_t pathSize = String::SizeOf(directory) + 1 + String::SizeOf(node->d_name) + 1;
            utf8 * path = Memory::Allocate<utf8>(pathSize);
            String::Set(path, pathSize, directory);
            Path::Append(path, pathSize, node->d_name);

            struct stat statInfo;
            int statRes = stat(path, &statInfo);
            if (statRes != -1)
            {
                result.Size = statInfo.st_size;
                result.LastModified = statInfo.st_mtime;
            }

            Memory::Free(path);
        }
        return result;
    }
};

#endif // defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__3DS__)

#ifdef __3DS__
#include <3ds.h>
#include <algorithm>

/**
 * n3ds port: the POSIX scanner calls stat() for every file to get its size. On the 3DS each
 * stat() opens the file, reads its size and closes it, and FAT looks the name up by walking the
 * whole folder each time; with 2000+ objects in one folder the object/track/scenario checks took
 * minutes at every start. The 3DS file system lists names together with sizes, so read them
 * from there. Results match the POSIX scanner (sorted names, same sizes, no modification time:
 * libctru's stat() reports 0), so existing objects.idx / tracks.idx stay valid.
 */
class FileScannerN3DS final : public FileScannerUnix
{
public:
    FileScannerN3DS(const utf8 * pattern, bool recurse)
        : FileScannerUnix(pattern, recurse)
    {
    }

protected:
    void GetDirectoryChildren(std::vector<DirectoryChild> &children, const utf8 * path) override
    {
        if (!ReadDirectory(children, path))
        {
            FileScannerUnix::GetDirectoryChildren(children, path);
        }
    }

private:
    static bool ReadDirectory(std::vector<DirectoryChild> &children, const utf8 * path)
    {
        static FS_Archive sdmcArchive = 0;
        if (sdmcArchive == 0 &&
            R_FAILED(FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
        {
            sdmcArchive = 0;
            return false;
        }

        // SD card paths only, without the device prefix or a trailing separator
        std::string dirPath = path;
        if (dirPath.compare(0, 5, "sdmc:") == 0) dirPath.erase(0, 5);
        if (dirPath.empty() || dirPath[0] != '/') return false;
        while (dirPath.size() > 1 && dirPath[dirPath.size() - 1] == '/') dirPath.erase(dirPath.size() - 1);

        Handle dir;
        if (R_FAILED(FSUSER_OpenDirectory(&dir, sdmcArchive, fsMakePath(PATH_ASCII, dirPath.c_str()))))
        {
            return false;
        }

        std::vector<DirectoryChild> listing;
        FS_DirectoryEntry entries[32];
        u32 count;
        while (R_SUCCEEDED(FSDIR_Read(dir, &count, 32, entries)) && count > 0)
        {
            for (u32 i = 0; i < count; i++)
            {
                char name[0x106 * 3 + 1] = { 0 };
                utf16_to_utf8((uint8_t *)name, entries[i].name, sizeof(name) - 1);
                DirectoryChild child;
                child.Name = name;
                if (entries[i].attributes & FS_ATTRIBUTE_DIRECTORY)
                {
                    child.Type = DCT_DIRECTORY;
                    child.Size = 0;
                }
                else
                {
                    child.Type = DCT_FILE;
                    child.Size = entries[i].fileSize;
                }
                child.LastModified = 0;
                listing.push_back(child);
            }
        }
        FSDIR_Close(dir);

        std::sort(listing.begin(), listing.end(), [](const DirectoryChild &a, const DirectoryChild &b) -> bool
        {
            return strcmp(a.Name.c_str(), b.Name.c_str()) < 0;
        });
        children.insert(children.end(), listing.begin(), listing.end());
        return true;
    }
};
#endif // __3DS__

IFileScanner * Path::ScanDirectory(const utf8 * pattern, bool recurse)
{
#ifdef __WINDOWS__
    return new FileScannerWindows(pattern, recurse);
#elif defined(__3DS__)
    return new FileScannerN3DS(pattern, recurse);
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
    return new FileScannerUnix(pattern, recurse);
#endif
}

void Path::QueryDirectory(QueryDirectoryResult * result, const utf8 * pattern)
{
    IFileScanner * scanner = Path::ScanDirectory(pattern, true);
    while (scanner->Next())
    {
        const FileInfo * fileInfo = scanner->GetFileInfo();
        const utf8 * path = scanner->GetPath();

        result->TotalFiles++;
        result->TotalFileSize += fileInfo->Size;
        result->FileDateModifiedChecksum ^=
            (uint32)(fileInfo->LastModified >> 32) ^
            (uint32)(fileInfo->LastModified & 0xFFFFFFFF);
        result->FileDateModifiedChecksum = ror32(result->FileDateModifiedChecksum, 5);
        result->PathChecksum += GetPathChecksum(path);
    }
    delete scanner;
}

static uint32 GetPathChecksum(const utf8 * path)
{
    uint32 hash = 0xD8430DED;
    for (const utf8 * ch = path; *ch != '\0'; ch++)
    {
        hash += (*ch);
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

/**
 * Due to FindFirstFile / FindNextFile searching for DOS names as well, *.doc also matches *.docx which isn't what the pattern
 * specified. This will verify if a filename does indeed match the pattern we asked for.
 * @remarks Based on algorithm (http://xoomer.virgilio.it/acantato/dev/wildcard/wildmatch.html)
 */
static bool MatchWildcard(const utf8 * fileName, const utf8 * pattern)
{
    while (*fileName != '\0')
    {
        switch (*pattern) {
        case '?':
            if (*fileName == '.')
            {
                return false;
            }
            break;
        case '*':
            do
            {
                pattern++;
            }
            while (*pattern == '*');
            if (*pattern == '\0')
            {
                return false;
            }
            while (*fileName != '\0')
            {
                if (MatchWildcard(fileName++, pattern))
                {
                    return true;
                }
            }
            return false;
        default:
            if (toupper(*fileName) != toupper(*pattern))
            {
                return false;
            }
            break;
        }
        pattern++;
        fileName++;
    }
    while (*pattern == '*')
    {
        ++fileName;
    }
    return *pattern == '\0';
}
