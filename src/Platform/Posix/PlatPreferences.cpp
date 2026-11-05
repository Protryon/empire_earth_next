// Platform layer, POSIX: the preferences store in files (include/platform/PlatPreferences.h).
//
// One text file per root in the user's configuration directory: $EE_PREFS_DIR when set, else
// $XDG_CONFIG_HOME/empire-earth-aoc (~/.config/empire-earth-aoc) on Linux and
// ~/Library/Application Support/Empire Earth AoC on macOS. The file of HKEY_CURRENT_USER is
// HKEY_CURRENT_USER.prefs, and so on. Its format (an INI file, written whole after every change through a
// temporary file and a rename):
//
//     [Software\SSSI\Empire Earth]
//     "Name" = string "text with \"escapes\" \\ \x01 and \0"
//     "Width" = int32 1024
//
// Keys and value names are matched ignoring ASCII case and keep the case they were created with, as in the
// registry. Creating a key creates its parents; a key with subkeys cannot be deleted; values go into existing keys
// only. Failures set the Win32 codes the registry reports (ERROR_FILE_NOT_FOUND for a missing key or value,
// ERROR_MORE_DATA for a buffer too small, ERROR_ACCESS_DENIED for a key with subkeys).
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mutex>
#include <string>
#include <vector>

#include "platform/PlatPreferences.h"
#include "PlatPosix.h"

