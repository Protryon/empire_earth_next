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
#include "platform/PlatString.h"
#include "rust/ee_ai.h"   // ee_ai_*: the manifest, the choices and the resolve (ee_ai manifest.rs, seat.rs)

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
// The manifest: its document's checks are Rust's (ee_ai json.rs, manifest.rs: the JSON reader, the entries' checks
// and their log lines; a name as the game's text, UTF-8 with a malformed byte as U+FFFD, decision D6)

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

static bool EECPAIScriptExistsOf(void* inContext, const uint8_t* inModule, size_t inLength)
{
    (void)inContext;
    return EECPAIScriptExists(std::string((const char*)inModule, inLength));
}

static std::string EECPAIText(const EeAiBytes& inBytes)
{
    return inBytes.len > 0 ? std::string((const char*)inBytes.data, inBytes.len) : std::string();
}

static EeAiBytes EECPAIBytes(const std::string& inText)
{
    EeAiBytes theBytes = {(const uint8_t*)inText.data(), inText.size()};
    return theBytes;
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
    EeAiManifest* theRead = ee_ai_manifest_read(EECPAIBytes(theText), EECPAIBytes(thePath), EECPAIScriptExistsOf, NULL);
    for (size_t k = 0; k < ee_ai_manifest_log_count(theRead); k++)
        EECPEngineLog("%s", EECPAIText(ee_ai_manifest_log(theRead, k)).c_str());
    for (size_t k = 0; k < ee_ai_manifest_count(theRead); k++) {
        EeAiManifestEntry e;
        if (!ee_ai_manifest_entry(theRead, k, &e))
            break;
        EECPAIEntry theAI;
        theAI.mID = EECPAIText(e.id);
        theAI.mName = EECPAIText(e.name);
        theAI.mScript = EECPAIText(e.script);
        theAI.mDescription = EECPAIText(e.description);
        theAI.mRandom = e.random;
        theAI.mSeesAll = e.sees_all;
        theAI.mFillsGarrison = e.fills_garrison;
        theAIs.push_back(theAI);
    }
    bool theListed = ee_ai_manifest_listed(theRead);
    ee_ai_manifest_free(theRead);
    if (!theListed)
        return theAIs;   // (the document is not a manifest: logged)
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
    // classic and random need no manifest (it is read at its first use)
    long theChoice = (long)ee_ai_choice_by_name(EECPAIBytes(inName), NULL, 0);
    if (theChoice == kEECPChoiceClassic || theChoice == kEECPChoiceRandom)
        return theChoice;
    const std::vector<EECPAIEntry>& theAIs = EECPAIManifest();
    std::vector<EeAiBytes> theIDs;
    for (const EECPAIEntry& a : theAIs)
        theIDs.push_back(EECPAIBytes(a.mID));
    return (long)ee_ai_choice_by_name(EECPAIBytes(inName), theIDs.data(), theIDs.size());
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
    // classic and the AIs that allow it, uniformly, from a hash of the seed and the slot (32-bit arithmetic: the same on
    // every peer and platform); not the game's generators, so the other draws (the random start epoch, the map) stay
    // as they were (ee_ai seat.rs)
    const std::vector<long>& theOverrides = EECPEngineOverrides();
    std::vector<int64_t> theChoices(theOverrides.begin(), theOverrides.end());
    const std::vector<EECPAIEntry>& theAIs = EECPAIManifest();
    bool theRandom[kEECPMaxAIs];
    for (size_t k = 0; k < theAIs.size() && k < kEECPMaxAIs; k++)
        theRandom[k] = theAIs[k].mRandom;
    return (unsigned long)ee_ai_engine_resolve((uint64_t)inChoice, (uint64_t)inSeed, (uint64_t)inSlot, (uint64_t)inPlayerID,
        theChoices.data(), theChoices.size(), theRandom, theAIs.size());
}

// (EECPNextUpdate, the next engine's tick, is in EECPNext.cpp: docs/AI_LUA_PLAN.md §2.1; EECPNextPrepare in
// EECPNextLua.cpp)
