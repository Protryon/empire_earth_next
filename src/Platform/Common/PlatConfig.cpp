// Platform layer: the configuration file config.ini (include/platform/PlatConfig.h). One implementation on every
// system: C stdio for the file, the platform layer's data root for its place. The whole file is kept in memory
// (loaded on first use) and rewritten after every change through "<file>.new", which then replaces it.
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "platform/PlatConfig.h"
#include "platform/PlatPath.h"

namespace {

struct Value {
    std::string mName;
    std::string mText;              // the value as the game set it (unquoted)
};

struct Section {
    std::string mName;
    std::vector<Value> mValues;
};

std::mutex sLock;
bool sLoaded;
std::string sPath;                  // the file in use
std::vector<Section> sSections;

#ifdef _WIN32
const char kSeparator = '\\';
#else
const char kSeparator = '/';
#endif

bool SameNoCase(const std::string& a, const char* b)
{
    size_t n = strlen(b);
    if (a.size() != n)
        return false;
    for (size_t i = 0; i < n; i++) {
        unsigned char x = static_cast<unsigned char>(a[i]), y = static_cast<unsigned char>(b[i]);
        if (x >= 'a' && x <= 'z')
            x -= 32;
        if (y >= 'a' && y <= 'z')
            y -= 32;
        if (x != y)
            return false;
    }
    return true;
}

bool Exists(const std::string& inPath)
{
    FILE* f = fopen(inPath.c_str(), "rb");
    if (!f)
        return false;
    fclose(f);
    return true;
}

// The data root's config.ini (native path)
std::string PrimaryPath()
{
    char theRoot[1024], theNative[1024];
    if (!PlatPathGetDataRoot(theRoot, sizeof(theRoot)) || !PlatPathToNative(theRoot, theNative, sizeof(theNative)))
        return "config.ini";
    std::string thePath = theNative;
    if (!thePath.empty() && thePath.back() != '/' && thePath.back() != '\\')
        thePath += kSeparator;
    return thePath + "config.ini";
}

// The user's configuration directory (native path, no trailing separator; empty when unknown)
std::string UserDirectory()
{
#ifdef _WIN32
    const char* theBase = getenv("APPDATA");
    return theBase && *theBase ? std::string(theBase) + "\\Empire Earth AoC" : std::string();
#else
    const char* theHome = getenv("HOME");
    std::string theBase = theHome && *theHome ? theHome : "";
#if defined(__APPLE__)
    return theBase.empty() ? theBase : theBase + "/Library/Application Support/Empire Earth AoC";
#else
    const char* theConfig = getenv("XDG_CONFIG_HOME");
    if (theConfig && *theConfig == '/')
        return std::string(theConfig) + "/empire-earth-aoc";
    return theBase.empty() ? theBase : theBase + "/.config/empire-earth-aoc";
#endif
#endif
}

bool MakeDirectories(const std::string& inPath)
{
    for (size_t i = 1; i <= inPath.size(); i++) {
        if (i < inPath.size() && inPath[i] != '/' && inPath[i] != '\\')
            continue;
        std::string thePrefix = inPath.substr(0, i);
        if (thePrefix.size() == 2 && thePrefix[1] == ':')
            continue;   // a Windows drive
#ifdef _WIN32
        if (_mkdir(thePrefix.c_str()) != 0 && errno != EEXIST)
            return false;
#else
        if (mkdir(thePrefix.c_str(), 0700) != 0 && errno != EEXIST)
            return false;
#endif
    }
    return true;
}

std::string ChoosePath()
{
    const char* theEnv = getenv("EE_CONFIG");
    if (theEnv && *theEnv)
        return theEnv;
    std::string thePrimary = PrimaryPath();
    if (Exists(thePrimary))
        return thePrimary;
    std::string theUser = UserDirectory();
    if (!theUser.empty() && Exists(theUser + kSeparator + "config.ini"))
        return theUser + kSeparator + "config.ini";
    return thePrimary;
}

// ---- the text format

const char* SkipBlanks(const char* p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

std::string Trim(const std::string& inText)
{
    size_t a = 0, b = inText.size();
    while (a < b && (inText[a] == ' ' || inText[a] == '\t'))
        a++;
    while (b > a && (inText[b - 1] == ' ' || inText[b - 1] == '\t'))
        b--;
    return inText.substr(a, b - a);
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

// A value's text as written -> the value; false for a malformed quoted string
bool Unquote(const std::string& inText, std::string& outValue)
{
    if (inText.empty() || inText[0] != '"') {
        outValue = inText;
        return true;
    }
    outValue.clear();
    size_t i = 1;
    for (; i < inText.size() && inText[i] != '"'; i++) {
        char c = inText[i];
        if (c != '\\') {
            outValue += c;
            continue;
        }
        if (++i >= inText.size())
            return false;
        c = inText[i];
        if (c == 'n')
            outValue += '\n';
        else if (c == 'r')
            outValue += '\r';
        else if (c == 't')
            outValue += '\t';
        else if (c == 'x' && i + 2 < inText.size() && HexDigit(inText[i + 1]) >= 0 && HexDigit(inText[i + 2]) >= 0) {
            outValue += static_cast<char>(HexDigit(inText[i + 1]) * 16 + HexDigit(inText[i + 2]));
            i += 2;
        } else
            outValue += c;   // \" \\ and any other escaped character: itself
    }
    return i + 1 == inText.size();   // the closing quote ends the value
}

std::string Quote(const std::string& inValue)
{
    bool theQuote = !inValue.empty() && (inValue[0] == '"' || inValue[0] == ' ' || inValue[0] == '\t'
                                         || inValue.back() == ' ' || inValue.back() == '\t');
    for (unsigned char c : inValue)
        if (c < 0x20 || c == 0x7f)
            theQuote = true;
    if (!theQuote)
        return inValue;
    static const char kHex[] = "0123456789abcdef";
    std::string theOut = "\"";
    for (unsigned char c : inValue) {
        if (c == '"' || c == '\\') {
            theOut += '\\';
            theOut += static_cast<char>(c);
        } else if (c == '\n')
            theOut += "\\n";
        else if (c == '\r')
            theOut += "\\r";
        else if (c == '\t')
            theOut += "\\t";
        else if (c < 0x20 || c == 0x7f) {
            theOut += "\\x";
            theOut += kHex[c >> 4];
            theOut += kHex[c & 15];
        } else
            theOut += static_cast<char>(c);
    }
    return theOut + "\"";
}

Section* FindSection(const char* inName)
{
    for (Section& theSection : sSections)
        if (SameNoCase(theSection.mName, inName))
            return &theSection;
    return NULL;
}

Value* FindValue(Section& inSection, const char* inName)
{
    for (Value& theValue : inSection.mValues)
        if (SameNoCase(theValue.mName, inName))
            return &theValue;
    return NULL;
}

void Load()
{
    if (sLoaded)
        return;
    sLoaded = true;
    sPath = ChoosePath();
    FILE* theFile = fopen(sPath.c_str(), "rb");
    if (!theFile)
        return;
    std::string theLine;
    Section* theSection = NULL;
    int c;
    do {
        c = fgetc(theFile);
        if (c != '\n' && c != EOF) {
            if (c != '\r')
                theLine += static_cast<char>(c);
            continue;
        }
        const char* p = SkipBlanks(theLine.c_str());
        if (*p == '[') {
            const char* theEnd = strrchr(p, ']');
            if (theEnd) {
                std::string theName = Trim(std::string(p + 1, theEnd));
                theSection = FindSection(theName.c_str());
                if (!theSection) {
                    sSections.push_back(Section{theName, {}});
                    theSection = &sSections.back();
                }
            }
        } else if (*p && *p != ';' && *p != '#' && theSection) {
            const char* theEquals = strchr(p, '=');
            std::string theValue;
            if (theEquals && Unquote(Trim(std::string(theEquals + 1)), theValue)) {
                std::string theName = Trim(std::string(p, theEquals));
                if (!theName.empty()) {
                    Value* theOld = FindValue(*theSection, theName.c_str());
                    if (theOld)
                        theOld->mText = theValue;
                    else
                        theSection->mValues.push_back(Value{theName, theValue});
                }
            }
        }
        theLine.clear();
    } while (c != EOF);
    fclose(theFile);
}

bool WriteTo(const std::string& inPath)
{
    std::string theTemporary = inPath + ".new";
    FILE* theFile = fopen(theTemporary.c_str(), "wb");
    if (!theFile)
        return false;
    bool theFirst = true;
    for (const Section& theSection : sSections) {
        fprintf(theFile, "%s[%s]\n", theFirst ? "" : "\n", theSection.mName.c_str());
        theFirst = false;
        for (const Value& theValue : theSection.mValues)
            fprintf(theFile, "%s = %s\n", theValue.mName.c_str(), Quote(theValue.mText).c_str());
    }
    bool theWritten = fflush(theFile) == 0;
    theWritten = fclose(theFile) == 0 && theWritten;
#ifdef _WIN32
    theWritten = theWritten && MoveFileExA(theTemporary.c_str(), inPath.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    theWritten = theWritten && rename(theTemporary.c_str(), inPath.c_str()) == 0;
#endif
    if (!theWritten)
        remove(theTemporary.c_str());
    return theWritten;
}

// Rewrites the file; when its directory cannot be written (and EE_CONFIG does not name it), moves to the user's
// configuration directory
bool Save()
{
    if (WriteTo(sPath))
        return true;
    const char* theEnv = getenv("EE_CONFIG");
    std::string theUser = UserDirectory();
    if ((theEnv && *theEnv) || theUser.empty() || !MakeDirectories(theUser))
        return false;
    std::string theFallback = theUser + kSeparator + "config.ini";
    if (theFallback == sPath || !WriteTo(theFallback))
        return false;
    sPath = theFallback;
    return true;
}

int Set(const char* inSection, const char* inName, const std::string& inText)
{
    if (!inSection || !inName || !*inName || strchr(inName, '=') || strchr(inSection, ']'))
        return 0;
    std::lock_guard<std::mutex> theLock(sLock);
    Load();
    Section* theSection = FindSection(inSection);
    if (!theSection) {
        sSections.push_back(Section{inSection, {}});
        theSection = &sSections.back();
    }
    Value* theValue = FindValue(*theSection, inName);
    if (theValue) {
        if (theValue->mText == inText)
            return 1;   // unchanged: no rewrite
        theValue->mText = inText;
    } else
        theSection->mValues.push_back(Value{inName, inText});
    return Save() ? 1 : 0;
}

}   // namespace

extern "C" int PlatConfigGetString(const char* inSection, const char* inName, char* outData, uint32_t inSize)
{
    if (!inSection || !inName || !outData || inSize == 0)
        return 0;
    std::lock_guard<std::mutex> theLock(sLock);
    Load();
    Section* theSection = FindSection(inSection);
    Value* theValue = theSection ? FindValue(*theSection, inName) : NULL;
    if (!theValue || theValue->mText.size() + 1 > inSize)
        return 0;
    memcpy(outData, theValue->mText.c_str(), theValue->mText.size() + 1);
    return 1;
}

extern "C" int PlatConfigGetInt(const char* inSection, const char* inName, int32_t* outValue)
{
    char theText[64];
    if (!outValue || !PlatConfigGetString(inSection, inName, theText, sizeof(theText)))
        return 0;
    char* theEnd;
    errno = 0;
    long long theValue = strtoll(theText, &theEnd, 0);
    if (theEnd == theText || *SkipBlanks(theEnd) || errno || theValue < -2147483648LL || theValue > 4294967295LL)
        return 0;
    *outValue = static_cast<int32_t>(static_cast<uint32_t>(theValue));   // 0x80000000..0xffffffff: the bit pattern
    return 1;
}

extern "C" int PlatConfigSetString(const char* inSection, const char* inName, const char* inValue)
{
    return Set(inSection, inName, inValue ? inValue : "");
}

extern "C" int PlatConfigSetInt(const char* inSection, const char* inName, int32_t inValue)
{
    char theText[16];
    snprintf(theText, sizeof(theText), "%d", static_cast<int>(inValue));
    return Set(inSection, inName, theText);
}

extern "C" int PlatConfigDelete(const char* inSection, const char* inName)
{
    if (!inSection)
        return 0;
    std::lock_guard<std::mutex> theLock(sLock);
    Load();
    Section* theSection = FindSection(inSection);
    if (!theSection)
        return 0;
    if (!inName) {
        sSections.erase(sSections.begin() + (theSection - &sSections[0]));
        return Save() ? 1 : 0;
    }
    Value* theValue = FindValue(*theSection, inName);
    if (!theValue)
        return 0;
    theSection->mValues.erase(theSection->mValues.begin() + (theValue - &theSection->mValues[0]));
    return Save() ? 1 : 0;
}

extern "C" uint32_t PlatConfigGetPath(char* outPath, uint32_t inSize)
{
    std::lock_guard<std::mutex> theLock(sLock);
    Load();
    if (!outPath || sPath.size() + 1 > inSize)
        return 0;
    memcpy(outPath, sPath.c_str(), sPath.size() + 1);
    return static_cast<uint32_t>(sPath.size());
}
