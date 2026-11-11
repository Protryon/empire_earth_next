// Platform layer: the configuration file config.yaml (include/platform/PlatConfig.h). One implementation on every
// system: C stdio for the file, the platform layer's data root for its place. The whole file is kept in memory as a
// tree of blocks (loaded on first use) and rewritten after every change through "<file>.new", which then replaces it.
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

// A key: a value, or a block of keys
struct Node {
    std::string mName;
    std::string mText;              // a value: as the game set it (unquoted)
    bool mBlock = false;
    bool mPlain = false;            // a value set as a number or boolean: never quoted
    std::vector<Node> mChildren;    // a block's keys in their first-written order
};

std::mutex sLock;
bool sLoaded;
std::string sPath;                  // the file in use
Node sRoot;                         // the top level (a block)

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

// The data root's config.yaml (native path)
std::string PrimaryPath()
{
    char theRoot[1024], theNative[1024];
    if (!PlatPathGetDataRoot(theRoot, sizeof(theRoot)) || !PlatPathToNative(theRoot, theNative, sizeof(theNative)))
        return "config.yaml";
    std::string thePath = theNative;
    if (!thePath.empty() && thePath.back() != '/' && thePath.back() != '\\')
        thePath += kSeparator;
    return thePath + "config.yaml";
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
    if (!theUser.empty() && Exists(theUser + kSeparator + "config.yaml"))
        return theUser + kSeparator + "config.yaml";
    return thePrimary;
}

// ---- the tree

Node* FindChild(Node& inBlock, const char* inName, size_t inLength)
{
    for (Node& theChild : inBlock.mChildren)
        if (theChild.mName.size() == inLength && SameNoCase(theChild.mName, std::string(inName, inLength).c_str()))
            return &theChild;
    return NULL;
}

// The block at inPath ("a.b"; NULL or "" the top level); inCreate makes the missing blocks (a value in the way is
// replaced by a block). NULL when it is missing (or a part of the path is empty).
Node* FindBlock(const char* inPath, bool inCreate)
{
    Node* theBlock = &sRoot;
    if (!inPath)
        return theBlock;
    const char* p = inPath;
    while (*p) {
        const char* theEnd = strchr(p, '.');
        size_t theLength = theEnd ? static_cast<size_t>(theEnd - p) : strlen(p);
        if (theLength == 0)
            return NULL;
        Node* theChild = FindChild(*theBlock, p, theLength);
        if (!theChild) {
            if (!inCreate)
                return NULL;
            theBlock->mChildren.push_back(Node());
            theChild = &theBlock->mChildren.back();
            theChild->mName.assign(p, theLength);
        }
        if (!theChild->mBlock) {
            if (!inCreate)
                return NULL;
            theChild->mBlock = true;
            theChild->mText.clear();
        }
        theBlock = theChild;
        p += theLength;
        if (*p == '.')
            p++;
    }
    return theBlock;
}

Node* FindValue(const char* inPath, const char* inKey)
{
    Node* theBlock = FindBlock(inPath, false);
    Node* theValue = theBlock && inKey ? FindChild(*theBlock, inKey, strlen(inKey)) : NULL;
    return theValue && !theValue->mBlock ? theValue : NULL;
}

// ---- the text format

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

bool SameWordNoCase(const std::string& inText, const char* const* inWords)
{
    for (; *inWords; inWords++)
        if (SameNoCase(inText, *inWords))
            return true;
    return false;
}

const char* const kTrueWords[] = {"true", "yes", "on", NULL};
const char* const kFalseWords[] = {"false", "no", "off", NULL};
const char* const kNullWords[] = {"null", "~", NULL};