namespace {

enum ValueType { kString, kInt32 };

struct Value {
    std::string mName;
    ValueType mType;
    std::string mData;              // the bytes (a string's bytes as stored; an int32's 4 bytes, little-endian)
};

struct Key {
    std::string mPath;              // normalised: no leading, trailing or doubled backslashes
    std::vector<Value> mValues;
};

struct Store {
    bool mLoaded;
    std::vector<Key> mKeys;
};

std::mutex sLock;
Store sStores[kPlatPrefRootCount];

const char* const kRootNames[kPlatPrefRootCount] = {"HKEY_CLASSES_ROOT", "HKEY_CURRENT_USER",
                                                    "HKEY_LOCAL_MACHINE", "HKEY_USERS",
                                                    "HKEY_CURRENT_CONFIG", "HKEY_DYN_DATA"};

bool SameNoCase(const std::string& a, const std::string& b)
{
    return a.size() == b.size() && PlatPosix::CompareNoCase(a.c_str(), b.c_str()) == 0;
}

std::string Normalise(const char* inKey)
{
    std::string theKey;
    for (const char* p = inKey ? inKey : ""; *p; p++) {
        if (*p == '\\' || *p == '/') {
            if (!theKey.empty() && theKey.back() != '\\')
                theKey += '\\';
        } else {
            theKey += *p;
        }
    }
    while (!theKey.empty() && theKey.back() == '\\')
        theKey.pop_back();
    return theKey;
}

bool MakeDirectories(const std::string& inPath)
{
    std::string thePrefix;
    for (size_t i = 0; i <= inPath.size(); i++) {
        if (i == inPath.size() || (inPath[i] == '/' && i > 0)) {
            thePrefix = inPath.substr(0, i);
            if (mkdir(thePrefix.c_str(), 0700) != 0 && errno != EEXIST)
                return false;
        }
    }
    return true;
}

std::string Directory()
{
    const char* theEnv = getenv("EE_PREFS_DIR");
    if (theEnv && *theEnv)
        return theEnv;
    const char* theHome = getenv("HOME");
    std::string theBase = theHome && *theHome ? theHome : "/tmp";
#if defined(__APPLE__)
    return theBase + "/Library/Application Support/Empire Earth AoC";
#else
    const char* theConfig = getenv("XDG_CONFIG_HOME");
    if (theConfig && *theConfig == '/')
        return std::string(theConfig) + "/empire-earth-aoc";
    return theBase + "/.config/empire-earth-aoc";
#endif
}

std::string FileOf(PlatPrefRoot inRoot)
{
    return Directory() + "/" + kRootNames[inRoot] + ".prefs";
}

// ---- the text format

std::string Quote(const std::string& inText)
{
    static const char kHex[] = "0123456789abcdef";
    std::string theOut = "\"";
    for (unsigned char c : inText) {
        if (c == '"' || c == '\\') {
            theOut += '\\';
            theOut += static_cast<char>(c);
        } else if (c == 0) {
            theOut += "\\0";
        } else if (c < 0x20 || c >= 0x7F) {
            theOut += "\\x";
            theOut += kHex[c >> 4];
            theOut += kHex[c & 15];
        } else {
            theOut += static_cast<char>(c);
        }
    }
    return theOut + "\"";
}

int HexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// Reads a quoted string at p -> the position after it, NULL when malformed
const char* Unquote(const char* p, std::string& outText)
{
    if (*p != '"')
        return NULL;
    outText.clear();
    for (p++; *p && *p != '"'; p++) {
        if (*p != '\\') {
            outText += *p;
            continue;
        }
        p++;
        if (*p == '0') {
            outText += '\0';
        } else if (*p == 'x' && HexDigit(p[1]) >= 0 && HexDigit(p[2]) >= 0) {
            outText += static_cast<char>(HexDigit(p[1]) * 16 + HexDigit(p[2]));
            p += 2;
        } else if (*p) {
            outText += *p;
        } else {
            return NULL;
        }
    }
    return *p == '"' ? p + 1 : NULL;
}

const char* SkipSpaces(const char* p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

void Load(PlatPrefRoot inRoot)
{
    Store& theStore = sStores[inRoot];
    if (theStore.mLoaded)
        return;
    theStore.mLoaded = true;
    FILE* theFile = fopen(FileOf(inRoot).c_str(), "rb");
    if (!theFile)
        return;
    std::string theLine;
    Key* theKey = NULL;
    int c;
    do {
        c = fgetc(theFile);
        if (c != '\n' && c != EOF) {
            if (c != '\r')
                theLine += static_cast<char>(c);
            continue;
        }
        const char* p = SkipSpaces(theLine.c_str());
        if (*p == '[') {
            size_t theEnd = theLine.rfind(']');
            size_t theStart = static_cast<size_t>(p - theLine.c_str()) + 1;
            if (theEnd != std::string::npos && theEnd >= theStart) {
                Key theNew;
                theNew.mPath = Normalise(theLine.substr(theStart, theEnd - theStart).c_str());
                theStore.mKeys.push_back(theNew);
                theKey = &theStore.mKeys.back();
            }
        } else if (*p == '"' && theKey) {
            Value theValue;
            p = Unquote(p, theValue.mName);
            if (p) {
                p = SkipSpaces(p);
                if (*p == '=') {
                    p = SkipSpaces(p + 1);
                    if (strncmp(p, "string", 6) == 0) {
                        theValue.mType = kString;
                        if (Unquote(SkipSpaces(p + 6), theValue.mData))
                            theKey->mValues.push_back(theValue);
                    } else if (strncmp(p, "int32", 5) == 0) {
                        theValue.mType = kInt32;
                        int32_t theInt = static_cast<int32_t>(strtol(SkipSpaces(p + 5), NULL, 0));
                        uint32_t u = static_cast<uint32_t>(theInt);
                        char theBytes[4] = {static_cast<char>(u), static_cast<char>(u >> 8),
                                            static_cast<char>(u >> 16), static_cast<char>(u >> 24)};
                        theValue.mData.assign(theBytes, 4);
                        theKey->mValues.push_back(theValue);
                    }
                }
            }
        }
        theLine.clear();
    } while (c != EOF);
    fclose(theFile);
}

int32_t Int32Of(const std::string& inData)
{
    uint32_t u = 0;
    for (size_t i = 0; i < 4 && i < inData.size(); i++)
        u |= static_cast<uint32_t>(static_cast<unsigned char>(inData[i])) << (8 * i);
    return static_cast<int32_t>(u);
}

bool Save(PlatPrefRoot inRoot)
{
    std::string theDirectory = Directory();
    if (!MakeDirectories(theDirectory))
        return PlatPosix::FailErrno();
    std::string thePath = FileOf(inRoot);
    std::string theTemporary = thePath + ".new";
    FILE* theFile = fopen(theTemporary.c_str(), "wb");
    if (!theFile)
        return PlatPosix::FailErrno();
    for (const Key& theKey : sStores[inRoot].mKeys) {
        fprintf(theFile, "[%s]\n", theKey.mPath.c_str());
        for (const Value& theValue : theKey.mValues) {
            if (theValue.mType == kInt32)
                fprintf(theFile, "%s = int32 %d\n", Quote(theValue.mName).c_str(), Int32Of(theValue.mData));
            else
                fprintf(theFile, "%s = string %s\n", Quote(theValue.mName).c_str(), Quote(theValue.mData).c_str());
        }
        fputc('\n', theFile);
    }
    bool theWritten = fflush(theFile) == 0;
    theWritten = fclose(theFile) == 0 && theWritten;
    if (!theWritten || rename(theTemporary.c_str(), thePath.c_str()) != 0) {
        PlatPosix::FailErrno();
        unlink(theTemporary.c_str());
        return false;
    }
    return true;
}

bool ValidRoot(PlatPrefRoot inRoot)
{
    if (static_cast<int>(inRoot) < 0 || inRoot >= kPlatPrefRootCount) {
        PlatPosix::SetLastError(PlatPosix::kErrorInvalidHandle);
        return false;
    }
    Load(inRoot);
    return true;
}

Key* FindKey(PlatPrefRoot inRoot, const std::string& inPath)
{
    for (Key& theKey : sStores[inRoot].mKeys)
        if (SameNoCase(theKey.mPath, inPath))
            return &theKey;
    return NULL;
}

Value* FindValue(Key& inKey, const char* inName)
{
    std::string theName = inName ? inName : "";
    for (Value& theValue : inKey.mValues)
        if (SameNoCase(theValue.mName, theName))
            return &theValue;
    return NULL;
}

// The existing key inKey under inRoot (the root itself for an empty path), NULL with ERROR_FILE_NOT_FOUND
Key* OpenKey(PlatPrefRoot inRoot, const char* inKey)
{
    std::string thePath = Normalise(inKey);
    Key* theKey = FindKey(inRoot, thePath);
    if (!theKey && thePath.empty()) {
        Key theRoot;
        sStores[inRoot].mKeys.insert(sStores[inRoot].mKeys.begin(), theRoot);
        theKey = &sStores[inRoot].mKeys.front();
    }
    if (!theKey)
        PlatPosix::SetLastError(PlatPosix::kErrorFileNotFound);
    return theKey;
}

int SetValue(PlatPrefRoot inRoot, const char* inKey, const char* inName, ValueType inType, const std::string& inData)
{
    std::lock_guard<std::mutex> theLock(sLock);
    if (!ValidRoot(inRoot))
        return 0;
    Key* theKey = OpenKey(inRoot, inKey);
    if (!theKey)
        return 0;
    Value* theValue = FindValue(*theKey, inName);
    if (!theValue) {
        Value theNew;
        theNew.mName = inName ? inName : "";
        theKey->mValues.push_back(theNew);
        theValue = &theKey->mValues.back();
    }
    theValue->mType = inType;
    theValue->mData = inData;
    return Save(inRoot) ? 1 : 0;
}

}   // namespace

extern "C" int PlatPrefCreateKey(PlatPrefRoot inRoot, const char* inKey)
{
    std::lock_guard<std::mutex> theLock(sLock);
    if (!ValidRoot(inRoot))
        return 0;
    std::string thePath = Normalise(inKey);
    if (thePath.empty())
        return 1;
    // the key and every parent it lacks
    bool theChanged = false;
    for (size_t i = 0; i <= thePath.size(); i++) {
        if (i < thePath.size() && thePath[i] != '\\')
            continue;
        std::string thePrefix = thePath.substr(0, i);
        if (!FindKey(inRoot, thePrefix)) {
            Key theNew;
            theNew.mPath = thePrefix;
            sStores[inRoot].mKeys.push_back(theNew);
            theChanged = true;
        }
    }
    return !theChanged || Save(inRoot) ? 1 : 0;
}

extern "C" int PlatPrefDeleteKey(PlatPrefRoot inRoot, const char* inKey)
{
    std::lock_guard<std::mutex> theLock(sLock);
    if (!ValidRoot(inRoot))
        return 0;
    std::string thePath = Normalise(inKey);
    if (thePath.empty())
        return PlatPosix::Fail(PlatPosix::kErrorAccessDenied);
    std::vector<Key>& theKeys = sStores[inRoot].mKeys;
    size_t theIndex = theKeys.size();
    for (size_t i = 0; i < theKeys.size(); i++) {
        const std::string& theOther = theKeys[i].mPath;
        if (SameNoCase(theOther, thePath))
            theIndex = i;
        else if (theOther.size() > thePath.size() && theOther[thePath.size()] == '\\'
                 && SameNoCase(theOther.substr(0, thePath.size()), thePath))
            return PlatPosix::Fail(PlatPosix::kErrorAccessDenied);   // has subkeys
    }
    if (theIndex == theKeys.size())
        return PlatPosix::Fail(PlatPosix::kErrorFileNotFound);
    theKeys.erase(theKeys.begin() + static_cast<long>(theIndex));
    return Save(inRoot) ? 1 : 0;
}

extern "C" int PlatPrefDeleteValue(PlatPrefRoot inRoot, const char* inKey, const char* inName)
{
    std::lock_guard<std::mutex> theLock(sLock);
    if (!ValidRoot(inRoot))
        return 0;
    Key* theKey = OpenKey(inRoot, inKey);
    if (!theKey)
        return 0;
    Value* theValue = FindValue(*theKey, inName);
    if (!theValue)
        return PlatPosix::Fail(PlatPosix::kErrorFileNotFound);
    theKey->mValues.erase(theKey->mValues.begin() + (theValue - &theKey->mValues[0]));
    return Save(inRoot) ? 1 : 0;
}

extern "C" int PlatPrefSetString(PlatPrefRoot inRoot, const char* inKey, const char* inName, const char* inData,
                                 uint32_t inSize)
{
    return SetValue(inRoot, inKey, inName, kString, std::string(inData ? inData : "", inData ? inSize : 0));
}

extern "C" int PlatPrefSetInt32(PlatPrefRoot inRoot, const char* inKey, const char* inName, int32_t inValue)
{
    uint32_t u = static_cast<uint32_t>(inValue);
    char theBytes[4] = {static_cast<char>(u), static_cast<char>(u >> 8), static_cast<char>(u >> 16),
                        static_cast<char>(u >> 24)};
    return SetValue(inRoot, inKey, inName, kInt32, std::string(theBytes, 4));
}

extern "C" int PlatPrefGetInt32(PlatPrefRoot inRoot, const char* inKey, const char* inName, int32_t* outValue)
{
    std::lock_guard<std::mutex> theLock(sLock);
    if (!ValidRoot(inRoot))
        return 0;
    Key* theKey = OpenKey(inRoot, inKey);
    if (!theKey)
        return 0;
    Value* theValue = FindValue(*theKey, inName);
    if (!theValue)
        return PlatPosix::Fail(PlatPosix::kErrorFileNotFound);
    if (theValue->mData.size() > 4)
        return PlatPosix::Fail(PlatPosix::kErrorMoreData);   // as RegQueryValueEx with a 4-byte buffer
    *outValue = Int32Of(theValue->mData);
    return 1;
}

extern "C" int PlatPrefGetString(PlatPrefRoot inRoot, const char* inKey, const char* inName, char* outData,
                                 uint32_t* ioSize)
{
    std::lock_guard<std::mutex> theLock(sLock);
    if (!ValidRoot(inRoot))
        return 0;
    Key* theKey = OpenKey(inRoot, inKey);
    if (!theKey)
        return 0;
    Value* theValue = FindValue(*theKey, inName);
    if (!theValue)
        return PlatPosix::Fail(PlatPosix::kErrorFileNotFound);
    if (theValue->mData.size() > *ioSize) {
        *ioSize = static_cast<uint32_t>(theValue->mData.size());
        return PlatPosix::Fail(PlatPosix::kErrorMoreData);
    }
    memcpy(outData, theValue->mData.data(), theValue->mData.size());
    *ioSize = static_cast<uint32_t>(theValue->mData.size());
    return 1;
}
