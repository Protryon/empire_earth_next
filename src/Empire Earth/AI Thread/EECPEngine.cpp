// The computer player's engine per computer seat and the AI manifest (ee/EECPEngine.h; docs/AI_LUA_PLAN.md §5 S0,
// "AI manifest"). New in this port: no original address.
#include <windows.h>
#include <vector>
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECPTrace.h"
#include "ee/EECPEngine.h"
#include "ee/EECPLua.h"
#include "ee/EECPNext.h"
#include "ee/EEGameLog.h"
#include "platform/PlatFile.h"
#include "platform/PlatSim.h"

// ---------------------------------------------------------------------------------------------------------------
// The log: "EE-AI engine ..." lines to the debug output, the game log when one is open, and EE-SIM's stderr

void EECPEngineLog(const char* inFormat, ...)
{
    char theText[900];
    va_list theArguments;
    va_start(theArguments, inFormat);
    vsnprintf(theText, sizeof(theText), inFormat, theArguments);
    va_end(theArguments);
    std::string theLine = std::string("EE-AI engine ") + theText + "\n";
    ESDebugOut("%s", theLine.c_str());
    if (EEGameLogOn())
        EEGameLogLine(theLine.c_str());
    if (gPlatSimMode) {
        fprintf(stderr, "SIM %s", theLine.c_str());
        fflush(stderr);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The files: EE_AI_SCRIPTS's folder (TEST ONLY, as EECPNextLua.cpp's reader) before Data\ai

static const std::string& EECPAIOverrideFolder()
{
    static std::string sDir;
    static bool sRead = false;
    if (!sRead) {
        sRead = true;
        const char* theValue = getenv("EE_AI_SCRIPTS");
        if (theValue != NULL && theValue[0] != '\0') {
            sDir = theValue;
            if (sDir[sDir.size() - 1] != '\\' && sDir[sDir.size() - 1] != '/')
                sDir += '\\';
        }
    }
    return sDir;
}

static std::string EECPAINative(std::string inPath)
{
#ifndef _WIN32
    std::replace(inPath.begin(), inPath.end(), '\\', '/');
#endif
    return inPath;
}

static bool EECPAIReadFile(const std::string& inPath, std::string& outText)
{
    FILE* f = PlatFOpen(EECPAINative(inPath).c_str(), "rb");
    if (f == NULL)
        return false;
    outText.clear();
    char theBuffer[4096];
    size_t n;
    while ((n = fread(theBuffer, 1, sizeof(theBuffer), f)) > 0)
        outText.append(theBuffer, n);
    bool theOK = !ferror(f);
    fclose(f);
    return theOK;
}

static bool EECPAIFileExists(const std::string& inPath)
{
    FILE* f = PlatFOpen(EECPAINative(inPath).c_str(), "rb");
    if (f == NULL)
        return false;
    fclose(f);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// A small JSON reader (RFC 8259: objects, arrays, strings with escapes, numbers, true, false, null)

namespace {

struct JValue {
    enum Kind { kNull, kBool, kNumber, kString, kArray, kObject } mKind = kNull;
    bool mBool = false;
    double mNumber = 0.0;
    std::string mString;
    std::vector<JValue> mItems;
    std::vector<std::pair<std::string, JValue>> mFields;
};

struct JReader {
    const std::string& mText;
    size_t mAt = 0;
    std::string mError;
    int mDepth = 0;

    explicit JReader(const std::string& inText) : mText(inText) {}

    bool Fail(const std::string& inWhat)
    {
        if (mError.empty()) {
            long theLine = 1;
            for (size_t i = 0; i < mAt && i < mText.size(); i++)
                theLine += mText[i] == '\n';
            mError = "line " + std::to_string(theLine) + ": " + inWhat;
        }
        return false;
    }
    void Space()
    {
        while (mAt < mText.size() && (mText[mAt] == ' ' || mText[mAt] == '\t' || mText[mAt] == '\r' || mText[mAt] == '\n'))
            mAt++;
    }
    bool Word(const char* inWord)
    {
        size_t n = strlen(inWord);
        if (mText.compare(mAt, n, inWord) != 0)
            return Fail("unexpected text");
        mAt += n;
        return true;
    }
    static void PutUTF8(std::string& out, uint32_t c)
    {
        if (c < 0x80) {
            out += (char)c;
        } else if (c < 0x800) {
            out += (char)(0xc0 | (c >> 6));
            out += (char)(0x80 | (c & 0x3f));
        } else if (c < 0x10000) {
            out += (char)(0xe0 | (c >> 12));
            out += (char)(0x80 | ((c >> 6) & 0x3f));
            out += (char)(0x80 | (c & 0x3f));
        } else {
            out += (char)(0xf0 | (c >> 18));
            out += (char)(0x80 | ((c >> 12) & 0x3f));
            out += (char)(0x80 | ((c >> 6) & 0x3f));
            out += (char)(0x80 | (c & 0x3f));
        }
    }
    bool Hex4(uint32_t& out)
    {
        out = 0;
        for (int i = 0; i < 4; i++, mAt++) {
            if (mAt >= mText.size())
                return Fail("unterminated \\u escape");
            char c = mText[mAt];
            out <<= 4;
            if (c >= '0' && c <= '9')
                out |= (uint32_t)(c - '0');
            else if (c >= 'a' && c <= 'f')
                out |= (uint32_t)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                out |= (uint32_t)(c - 'A' + 10);
            else
                return Fail("bad \\u escape");
        }
        return true;
    }
    bool String(std::string& out)
    {
        mAt++;   // "
        out.clear();
        while (true) {
            if (mAt >= mText.size())
                return Fail("unterminated string");
            unsigned char c = (unsigned char)mText[mAt++];
            if (c == '"')
                return true;
            if (c < 0x20)
                return Fail("a control character in a string");
            if (c != '\\') {
                out += (char)c;
                continue;
            }
            if (mAt >= mText.size())
                return Fail("unterminated string");
            char e = mText[mAt++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                uint32_t u;
                if (!Hex4(u))
                    return false;
                if (u >= 0xd800 && u < 0xdc00 && mText.compare(mAt, 2, "\\u") == 0) {
                    mAt += 2;
                    uint32_t v;
                    if (!Hex4(v))
                        return false;
                    if (v >= 0xdc00 && v < 0xe000)
                        u = 0x10000 + ((u - 0xd800) << 10) + (v - 0xdc00);
                }
                PutUTF8(out, u);
                break;
            }
            default:
                return Fail("bad escape in a string");
            }
        }
    }
    bool Number(JValue& out)
    {
        size_t theStart = mAt;
        if (mText[mAt] == '-')
            mAt++;
        while (mAt < mText.size() && strchr("0123456789.eE+-", mText[mAt]) != NULL)
            mAt++;
        std::string theText = mText.substr(theStart, mAt - theStart);
        char* theEnd = NULL;
        out.mKind = JValue::kNumber;
        out.mNumber = strtod(theText.c_str(), &theEnd);
        if (theText.empty() || theEnd == NULL || *theEnd != '\0')
            return Fail("bad number");
        return true;
    }
    bool Value(JValue& out)
    {
        Space();
        if (mAt >= mText.size())
            return Fail("unexpected end");
        if (++mDepth > 32)
            return Fail("nested too deep");
        bool theOK = true;
        char c = mText[mAt];
        if (c == '{') {
            out.mKind = JValue::kObject;
            mAt++;
            Space();
            if (mAt < mText.size() && mText[mAt] == '}') {
                mAt++;
            } else {
                while (theOK) {
                    Space();
                    if (mAt >= mText.size() || mText[mAt] != '"') {
                        theOK = Fail("expected a key");
                        break;
                    }
                    std::pair<std::string, JValue> theField;
                    if (!String(theField.first)) {
                        theOK = false;
                        break;
                    }
                    Space();
                    if (mAt >= mText.size() || mText[mAt] != ':') {
                        theOK = Fail("expected :");
                        break;
                    }
                    mAt++;
                    if (!Value(theField.second)) {
                        theOK = false;
                        break;
                    }
                    out.mFields.push_back(std::move(theField));
                    Space();
                    if (mAt < mText.size() && mText[mAt] == ',') {
                        mAt++;
                        continue;
                    }
                    if (mAt < mText.size() && mText[mAt] == '}') {
                        mAt++;
                        break;
                    }
                    theOK = Fail("expected , or }");
                }
            }
        } else if (c == '[') {
            out.mKind = JValue::kArray;
            mAt++;
            Space();
            if (mAt < mText.size() && mText[mAt] == ']') {
                mAt++;
            } else {
                while (theOK) {
                    JValue theItem;
                    if (!Value(theItem)) {
                        theOK = false;
                        break;
                    }
                    out.mItems.push_back(std::move(theItem));
                    Space();
                    if (mAt < mText.size() && mText[mAt] == ',') {
                        mAt++;
                        continue;
                    }
                    if (mAt < mText.size() && mText[mAt] == ']') {
                        mAt++;
                        break;
                    }
                    theOK = Fail("expected , or ]");
                }
            }
        } else if (c == '"') {
            out.mKind = JValue::kString;
            theOK = String(out.mString);
        } else if (c == 't') {
            out.mKind = JValue::kBool;
            out.mBool = true;
            theOK = Word("true");
        } else if (c == 'f') {
            out.mKind = JValue::kBool;
            theOK = Word("false");
        } else if (c == 'n') {
            theOK = Word("null");
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            theOK = Number(out);
        } else {
            theOK = Fail("unexpected character");
        }
        mDepth--;
        return theOK;
    }
    bool Document(JValue& out)
    {
        if (mText.compare(0, 3, "\xef\xbb\xbf") == 0)
            mAt = 3;   // a UTF-8 byte order mark
        if (!Value(out))
            return false;
        Space();
        return mAt == mText.size() || Fail("text after the document");
    }
};

const JValue* Field(const JValue& inObject, const char* inName)
{
    for (const auto& f : inObject.mFields)
        if (f.first == inName)
            return &f.second;
    return NULL;
}

}   // namespace

// ---------------------------------------------------------------------------------------------------------------
// The manifest

// UTF-8 -> Windows-1252 (the game's narrow text): U+0000..U+00FF as themselves (1252 agrees from 0xa0), else '?'
static std::string EECPAIToGameText(const std::string& inText)
{
    std::string theOut;
    for (size_t i = 0; i < inText.size();) {
        unsigned char c = (unsigned char)inText[i];
        uint32_t u;
        size_t n;
        if (c < 0x80) {
            u = c;
            n = 1;
        } else if ((c & 0xe0) == 0xc0) {
            u = c & 0x1f;
            n = 2;
        } else if ((c & 0xf0) == 0xe0) {
            u = c & 0x0f;
            n = 3;
        } else if ((c & 0xf8) == 0xf0) {
            u = c & 0x07;
            n = 4;
        } else {
            theOut += '?';
            i++;
            continue;
        }
        for (size_t k = 1; k < n && i + k < inText.size(); k++)
            u = (u << 6) | ((unsigned char)inText[i + k] & 0x3f);
        i += n;
        theOut += u < 0x80 || (u >= 0xa0 && u <= 0xff) ? (char)u : '?';
    }
    return theOut;
}

static bool EECPAIIdOK(const std::string& inID)
{
    if (inID.empty() || inID.size() > 64)
        return false;
    for (char c : inID)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}

// a Lua module name as require takes it (EECPLua.cpp ModuleName): letters, digits and _, in segments separated by dots
static bool EECPAIModuleOK(const std::string& inName)
{
    if (inName.empty() || inName.size() > 96)
        return false;
    bool theStart = true;
    for (char c : inName) {
        if (c == '.') {
            if (theStart)
                return false;
            theStart = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            theStart = false;
        } else {
            return false;
        }
    }
    return !theStart;
}

// the script's file is there (EE_AI_SCRIPTS's folder or Data\ai; dots are folders)
static bool EECPAIScriptExists(const std::string& inModule)
{
    std::string theRelative = inModule;
    std::replace(theRelative.begin(), theRelative.end(), '.', '\\');
    theRelative += ".lua";
    const std::string& theOverride = EECPAIOverrideFolder();
    return (!theOverride.empty() && EECPAIFileExists(theOverride + theRelative)) ||
           EECPAIFileExists("Data\\ai\\" + theRelative);
}

static std::vector<EECPAIEntry> EECPAIReadManifest()
{
    std::vector<EECPAIEntry> theAIs;
    std::string thePath, theText;
    const std::string& theOverride = EECPAIOverrideFolder();
    if (!theOverride.empty() && EECPAIReadFile(theOverride + "ais.json", theText))
        thePath = theOverride + "ais.json";
    else if (EECPAIReadFile("Data\\ai\\ais.json", theText))
        thePath = "Data\\ai\\ais.json";
    else {
        EECPEngineLog("no Data/ai/ais.json: the lobby offers only classic and random");
        return theAIs;
    }
    JValue theRoot;
    JReader theReader(theText);
    if (!theReader.Document(theRoot)) {
        EECPEngineLog("%s: %s: no AIs read (only classic and random)", thePath.c_str(), theReader.mError.c_str());
        return theAIs;
    }
    const JValue* theList = theRoot.mKind == JValue::kObject ? Field(theRoot, "ais") : NULL;
    if (theList == NULL || theList->mKind != JValue::kArray) {
        EECPEngineLog("%s: the document must be an object with an array \"ais\": no AIs read (only classic and random)",
                  thePath.c_str());
        return theAIs;
    }
    for (size_t k = 0; k < theList->mItems.size(); k++) {
        const JValue& e = theList->mItems[k];
        std::string theWhere = thePath + ": ais[" + std::to_string(k) + "]";
        if (e.mKind != JValue::kObject) {
            EECPEngineLog("%s: not an object: skipped", theWhere.c_str());
            continue;
        }
        EECPAIEntry theAI;
        theAI.mRandom = false;
        theAI.mSeesAll = false;
        theAI.mFillsGarrison = false;
        std::string theProblem;
        for (const auto& f : e.mFields) {
            const JValue& v = f.second;
            if (f.first == "id" || f.first == "name" || f.first == "script" || f.first == "description") {
                if (v.mKind != JValue::kString) {
                    theProblem = f.first + " must be a string";
                    break;
                }
                (f.first == "id" ? theAI.mID : f.first == "name" ? theAI.mName : f.first == "script" ? theAI.mScript
                                                                                             : theAI.mDescription) = v.mString;
            } else if (f.first == "random") {
                if (v.mKind != JValue::kBool) {
                    theProblem = "random must be true or false";
                    break;
                }
                theAI.mRandom = v.mBool;
            } else if (f.first == "sees_all") {
                if (v.mKind != JValue::kBool) {
                    theProblem = "sees_all must be true or false";
                    break;
                }
                theAI.mSeesAll = v.mBool;
            } else if (f.first == "fills_garrison") {
                if (v.mKind != JValue::kBool) {
                    theProblem = "fills_garrison must be true or false";
                    break;
                }
                theAI.mFillsGarrison = v.mBool;
            } else {
                EECPEngineLog("%s: unknown key \"%s\" ignored", theWhere.c_str(), f.first.c_str());
            }
        }
        if (theProblem.empty() && !EECPAIIdOK(theAI.mID))
            theProblem = "id '" + theAI.mID + "' must be 1 to 64 of a-z, 0-9 and _";
        if (theProblem.empty() && (theAI.mID == "classic" || theAI.mID == "random" || theAI.mID == "next"))
            theProblem = "id '" + theAI.mID + "' is reserved (classic, random, next)";
        for (const EECPAIEntry& o : theAIs)
            if (theProblem.empty() && o.mID == theAI.mID)
                theProblem = "id '" + theAI.mID + "' is listed twice";
        if (theProblem.empty() && !EECPAIModuleOK(theAI.mScript))
            theProblem = "script '" + theAI.mScript +
                         "' is not a module name (letters, digits and _, in segments separated by dots)";
        if (theProblem.empty() && !EECPAIScriptExists(theAI.mScript))
            theProblem = "script '" + theAI.mScript + "': no such file under Data/ai";
        if (theProblem.empty() && theAIs.size() >= kEECPMaxAIs)
            theProblem = "more than " + std::to_string(kEECPMaxAIs) + " AIs";
        if (!theProblem.empty()) {
            EECPEngineLog("%s: %s: skipped", theWhere.c_str(), theProblem.c_str());
            continue;
        }
        theAI.mName = EECPAIToGameText(theAI.mName.empty() ? theAI.mID : theAI.mName);
        theAIs.push_back(theAI);
    }
    std::string theIDs;
    for (const EECPAIEntry& a : theAIs)
        theIDs += (theIDs.empty() ? "" : ",") + a.mID + (a.mRandom ? "" : "(not random)");
    EECPEngineLog("%s: %lu AIs (%s) folder hash %08x", thePath.c_str(), (unsigned long)theAIs.size(),
              theIDs.empty() ? "none" : theIDs.c_str(), (unsigned)EECPAIFolderHash());
    return theAIs;
}

const std::vector<EECPAIEntry>& EECPAIManifest()
{
    static std::vector<EECPAIEntry> sAIs = EECPAIReadManifest();
    return sAIs;
}

unsigned long EECPEngineChoiceCount()
{
    return kEECPChoiceFirstAI + (unsigned long)EECPAIManifest().size();
}

const EECPAIEntry* EECPAIByChoice(unsigned long inChoice)
{
    const std::vector<EECPAIEntry>& theAIs = EECPAIManifest();
    if (inChoice < kEECPChoiceFirstAI || inChoice - kEECPChoiceFirstAI >= theAIs.size())
        return NULL;
    return &theAIs[inChoice - kEECPChoiceFirstAI];
}

const EECPAIEntry* EECPAIByID(const std::string& inID)
{
    for (const EECPAIEntry& a : EECPAIManifest())
        if (a.mID == inID)
            return &a;
    return NULL;
}

long EECPChoiceByName(const std::string& inName)
{
    if (inName == "classic")
        return kEECPChoiceClassic;
    if (inName == "random")
        return kEECPChoiceRandom;
    const std::vector<EECPAIEntry>& theAIs = EECPAIManifest();
    if (inName == "next")
        return theAIs.empty() ? -1 : (long)kEECPChoiceFirstAI;
    for (size_t k = 0; k < theAIs.size(); k++)
        if (theAIs[k].mID == inName)
            return (long)(kEECPChoiceFirstAI + k);
    return -1;
}

std::string EECPChoiceName(unsigned long inChoice)
{
    if (inChoice == kEECPChoiceRandom)
        return "random";
    const EECPAIEntry* theAI = EECPAIByChoice(inChoice);
    return theAI != NULL ? theAI->mID : "classic";
}

// ---------------------------------------------------------------------------------------------------------------
// The folder's hash

static uint32_t EECPAIHashBytes(uint32_t h, const char* inData, size_t inSize, bool inSkipCR)
{
    for (size_t i = 0; i < inSize; i++) {
        if (inSkipCR && inData[i] == '\r')
            continue;   // line ends: the same scripts written on Windows and elsewhere hash alike
        h ^= (unsigned char)inData[i];
        h *= 16777619u;
    }
    return h;
}

// every ais.json (the root's) and *.lua under inFolder (game form, trailing backslash), relative paths with '/'
static void EECPAIListFiles(const std::string& inFolder, const std::string& inRelative, int inDepth,
                            std::vector<std::string>& outFiles)
{
    if (inDepth > 8)
        return;
    PlatFindData theData;
    PlatFind* theFind = PlatFindFirst(EECPAINative(inFolder + inRelative + "*").c_str(), &theData);
    if (theFind == NULL)
        return;
    do {
        std::string theName = theData.mName;
        if (theName == "." || theName == "..")
            continue;
        if (theData.mAttributes & PLAT_ATTR_DIRECTORY) {
            EECPAIListFiles(inFolder, inRelative + theName + "\\", inDepth + 1, outFiles);
            continue;
        }
        std::string theLower = theName;
        for (char& c : theLower)
            c = (char)tolower((unsigned char)c);
        bool theLua = theLower.size() > 4 && theLower.compare(theLower.size() - 4, 4, ".lua") == 0;
        if (theLua || (inRelative.empty() && theLower == "ais.json"))
            outFiles.push_back(inRelative + theName);
    } while (PlatFindNext(theFind, &theData) == PLAT_FILE_OK);
    PlatFindClose(theFind);
}

static uint32_t EECPAIHashFolder(const std::string& inFolder, uint32_t h)
{
    std::vector<std::string> theFiles;
    EECPAIListFiles(inFolder, "", 0, theFiles);
    std::vector<std::pair<std::string, std::string>> theKeyed;   // (relative path, '/' and lower case; the path)
    for (const std::string& f : theFiles) {
        std::string theKey = f;
        for (char& c : theKey)
            c = c == '\\' ? '/' : (char)tolower((unsigned char)c);
        theKeyed.push_back(std::make_pair(theKey, f));
    }
    std::sort(theKeyed.begin(), theKeyed.end());
    for (const auto& f : theKeyed) {
        std::string theText;
        if (!EECPAIReadFile(inFolder + f.second, theText))
            continue;
        h = EECPAIHashBytes(h, f.first.c_str(), f.first.size() + 1, false);
        h = EECPAIHashBytes(h, theText.data(), theText.size(), true);
    }
    return h;
}

uint32_t EECPAIFolderHash()
{
    static uint32_t sHash = 0;
    static bool sDone = false;
    if (!sDone) {
        sDone = true;
        uint32_t h = EECPAIHashFolder("Data\\ai\\", 2166136261u);
        if (!EECPAIOverrideFolder().empty())
            h = EECPAIHashFolder(EECPAIOverrideFolder(), h);
        sHash = h;
    }
    return sHash;
}

// ---------------------------------------------------------------------------------------------------------------
// The seats

long EECPEngine(const EEComputerPlayer* inPlayer)
{
    return inPlayer != NULL && inPlayer->mEngine == kEECPEngineNext ? kEECPEngineNext : kEECPEngineClassic;
}

std::string EECPEngineAI(const EEComputerPlayer* inPlayer)
{
    if (EECPEngine(inPlayer) != kEECPEngineNext || inPlayer->mNextState == NULL)
        return std::string();
    return inPlayer->mNextState->mAI;
}

bool EECPEngineSeesAll(const EEComputerPlayer* inPlayer)
{
    std::string theID = EECPEngineAI(inPlayer);
    const EECPAIEntry* theAI = theID.empty() ? NULL : EECPAIByID(theID);
    return theAI != NULL && theAI->mSeesAll;
}

bool EECPEngineFillsGarrison(const EEComputerPlayer* inPlayer)
{
    std::string theID = EECPEngineAI(inPlayer);
    const EECPAIEntry* theAI = theID.empty() ? NULL : EECPAIByID(theID);
    return theAI != NULL && theAI->mFillsGarrison;
}

std::string EECPEngineName(const EEComputerPlayer* inPlayer)
{
    if (EECPEngine(inPlayer) != kEECPEngineNext)
        return "classic";
    std::string theAI = EECPEngineAI(inPlayer);
    return theAI.empty() ? "next" : theAI;
}

void EECPEngineSet(EEComputerPlayer* inPlayer, unsigned long inChoice)
{
    const EECPAIEntry* theAI = EECPAIByChoice(inChoice);
    if (theAI == NULL) {
        inPlayer->mEngine = kEECPEngineClassic;
        return;
    }
    inPlayer->mEngine = kEECPEngineNext;
    EECPNextGetState(inPlayer)->mAI = theAI->mID;
}

// EE_AI_ENGINE (TEST ONLY): the choices in turn by player id from player 1 (empty: unset)
static const std::vector<long>& EECPEngineOverrides()
{
    static std::vector<long> sChoices;
    static bool sRead = false;
    if (sRead)
        return sChoices;
    sRead = true;
    const char* theValue = getenv("EE_AI_ENGINE");
    if (theValue == NULL || theValue[0] == '\0')
        return sChoices;
    std::string theList(theValue);
    size_t theStart = 0;
    while (theStart <= theList.size()) {
        size_t theEnd = theList.find(',', theStart);
        if (theEnd == std::string::npos)
            theEnd = theList.size();
        std::string theName = theList.substr(theStart, theEnd - theStart);
        long theChoice = EECPChoiceByName(theName);
        if (theChoice >= 0)
            sChoices.push_back(theChoice);
        else
            EECPEngineLog("EE_AI_ENGINE: unknown engine '%s' ignored", theName.c_str());
        theStart = theEnd + 1;
    }
    return sChoices;
}

unsigned long EECPEngineResolve(unsigned long inChoice, unsigned long inSeed, unsigned long inSlot,
                                unsigned long inPlayerID)
{
    const std::vector<long>& theOverrides = EECPEngineOverrides();
    if (!theOverrides.empty())
        inChoice = (unsigned long)theOverrides[(inPlayerID > 0 ? inPlayerID - 1 : 0) % theOverrides.size()];
    if (inChoice == kEECPChoiceRandom) {
        // classic and the AIs that allow it, uniformly, from a hash of the seed and the slot (32-bit arithmetic: the
        // same on every peer and platform); not the game's generators, so the other draws (the random start epoch,
        // the map) stay as they were. With one such AI this is the old classic/next coin.
        std::vector<unsigned long> thePool(1, (unsigned long)kEECPChoiceClassic);
        const std::vector<EECPAIEntry>& theAIs = EECPAIManifest();
        for (size_t k = 0; k < theAIs.size(); k++)
            if (theAIs[k].mRandom)
                thePool.push_back(kEECPChoiceFirstAI + (unsigned long)k);
        uint32_t h = (uint32_t)inSeed * 2654435761u ^ ((uint32_t)inSlot + 1u) * 0x9e3779b9u;
        h ^= h >> 15;
        h *= 0x2c1b3c6du;
        h ^= h >> 12;
        return thePool[(h >> 7) % (uint32_t)thePool.size()];
    }
    return EECPAIByChoice(inChoice) != NULL ? inChoice : (unsigned long)kEECPChoiceClassic;
}

// (EECPNextUpdate, the next engine's tick, is in EECPNext.cpp: docs/AI_LUA_PLAN.md §2.1; EECPNextPrepare in
// EECPNextLua.cpp)