// The text after "key:" -> the value; false for a malformed quoted string. A comment (" #" outside quotes) ends it.
bool ParseValue(const std::string& inText, std::string& outValue)
{
    std::string theText = Trim(inText);
    outValue.clear();
    if (theText.empty())
        return true;
    if (theText[0] == '"' || theText[0] == '\'') {
        char theQuote = theText[0];
        size_t i = 1;
        for (; i < theText.size(); i++) {
            char c = theText[i];
            if (theQuote == '\'') {
                if (c == '\'') {
                    if (i + 1 < theText.size() && theText[i + 1] == '\'') {
                        outValue += '\'';
                        i++;
                        continue;
                    }
                    break;
                }
                outValue += c;
                continue;
            }
            if (c == '"')
                break;
            if (c != '\\') {
                outValue += c;
                continue;
            }
            if (++i >= theText.size())
                return false;
            c = theText[i];
            if (c == 'n')
                outValue += '\n';
            else if (c == 'r')
                outValue += '\r';
            else if (c == 't')
                outValue += '\t';
            else if (c == '0')
                outValue += '\0';
            else if (c == 'x' && i + 2 < theText.size() && HexDigit(theText[i + 1]) >= 0 && HexDigit(theText[i + 2]) >= 0) {
                outValue += static_cast<char>(HexDigit(theText[i + 1]) * 16 + HexDigit(theText[i + 2]));
                i += 2;
            } else
                outValue += c;   // \" \\ \/ and any other escaped character: itself
        }
        if (i >= theText.size())
            return false;   // no closing quote
        std::string theRest = Trim(theText.substr(i + 1));
        return theRest.empty() || theRest[0] == '#';
    }
    size_t theComment = theText.find(" #");
    size_t theTab = theText.find("\t#");
    if (theTab < theComment)
        theComment = theTab;
    if (theComment != std::string::npos)
        theText = Trim(theText.substr(0, theComment));
    if (!SameWordNoCase(theText, kNullWords))
        outValue = theText;
    return true;
}

// Whether a string reads back as itself without quotes
bool IsPlain(const std::string& inValue)
{
    if (inValue.empty() || inValue[0] == ' ' || inValue[0] == '\t' || inValue.back() == ' ' || inValue.back() == '\t')
        return false;
    if (strchr("-?:,[]{}#&*!|>'\"%@`", inValue[0]) || (inValue[0] >= '0' && inValue[0] <= '9') || inValue[0] == '+'
        || inValue[0] == '.')
        return false;
    if (SameWordNoCase(inValue, kTrueWords) || SameWordNoCase(inValue, kFalseWords) || SameWordNoCase(inValue, kNullWords))
        return false;
    if (inValue.find(": ") != std::string::npos || inValue.find(" #") != std::string::npos || inValue.back() == ':')
        return false;
    for (unsigned char c : inValue)
        if (c < 0x20 || c == 0x7f)
            return false;
    return true;
}

std::string Quote(const std::string& inValue)
{
    if (IsPlain(inValue))
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

// One line: its indent (-1 for a line to skip: blank, comment, a tab indent, no "key:"), its key and the value text
int SplitLine(const std::string& inLine, std::string& outKey, std::string& outRest)
{
    size_t theIndent = 0;
    while (theIndent < inLine.size() && inLine[theIndent] == ' ')
        theIndent++;
    if (theIndent >= inLine.size() || inLine[theIndent] == '#' || inLine[theIndent] == '\t')
        return -1;
    if (inLine.compare(theIndent, 3, "---") == 0 || inLine.compare(theIndent, 3, "...") == 0)
        return -1;   // document markers
    // the key ends at the first ':' followed by a blank or the end of the line
    size_t theColon = theIndent;
    for (;; theColon++) {
        theColon = inLine.find(':', theColon);
        if (theColon == std::string::npos)
            return -1;
        if (theColon + 1 == inLine.size() || inLine[theColon + 1] == ' ' || inLine[theColon + 1] == '\t')
            break;
    }
    outKey = Trim(inLine.substr(theIndent, theColon - theIndent));
    std::string theUnquoted;
    if (outKey.size() >= 2 && (outKey[0] == '"' || outKey[0] == '\'') && ParseValue(outKey, theUnquoted))
        outKey = theUnquoted;
    if (outKey.empty())
        return -1;
    outRest = inLine.substr(theColon + 1);
    return static_cast<int>(theIndent);
}

void Load()
{
    if (sLoaded)
        return;
    sLoaded = true;
    sPath = ChoosePath();
    sRoot = Node();
    sRoot.mBlock = true;
    FILE* theFile = fopen(sPath.c_str(), "rb");
    if (!theFile)
        return;
    // the open blocks, outermost first: their indents (the top level -1)
    std::vector<std::pair<int, Node*>> theStack;
    theStack.push_back(std::make_pair(-1, &sRoot));
    std::string theLine;
    int c;
    bool theFirst = true;
    do {
        c = fgetc(theFile);
        if (c != '\n' && c != EOF) {
            if (c != '\r')
                theLine += static_cast<char>(c);
            continue;
        }
        if (theFirst && theLine.compare(0, 3, "\xef\xbb\xbf") == 0)
            theLine.erase(0, 3);   // a UTF-8 byte order mark
        theFirst = false;
        std::string theKey, theRest;
        int theIndent = SplitLine(theLine, theKey, theRest);
        theLine.clear();
        if (theIndent < 0)
            continue;
        while (theStack.back().first >= theIndent)
            theStack.pop_back();
        Node& theBlock = *theStack.back().second;
        std::string theTrimmed = Trim(theRest);
        Node* theNode = FindChild(theBlock, theKey.c_str(), theKey.size());
        if (theTrimmed.empty() || theTrimmed[0] == '#') {   // "key:" opens a block (a value of that name is replaced)
            if (!theNode) {
                theBlock.mChildren.push_back(Node());
                theNode = &theBlock.mChildren.back();
                theNode->mName = theKey;
            }
            theNode->mBlock = true;
            theNode->mText.clear();
            theStack.push_back(std::make_pair(theIndent, theNode));
            continue;
        }
        std::string theValue;
        if (!ParseValue(theRest, theValue))
            continue;
        if (!theNode) {
            theBlock.mChildren.push_back(Node());
            theNode = &theBlock.mChildren.back();
            theNode->mName = theKey;
        }
        theNode->mBlock = false;
        theNode->mChildren.clear();
        theNode->mText = theValue;
        theNode->mPlain = theTrimmed[0] != '"' && theTrimmed[0] != '\'';
    } while (c != EOF);
    fclose(theFile);
}

void WriteBlock(FILE* inFile, const Node& inBlock, int inIndent)
{
    for (const Node& theChild : inBlock.mChildren) {
        if (theChild.mBlock && theChild.mChildren.empty())
            continue;   // an empty block: dropped
        if (inIndent == 0 && theChild.mBlock && &theChild != &inBlock.mChildren.front())
            fputc('\n', inFile);   // a blank line before every top-level block
        fprintf(inFile, "%*s%s:", inIndent, "", IsPlain(theChild.mName) ? theChild.mName.c_str() : Quote(theChild.mName).c_str());
        if (theChild.mBlock) {
            fputc('\n', inFile);
            WriteBlock(inFile, theChild, inIndent + 2);
        } else
            fprintf(inFile, " %s\n", theChild.mPlain ? theChild.mText.c_str() : Quote(theChild.mText).c_str());
    }
}

bool WriteTo(const std::string& inPath)
{
    std::string theTemporary = inPath + ".new";
    FILE* theFile = fopen(theTemporary.c_str(), "wb");
    if (!theFile)
        return false;
    fputs("# Empire Earth AoC settings. The game rewrites this file whenever a setting changes (comments are not"
          " kept).\n", theFile);
    WriteBlock(theFile, sRoot, 0);
    bool theWritten = fflush(theFile) == 0;
    theWritten = fclose(theFile) == 0 && theWritten;
#ifdef _WIN32
    // Another process (a virus scanner, the indexer, a test harness reading the settings) may hold the file for a
    // moment; a failed replace would otherwise move the settings to the user's directory
    bool theMoved = false;
    for (int theTry = 0; theWritten && !theMoved && theTry < 10; theTry++) {
        if (theTry)
            Sleep(50);
        theMoved = MoveFileExA(theTemporary.c_str(), inPath.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
    }
    theWritten = theWritten && theMoved;
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
    std::string theFallback = theUser + kSeparator + "config.yaml";
    if (theFallback == sPath || !WriteTo(theFallback))
        return false;
    sPath = theFallback;
    return true;
}

bool ValidKey(const char* inKey)
{
    if (!inKey || !*inKey || strchr(inKey, '.') || inKey[0] == ' ' || inKey[strlen(inKey) - 1] == ' ')
        return false;
    for (const char* p = inKey; *p; p++)
        if (static_cast<unsigned char>(*p) < 0x20 || (p[0] == ':' && (p[1] == ' ' || !p[1])))
            return false;
    return true;
}

int Set(const char* inBlock, const char* inKey, const std::string& inText, bool inPlain)
{
    if (!ValidKey(inKey))
        return 0;
    std::lock_guard<std::mutex> theLock(sLock);
    Load();
    Node* theBlock = FindBlock(inBlock, true);
    if (!theBlock)
        return 0;
    Node* theValue = FindChild(*theBlock, inKey, strlen(inKey));
    if (theValue && !theValue->mBlock) {
        if (theValue->mText == inText && theValue->mPlain == inPlain)
            return 1;   // unchanged: no rewrite
    } else if (!theValue) {
        theBlock->mChildren.push_back(Node());
        theValue = &theBlock->mChildren.back();
        theValue->mName = inKey;
    }
    theValue->mBlock = false;
    theValue->mChildren.clear();
    theValue->mText = inText;
    theValue->mPlain = inPlain;
    return Save() ? 1 : 0;
}

}   // namespace

extern "C" int PlatConfigGetString(const char* inBlock, const char* inKey, char* outData, uint32_t inSize)
{
    if (!inKey || !outData || inSize == 0)
        return 0;
    std::lock_guard<std::mutex> theLock(sLock);
    Load();
    Node* theValue = FindValue(inBlock, inKey);
    if (!theValue || theValue->mText.size() + 1 > inSize)
        return 0;
    memcpy(outData, theValue->mText.c_str(), theValue->mText.size() + 1);
    return 1;
}

extern "C" int PlatConfigGetInt(const char* inBlock, const char* inKey, int32_t* outValue)
{
    char theText[64];
    if (!outValue || !PlatConfigGetString(inBlock, inKey, theText, sizeof(theText)))
        return 0;
    if (SameWordNoCase(theText, kTrueWords) || SameWordNoCase(theText, kFalseWords)) {
        *outValue = SameWordNoCase(theText, kTrueWords) ? 1 : 0;
        return 1;
    }
    char* theEnd;
    errno = 0;
    long long theValue = strtoll(theText, &theEnd, 0);
    if (theEnd == theText || *theEnd || errno || theValue < -2147483648LL || theValue > 4294967295LL)
        return 0;
    *outValue = static_cast<int32_t>(static_cast<uint32_t>(theValue));   // 0x80000000..0xffffffff: the bit pattern
    return 1;
}

extern "C" int PlatConfigSetString(const char* inBlock, const char* inKey, const char* inValue)
{
    return Set(inBlock, inKey, inValue ? inValue : "", false);
}

extern "C" int PlatConfigSetInt(const char* inBlock, const char* inKey, int32_t inValue)
{
    char theText[16];
    snprintf(theText, sizeof(theText), "%d", static_cast<int>(inValue));
    return Set(inBlock, inKey, theText, true);
}

extern "C" int PlatConfigSetBool(const char* inBlock, const char* inKey, int inValue)
{
    return Set(inBlock, inKey, inValue ? "true" : "false", true);
}

extern "C" int PlatConfigDelete(const char* inBlock, const char* inKey)
{
    std::lock_guard<std::mutex> theLock(sLock);
    Load();
    if (!inKey) {   // the whole block
        if (!inBlock || !*inBlock)
            return 0;
        const char* theLast = strrchr(inBlock, '.');
        std::string theParentPath = theLast ? std::string(inBlock, theLast) : std::string();
        const char* theName = theLast ? theLast + 1 : inBlock;
        Node* theParent = FindBlock(theParentPath.c_str(), false);
        Node* theNode = theParent ? FindChild(*theParent, theName, strlen(theName)) : NULL;
        if (!theNode || !theNode->mBlock)
            return 0;
        theParent->mChildren.erase(theParent->mChildren.begin() + (theNode - &theParent->mChildren[0]));
        return Save() ? 1 : 0;
    }
    Node* theBlock = FindBlock(inBlock, false);
    Node* theValue = theBlock ? FindChild(*theBlock, inKey, strlen(inKey)) : NULL;
    if (!theValue || theValue->mBlock)
        return 0;
    theBlock->mChildren.erase(theBlock->mChildren.begin() + (theValue - &theBlock->mChildren[0]));
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
