// C:\EEX\Empire Earth\Random Map\Random Map Generators\Default\Unit10.cpp (file name inferred)
// The random map script (.rmv) reader: preprocessor (#define/#if/#elif/#else), keyword tables, value parser.
// Unit flags (calibrated, docs/CALIBRATION.md): -O1 -G6 -MD -GR
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <map>

#include "lle/LowLevelEngine.h"
#include "lle/U2DGeometry.h"
#include "ee/EERandomMap.h"
#include "Unit10.h"
#include "Unit12.h"
#include "LostDebug.h"  // NOT original source: EE_LOST_DEBUG filler (ParseLongList, ParseFloatList; docs/LOST_DEBUG.md)
#include "ee/EERandomMapIDs.h"

// ---- Unit-local types (provisional names)
enum {                                      // value types of the number parsers (ParseNumber 0xcbea)
    kRMLong = 100,
    kRMFloat = 101
};
const long kRMUnset = (long)0xbaadf00d;           // value of a list entry that failed to parse (0xc9fd, 0xd223)

enum {                                      // keyword tokens (InitializeKeywords 0xefd7)
    kRMUnknownKeyword = 0,
    kRMBetween = 100,
    kRMOneOf = 101,
    kRMAnd = 200,
    kRMOr = 203,
    kRMXor = 204,
    kRMNot = 205,
    kRMEqual = 206,
    kRMNotEqual = 207,
    kRMLess = 208,
    kRMLessEqual = 209,
    kRMGreater = 210,
    kRMGreaterEqual = 211,
    kRMPercent = 212,
    kRMDefined = 1000,
    kRMNotDefined = 1001,
    kRMFirstCustomToken = 100000            // handed to gKeywordHandler (0xcccb)
};

// Statement trace: empty in this build, but its arguments are still evaluated (the out-of-line vector
// accessors stay as calls: ReadScript 0xf222, ParseLine 0xf361). (inferred)
inline void RMTrace(const UString& inText) {}
inline void RMTrace(const UString& inText, const long& inA, const long& inB, const long& inC) {}

struct RMDefine {                           // gDefines value: two dwords set by AddDefine 0xe68f
    long mToken;                            // attribute / directive token (InitializeDefines 0xd943)
    long mType;                             // value type: kRMLong, kRMFloat, 0 for directives
};

typedef void (*RMAttributeHandler)(EERandomMap* ioMap, unsigned long inToken, long inType,
                                   std::vector<long>& inLongs, std::vector<float>& inFloats);
typedef void (*RMKeywordHandler)(EERandomMap* ioMap, long inToken, std::vector<UString>& inTokens, long inType,
                                 void* outValue, long inSize);

// Attribute defaults (SetDefaults 0x11668): named constants, loaded from .rdata (0x19294.., in field order),
// unlike the literals stored as immediates. Names after the script keywords (provisional names)
const float kDefaultCoastalToDeeperWaterPercentage = 0.0f;
const float kDefaultConvergentSampleSize = 32.0f;
const float kDefaultConvergentThreshold = 0.1f;
const float kDefaultForestChaosLevel = 0.1f;
const float kDefaultForestFreeRadius = 10.0f;
const float kDefaultInnerNeutralFlatChaos = 0.7f;
const float kDefaultInnerNeutralLandChaos = 0.7f;
const float kDefaultInnerNeutralPercentFlat = 0.9f;
const float kDefaultInnerNeutralPercentLand = 0.25f;
const float kDefaultInnerNeutralTreePercentage = 0.0f;
const float kDefaultMaxAngleBetweenTeamMembers = 360.0f;
const float kDefaultNeutralInnerRadius = 0.1f;
const float kDefaultNeutralOuterRadius = 0.4f;
const float kDefaultOuterNeutralFlatChaos = 0.7f;
const float kDefaultOuterNeutralLandChaos = 0.7f;
const float kDefaultOuterNeutralPercentFlat = 0.9f;
const float kDefaultOuterNeutralPercentLand = 0.25f;
const float kDefaultOuterNeutralTreePercentage = 0.7f;
const float kDefaultPercentLand = 0.6f;
const float kDefaultPlayerFlatChaos = 0.7f;
const float kDefaultPlayerInnerRadius = 0.5f;
const float kDefaultPlayerLandChaos = 0.7f;
const float kDefaultPlayerOuterRadius = 0.8f;
const float kDefaultPlayerPercentFlat = 0.75f;
const float kDefaultPlayerTreePercentage = 0.4f;
const float kDefaultTerrainChaos = 10.0f;
const float kDefaultHeightMapChaos = 1.0f;
const float kDefaultHeightMapSmoothness = 0.2f;
const float kDefaultF2FC = 0.0f;
const float kDefaultResourceSeperation = 3.0f;

// Compiler-generated for this unit (names in analysis/default/names.txt):
// (0x11a9b / 0x11cdf: called out of line in the original, inlined in ours: inline-budget rows)
// TEMPLATE: DEFAULT 0x10011a9b
// TEMPLATE: DEFAULT 0x10011b5a
// TEMPLATE: DEFAULT 0x10011bed
// TEMPLATE: DEFAULT 0x10011c80
// TEMPLATE: DEFAULT 0x10011cdf
// TEMPLATE: DEFAULT 0x10011cef
// TEMPLATE: DEFAULT 0x10011d1b
// TEMPLATE: DEFAULT 0x10011d47
// TEMPLATE: DEFAULT 0x10012094
// TEMPLATE: DEFAULT 0x10013ccb
// TEMPLATE: DEFAULT 0x10012560
// TEMPLATE: DEFAULT 0x10011c45
// TEMPLATE: DEFAULT 0x1001244a
// TEMPLATE: DEFAULT 0x10014f70
// TEMPLATE: DEFAULT 0x10011bc1
// TEMPLATE: DEFAULT 0x10011cb3
// TEMPLATE: DEFAULT 0x10012331
// TEMPLATE: DEFAULT 0x100124fa
// TEMPLATE: DEFAULT 0x100131bd
// TEMPLATE: DEFAULT 0x10014fe2
// TEMPLATE: DEFAULT 0x10015014
// TEMPLATE: DEFAULT 0x10015037
// SYNTHETIC: DEFAULT 0x100153d7
// TEMPLATE: DEFAULT 0x10011ab1
// TEMPLATE: DEFAULT 0x10013a05
// TEMPLATE: DEFAULT 0x10013fc8
// TEMPLATE: DEFAULT 0x10014265
// TEMPLATE: DEFAULT 0x10014dac
// TEMPLATE: DEFAULT 0x10014dc3
// TEMPLATE: DEFAULT 0x10014dd7
// TEMPLATE: DEFAULT 0x10015206
// TEMPLATE: DEFAULT 0x10015231
// TEMPLATE: DEFAULT 0x10015253
// TEMPLATE: DEFAULT 0x10015283
// TEMPLATE: DEFAULT 0x1001543e
// TEMPLATE: DEFAULT 0x10011c19
// TEMPLATE: DEFAULT 0x100124a0
// TEMPLATE: DEFAULT 0x100123a0
// TEMPLATE: DEFAULT 0x100123fa
// TEMPLATE: DEFAULT 0x10013485
// TEMPLATE: DEFAULT 0x1001374d
// TEMPLATE: DEFAULT 0x10011abe
// TEMPLATE: DEFAULT 0x10011ac5
// TEMPLATE: DEFAULT 0x10011af9
// TEMPLATE: DEFAULT 0x10011cac
// TEMPLATE: DEFAULT 0x10011d73
// TEMPLATE: DEFAULT 0x10011d80
// TEMPLATE: DEFAULT 0x1001238b
// TEMPLATE: DEFAULT 0x1001248e
// TEMPLATE: DEFAULT 0x10013fb1
// TEMPLATE: DEFAULT 0x10014d71
// TEMPLATE: DEFAULT 0x10014d85
// TEMPLATE: DEFAULT 0x10014f4c
// TEMPLATE: DEFAULT 0x10015066
// TEMPLATE: DEFAULT 0x10015089
// TEMPLATE: DEFAULT 0x100150b8
// TEMPLATE: DEFAULT 0x100150ed
// TEMPLATE: DEFAULT 0x10015112
// TEMPLATE: DEFAULT 0x10015144
// TEMPLATE: DEFAULT 0x1001516c
// TEMPLATE: DEFAULT 0x1001519c
// TEMPLATE: DEFAULT 0x100151d4
// SYNTHETIC: DEFAULT 0x100153ed
// SYNTHETIC: DEFAULT 0x10015403
// SYNTHETIC: DEFAULT 0x10015428
// TEMPLATE: DEFAULT 0x10012554
// TEMPLATE: DEFAULT 0x10012698
// TEMPLATE: DEFAULT 0x10012dba
// TEMPLATE: DEFAULT 0x100147de
// TEMPLATE: DEFAULT 0x10014c03
// TEMPLATE: DEFAULT 0x10014e1f
// TEMPLATE: DEFAULT 0x10011d87
// TEMPLATE: DEFAULT 0x10011a57
// TEMPLATE: DEFAULT 0x10011b86
// TEMPLATE: DEFAULT 0x10011db3
// TEMPLATE: DEFAULT 0x10011efd
// TEMPLATE: DEFAULT 0x100125a4
// TEMPLATE: DEFAULT 0x10011e19
// TEMPLATE: DEFAULT 0x10011ee9
// TEMPLATE: DEFAULT 0x10011f6c
// TEMPLATE: DEFAULT 0x1001203c
// TEMPLATE: DEFAULT 0x10012050
// TEMPLATE: DEFAULT 0x100122e1
// TEMPLATE: DEFAULT 0x100125f4
// TEMPLATE: DEFAULT 0x1001271d
// TEMPLATE: DEFAULT 0x10012922
// TEMPLATE: DEFAULT 0x10012ab6
// TEMPLATE: DEFAULT 0x10012e2a
// TEMPLATE: DEFAULT 0x10012f27
// TEMPLATE: DEFAULT 0x1001427a
// TEMPLATE: DEFAULT 0x1001484e
// TEMPLATE: DEFAULT 0x1001490a
// TEMPLATE: DEFAULT 0x10014d30
// TEMPLATE: DEFAULT 0x10015325
// TEMPLATE: DEFAULT 0x1001534b
// TEMPLATE: DEFAULT 0x10014c74
// TEMPLATE: DEFAULT 0x10014dee
// TEMPLATE: DEFAULT 0x10014e90
// TEMPLATE: DEFAULT 0x10014f9d
// TEMPLATE: DEFAULT 0x100152a4
// TEMPLATE: DEFAULT 0x100152da
// TEMPLATE: DEFAULT 0x100152f8
// TEMPLATE: DEFAULT 0x10015372
// TEMPLATE: DEFAULT 0x1001539a
// SYNTHETIC: DEFAULT 0x10015455
// SYNTHETIC: DEFAULT 0x10015472
// TEMPLATE: DEFAULT 0x10011948
// TEMPLATE: DEFAULT 0x10011a43
// TEMPLATE: DEFAULT 0x10011dee
// TEMPLATE: DEFAULT 0x10011f41
// TEMPLATE: DEFAULT 0x100120c0
// TEMPLATE: DEFAULT 0x100126ac
// TEMPLATE: DEFAULT 0x100128b1
// TEMPLATE: DEFAULT 0x10011973
// TEMPLATE: DEFAULT 0x10012131
// SYNTHETIC: DEFAULT 0x100122c5
// TEMPLATE: DEFAULT 0x1001265f
// TEMPLATE: DEFAULT 0x10012afa
// SYNTHETIC: DEFAULT 0x10012db1
// TEMPLATE: DEFAULT 0x10012df5
// TEMPLATE: DEFAULT 0x10012e60
// TEMPLATE: DEFAULT 0x10012ea2
// TEMPLATE: DEFAULT 0x10012ee6
// TEMPLATE: DEFAULT 0x10014527
// TEMPLATE: DEFAULT 0x10014819
// TEMPLATE: DEFAULT 0x10014884
// TEMPLATE: DEFAULT 0x100148c6
// TEMPLATE: DEFAULT 0x1001494b
// TEMPLATE: DEFAULT 0x10014c3f
// TEMPLATE: DEFAULT 0x10014caa
// TEMPLATE: DEFAULT 0x10014cec
// TEMPLATE: DEFAULT 0x10014ea4
// TEMPLATE: DEFAULT 0x10014edc
// TEMPLATE: DEFAULT 0x10014f14
// TEMPLATE: DEFAULT 0x10014fc2
// TEMPLATE: DEFAULT 0x100153bf
// SYNTHETIC: DEFAULT 0x1000c737
// SYNTHETIC: DEFAULT 0x1000c741
// SYNTHETIC: DEFAULT 0x1000c752
// SYNTHETIC: DEFAULT 0x1000c75e
// SYNTHETIC: DEFAULT 0x1000c769
// SYNTHETIC: DEFAULT 0x1000c773
// SYNTHETIC: DEFAULT 0x1000c784
// SYNTHETIC: DEFAULT 0x1000c790
// SYNTHETIC: DEFAULT 0x1000c79b
// SYNTHETIC: DEFAULT 0x1000c7a5
// SYNTHETIC: DEFAULT 0x1000c7b6
// SYNTHETIC: DEFAULT 0x1000c7c2
// SYNTHETIC: DEFAULT 0x1000c7cd
// SYNTHETIC: DEFAULT 0x1000c7d7
// SYNTHETIC: DEFAULT 0x1000c7e8
// SYNTHETIC: DEFAULT 0x1000c7f4
// SYNTHETIC: DEFAULT 0x1000c7ff
// SYNTHETIC: DEFAULT 0x1000c809
// SYNTHETIC: DEFAULT 0x1000c81a
// SYNTHETIC: DEFAULT 0x1000c826
// SYNTHETIC: DEFAULT 0x1000c831
// SYNTHETIC: DEFAULT 0x1000c83b
// SYNTHETIC: DEFAULT 0x1000c84c
// SYNTHETIC: DEFAULT 0x1000c858
// SYNTHETIC: DEFAULT 0x1000c863
// SYNTHETIC: DEFAULT 0x1000c86d
// SYNTHETIC: DEFAULT 0x1000c87e
// SYNTHETIC: DEFAULT 0x1000c88a
// SYNTHETIC: DEFAULT 0x1000c895
// SYNTHETIC: DEFAULT 0x1000c89f
// SYNTHETIC: DEFAULT 0x1000c8b7
// SYNTHETIC: DEFAULT 0x1000c8c3
// SYNTHETIC: DEFAULT 0x1000c8cd
// SYNTHETIC: DEFAULT 0x1000c8d7
// SYNTHETIC: DEFAULT 0x1000c8ef
// SYNTHETIC: DEFAULT 0x1000c8fb
// SYNTHETIC: DEFAULT 0x1000c905
// SYNTHETIC: DEFAULT 0x1000c90f
// SYNTHETIC: DEFAULT 0x1000c927
// SYNTHETIC: DEFAULT 0x1000c933
// SYNTHETIC: DEFAULT 0x1000c93d
// SYNTHETIC: DEFAULT 0x1000c947
// SYNTHETIC: DEFAULT 0x1000c967
// SYNTHETIC: DEFAULT 0x1000c973
// SYNTHETIC: DEFAULT 0x1000c97d
// SYNTHETIC: DEFAULT 0x1000c987
// SYNTHETIC: DEFAULT 0x1000c9a7
// SYNTHETIC: DEFAULT 0x1000c9b3
// SYNTHETIC: DEFAULT 0x1000c9bd
// SYNTHETIC: DEFAULT 0x1000c9c7
// SYNTHETIC: DEFAULT 0x1000c9e7
// SYNTHETIC: DEFAULT 0x1000c9f3
// SYNTHETIC: DEFAULT 0x1001548f
// SYNTHETIC: DEFAULT 0x10015499

// ---- Globals in definition order (= dynamic initializer order 0xc895.., after the U2D header keys).
std::vector<UString> gFileNames;                    // 0x1c810: script file stack (#include)
std::vector<long> gLineNumbers;                     // 0x1c800: line number per open script file
std::vector<long> gIfStack;                         // 0x1c7f0: #if state per nesting level (0 not yet taken,
                                                    // 1 active, 2 done or inside an inactive branch)
std::map<UString, RMDefine> gDefines;               // 0x1c7e0
std::map<UString, UString> gMapTypes;               // 0x1c7d0
std::map<UString, long> gKeywords;                  // 0x1c7c0

long GetMapType(const UString& inName, UString& outValue);
long GetKeyword(const UString& inName);
long GetAttributeToken(const UString& inName);
long GetAttributeType(const UString& inName);
bool gReportErrors;                                 // 0x1c890: print "%s (%d) : ERROR, ..." diagnostics
RMAttributeHandler gAttributeHandler;               // 0x1c894: called for attribute tokens > kRMFirstCustomToken
RMKeywordHandler gKeywordHandler;                   // 0x1c898: called for keyword tokens >= kRMFirstCustomToken

static void ExpandMapTypes(const std::vector<UString>& inTokens, std::vector<UString>& outTokens);
static void ParseNumber(const UString& inText, long inType, void* outValue, long inSize);
static void EvaluateExpression(EERandomMap* ioMap, const UString& inText, long inType, void* outValue, long inSize);

// NEAR: the original keeps i in ebx (shared with the _Buy(0) zero) and i*16 in inText's slot; declaration variants did not move it
// FUNCTION: DEFAULT 0x1000c9fd
// Parses a script value list into integers: plain numbers directly, expressions such as Between or
// OneOf evaluated; an entry that fails stays 0xbaadf00d
void ParseLongList(EERandomMap* ioMap, const UString& inText, std::vector<long>& outValues)
{
    EE_LOST_DEBUG(1);  // LOST-DEBUG (PROVISIONAL, owner experiment 2026-10-24): 2001 twin inlines what AoC refuses (0 vs 3 out-of-line _Ptrit::operator+); no assert-line evidence
    std::vector<UString> theTokens;
    std::vector<UString> theExpanded;
    UString theSeparators(" \t,");
    UString theBrackets("()");
    long theValue = kRMUnset;
    outValues.clear();
    inText.NestedTokenize(theSeparators, theBrackets, theTokens);
    ExpandMapTypes(theTokens, theExpanded);
    for (unsigned long i = 0; i < theExpanded.size(); i++) {
        theValue = kRMUnset;
        if (theExpanded[i].Find("(", 0) == theExpanded[i].GetLength())
            ParseNumber(theExpanded[i], kRMLong, &theValue, sizeof(theValue));
        else
            EvaluateExpression(ioMap, theExpanded[i], kRMLong, &theValue, sizeof(theValue));
        outValues.push_back(theValue);
    }
}

// FUNCTION: DEFAULT 0x1000cb54
// Replaces every token that names a #define or map constant with its value (a leading # token stays)
static void ExpandMapTypes(const std::vector<UString>& inTokens, std::vector<UString>& outTokens)
{
    UString theValue;
    unsigned long theCount = inTokens.size();
    outTokens.clear();
    for (unsigned long i = 0; i < theCount; i++) {
        if (i == 0 && inTokens[0][0] == '#')
            outTokens.push_back(inTokens[0]);
        else if (GetMapType(inTokens[i], theValue) == kErrorOK)
            outTokens.push_back(theValue);
        else
            outTokens.push_back(inTokens[i]);
    }
}

// FUNCTION: DEFAULT 0x1000cbea
// Reads a float or integer from text into the given value of matching size
static void ParseNumber(const UString& inText, long inType, void* outValue, long inSize)
{
    switch (inType) {
    case kRMFloat:
        if (inSize == sizeof(float) && sscanf(inText, "%f", outValue) != 1 && gReportErrors)
            printf("%s (%d) : ERROR, unknown float value, \"%s\"\n",
                   (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)inText);
        break;
    case kRMLong:
        if (inSize == sizeof(long) && sscanf(inText, "%d", outValue) != 1 && gReportErrors)
            printf("%s (%d) : ERROR, unknown integer value, \"%s\"\n",
                   (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)inText);
        break;
    }
}

// FUNCTION: DEFAULT 0x1000cc97
// Looks up a #define or map constant; returns kErrorNotFound when it is unknown
long GetMapType(const UString& inName, UString& outValue)
{
    std::map<UString, UString>::iterator theEntry = gMapTypes.find(inName);
    if (theEntry != gMapTypes.end()) {
        outValue = (*theEntry).second;
        return kErrorOK;
    }
    return kErrorNotFound;
}

static void AddDefine(const UString& inName, long inToken, long inType);
static void AddMapType(const UString& inName, const UString& inValue);
static void AddKeyword(const UString& inName, long inToken);
void ParseLine(EERandomMap* ioMap, const UString& inLine);
long EvaluateCondition(EERandomMap* ioMap, const UString& inText);
void SetDefaults(EERandomMap* ioMap);
void InitializeDefines();
void InitializeMapTypes(EERandomMap* ioMap);
void InitializeKeywords();
void ReadScript(EERandomMap* ioMap, const UString& inName);

// NEAR: the original inlines URandom::Generate() (NextInSeries / modulus, 8-byte fild temp) inside Generate(min, max); ours calls the import. Lead: Generate() is an exported inline in the real header (shared-header change, not tested)
// FUNCTION: DEFAULT 0x1000cccb
// Evaluates a script value expression: OneOf picks a random listed value, Between a random value in
// range; user keywords go to the keyword handler (still reported as ignored)
void EvaluateExpression(EERandomMap* ioMap, const UString& inText, long inType, void* outValue, long inSize)
{
    std::vector<float> theFloats;
    std::vector<long> theLongs;
    std::vector<UString> theTokens;
    std::vector<UString> theExpanded;
    UString theSeparators(" \t(),");
    long theLong;
    float theFloat;
    long theMaximum;
    float theFloatMaximum;
    unsigned long theKeyword;
    unsigned long i;
    inText.Tokenize(theSeparators, theTokens);
    ExpandMapTypes(theTokens, theExpanded);
    theKeyword = GetKeyword(theExpanded[0]);
    if (theKeyword < kRMFirstCustomToken) {
        switch (theKeyword) {
        default:
            if (gReportErrors)
                printf("%s (%d) : ERROR, keyword \"%s\" can only be used in #if tests.\n",
                       (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)theExpanded[0]);
            break;
        case kRMOneOf:
            if (theExpanded.size() >= 2) {
                if (inType == kRMLong) {
                    if (inSize == sizeof(long)) {
                        for (i = 1; i < theExpanded.size(); i++) {
                            ParseNumber(theExpanded[i], kRMLong, &theLong, sizeof(theLong));
                            theLongs.push_back(theLong);
                        }
                        *static_cast<long*>(outValue) = theLongs[ioMap->mRandomIntegers.Generate(0, theLongs.size() - 1)];
                    }
                } else if (inType == kRMFloat && inSize == sizeof(float)) {
                    for (i = 1; i < theExpanded.size(); i++) {
                        ParseNumber(theExpanded[i], kRMFloat, &theFloat, sizeof(theFloat));
                        theFloats.push_back(theFloat);
                    }
                    *static_cast<float*>(outValue) = theFloats[ioMap->mRandomIntegers.Generate(0, theFloats.size() - 1)];
                }
            } else if (gReportErrors)
                printf("%s (%d) : ERROR, wrong number of arguments for OneOf(n1,n2,...).\n",
                       (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)theExpanded[0]);
            break;
        case kRMBetween:
            if (theExpanded.size() == 3) {
                if (inType == kRMLong) {
                    if (inSize == sizeof(long)) {
                        ParseNumber(theExpanded[1], kRMLong, &theLong, sizeof(theLong));
                        ParseNumber(theExpanded[2], kRMLong, &theMaximum, sizeof(theMaximum));
                        *static_cast<long*>(outValue) = ioMap->mRandomIntegers.Generate(theLong, theMaximum);
                    }
                } else if (inType == kRMFloat && inSize == sizeof(float)) {
                    ParseNumber(theExpanded[1], kRMFloat, &theFloat, sizeof(theFloat));
                    ParseNumber(theExpanded[2], kRMFloat, &theFloatMaximum, sizeof(theFloatMaximum));
                    *static_cast<float*>(outValue) = ioMap->mRandom.Generate(theFloat, theFloatMaximum);
                }
            } else if (gReportErrors)
                printf("%s (%d) : ERROR, wrong number of arguments for Between(min,max).\n",
                       (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)theExpanded[0]);
            break;
        case kRMUnknownKeyword:
            if (gReportErrors)
                printf("%s (%d) : ERROR, unknown keyword \"%s\" detected.\n",
                       (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)theExpanded[0]);
            break;
        }
    } else {
        if (gKeywordHandler)
            gKeywordHandler(ioMap, theKeyword, theExpanded, inType, outValue, inSize);
        if (gReportErrors)
            printf("%s (%d) : ERROR, user-defined keyword \"%s\" detected, ignoring.\n",
                   (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)theExpanded[0]);
    }
}

// FUNCTION: DEFAULT 0x1000d1fc
// Returns the token of a script keyword, or 0 when it is unknown
long GetKeyword(const UString& inName)
{
    std::map<UString, long>::iterator theEntry = gKeywords.find(inName);
    if (theEntry != gKeywords.end())
        return (*theEntry).second;
    return kRMUnknownKeyword;
}

// NEAR: as ParseLongList, plus the frame is 4 bytes larger in the original (0x58)
// FUNCTION: DEFAULT 0x1000d223
// Parses a script value list into floats like ParseLongList (a failed entry stays -1163005952.0)
void ParseFloatList(EERandomMap* ioMap, const UString& inText, std::vector<float>& outValues)
{
    EE_LOST_DEBUG(1);  // LOST-DEBUG (PROVISIONAL, owner experiment 2026-10-24): 2001 twin inlines what AoC refuses (0 vs 3 out-of-line _Ptrit::operator+); no assert-line evidence
    std::vector<UString> theTokens;
    std::vector<UString> theExpanded;
    UString theSeparators(" 	,");
    UString theBrackets("()");
    float theValue = 0.0f;
    outValues.clear();
    inText.NestedTokenize(theSeparators, theBrackets, theTokens);
    ExpandMapTypes(theTokens, theExpanded);
    for (unsigned long i = 0; i < theExpanded.size(); i++) {
        theValue = kRMUnset;
        if (theExpanded[i].Find("(", 0) == theExpanded[i].GetLength())
            ParseNumber(theExpanded[i], kRMFloat, &theValue, sizeof(theValue));
        else
            EvaluateExpression(ioMap, theExpanded[i], kRMFloat, &theValue, sizeof(theValue));
        outValues.push_back(theValue);
    }
}

// NEAR: inline-budget pattern (begin()/operator+ in/out of line differs per access); structure verified
// FUNCTION: DEFAULT 0x1000d378
// Resets and reads a .rmv script, converts the elevations to 0..1 units, sizes the flat area from
// the placed units and walls, and maps tree types to forest indices (dropping unknown ones)
long SetRandomMapValues(EERandomMap* ioMap, const UString& inScriptName)
{
    float theMaxY = 0.0f;
    float theMinX = 0.0f;
    float theMinY = 0.0f;
    float theMaxX = 0.0f;
    float theScale;
    float theMinimum;
    float theThreshold;
    float theX;
    float theY;
    float theRight;
    float theTop;
    unsigned long i;
    std::vector<long>::iterator theType;
    ResetPlayerOrder();
    InitializeDefines();
    InitializeMapTypes(ioMap);
    InitializeKeywords();
    SetDefaults(ioMap);
    gIfStack.clear();
    ReadScript(ioMap, inScriptName);
    theScale = 1.0f / (ioMap->mMaxIntElevation - ioMap->mMinIntElevation);
    ioMap->mElevationUnit = theScale;
    theMinimum = ioMap->mMinIntElevation;
    ioMap->mFlatElevation = (ioMap->mFlatElevation - theMinimum) * theScale;
    theThreshold = ioMap->mLandElevationThreshold = (ioMap->mLandElevationThreshold - theMinimum) * theScale;
    ioMap->mInitialElevation = (ioMap->mInitialElevation - theMinimum) * theScale;
    ioMap->mShallowsElevation = (ioMap->mShallowsElevation - theMinimum) * theScale - 0.00001f;
    ioMap->mMaxResourceElevation = (ioMap->mMaxResourceElevation - theMinimum) * theScale;
    if (ioMap->mMaxResourceElevation < theThreshold)
        ioMap->mMaxResourceElevation = theThreshold;
    ioMap->mMaxTreeElevation = (ioMap->mMaxTreeElevation - theMinimum) * theScale;
    if (ioMap->mMaxTreeElevation < theThreshold)
        ioMap->mMaxTreeElevation = theThreshold;
    if (ioMap->mNumInnerNeutrals == 0)
        ioMap->mInnerNeutralPercentLand = 0.0f;
    if (ioMap->mNumOuterNeutrals == 0)
        ioMap->mOuterNeutralPercentLand = 0.0f;
    if (ioMap->mResourceToEdgeDistance == 0)
        ioMap->mResourceToEdgeDistance = 1;
    for (i = 0; i < ioMap->mFeatures.size(); i++) {
        if (ioMap->mFeatures[i].mInRing || ioMap->mFeatures[i].mIsHerd)
            continue;
        if (ioMap->mUnitNames[ioMap->mFeatures[i].mUnitType].mSize < 2) {
            theRight = theX = ioMap->mFeatures[i].mPosition.mX;
            theTop = theY = ioMap->mFeatures[i].mPosition.mY;
        } else {
            theX = ioMap->mFeatures[i].mPosition.mX;
            theRight = ioMap->mUnitNames[ioMap->mFeatures[i].mUnitType].mSize + theX;
            theY = ioMap->mFeatures[i].mPosition.mY;
            theTop = ioMap->mUnitNames[ioMap->mFeatures[i].mUnitType].mSize + theY;
        }
        if (theMinX > theX)
            theMinX = theX;
        if (theMaxX < theRight)
            theMaxX = theRight;
        if (theY < theMinY)
            theMinY = theY;
        if (theTop > theMaxY)
            theMaxY = theTop;
    }
    for (i = 0; i < ioMap->mWalls.size(); i++) {
        if (ioMap->mWalls[i].mStartX < ioMap->mWalls[i].mEndX) {
            theX = ioMap->mWalls[i].mStartX;
            theRight = ioMap->mWalls[i].mEndX;
        } else {
            theX = ioMap->mWalls[i].mEndX;
            theRight = ioMap->mWalls[i].mStartX;
        }
        if (ioMap->mWalls[i].mStartY < ioMap->mWalls[i].mEndY) {
            theY = ioMap->mWalls[i].mStartY;
            theTop = ioMap->mWalls[i].mEndY;
        } else {
            theY = ioMap->mWalls[i].mEndY;
            theTop = ioMap->mWalls[i].mStartY;
        }
        if (theMinX > theX)
            theMinX = theX;
        else if (theMaxX < theRight)
            theMaxX = theRight;
        if (theMinX > theY)
            theMinX = theY;
        else if (theTop > theMaxX)
            theMaxX = theTop;
    }
    ioMap->mFlatArea.mTop = theMaxY;
    ioMap->mFlatArea.mLeft = theMinX;
    ioMap->mFlatArea.mBottom = theMinY;
    ioMap->mFlatArea.mRight = theMaxX;
    theType = ioMap->mTreeTypes.begin();
    while (theType != ioMap->mTreeTypes.end()) {
        for (i = 0; i < ioMap->mForests.size(); i++)
            if (ioMap->mForests[i].mId == *theType) {
                *theType = i;
                break;
            }
        if (i < ioMap->mForests.size())
            ++theType;
        else
            theType = ioMap->mTreeTypes.erase(theType);
    }
    return 0;
}

// FUNCTION: DEFAULT 0x1000d943
// Fills the table of script attribute and directive names with tokens and value types
void InitializeDefines()
{
    gDefines.clear();
    AddDefine("#define", kRMTokenDefine, 0);
    AddDefine("#if", kRMTokenIf, 0);
    AddDefine("#elif", kRMTokenElif, 0);
    AddDefine("#else", kRMTokenElse, 0);
    AddDefine("#endif", kRMTokenEndif, 0);
    AddDefine("#include", kRMTokenInclude, 0);
    AddDefine("MaxIntElevation", kRMTokenMaxIntElevation, kRMLong);
    AddDefine("MinIntElevation", kRMTokenMinIntElevation, kRMLong);
    AddDefine("InitialElevation", kRMTokenInitialElevation, kRMLong);
    AddDefine("FlatElevation", kRMTokenFlatElevation, kRMLong);
    AddDefine("ElevationScale", kRMTokenElevationScale, kRMFloat);
    AddDefine("HeightMapChaos", kRMTokenHeightMapChaos, kRMFloat);
    AddDefine("HeightMapSmoothness", kRMTokenHeightMapSmoothness, kRMFloat);
    AddDefine("PercentLand", kRMTokenPercentLand, kRMFloat);
    AddDefine("LandElevationThreshold", kRMTokenLandElevationThreshold, kRMLong);
    AddDefine("WaterBorder", kRMTokenWaterBorder, kRMLong);
    AddDefine("RiverWidth", kRMTokenRiverWidth, kRMLong);
    AddDefine("CoastalToDeeperWaterPercentage", kRMTokenCoastalToDeeperWaterPercentage, kRMFloat);
    AddDefine("TeamGrowth", kRMTokenTeamGrowth, kRMLong);
    AddDefine("LandfillPercentage", kRMTokenLandfillPercentage, kRMFloat);
    AddDefine("PlayerInnerRadius", kRMTokenPlayerInnerRadius, kRMFloat);
    AddDefine("PlayerOuterRadius", kRMTokenPlayerOuterRadius, kRMFloat);
    AddDefine("PlayerPercentFlat", kRMTokenPlayerPercentFlat, kRMFloat);
    AddDefine("PlayerTreePercentage", kRMTokenPlayerTreePercentage, kRMFloat);
    AddDefine("PlayerFlatChaos", kRMTokenPlayerFlatChaos, kRMFloat);
    AddDefine("PlayerFlatClumps", kRMTokenPlayerFlatClumps, kRMLong);
    AddDefine("PlayerLandChaos", kRMTokenPlayerLandChaos, kRMFloat);
    AddDefine("PlayerLandClumps", kRMTokenPlayerLandClumps, kRMLong);
    AddDefine("MaxAngleBetweenTeamMembers", kRMTokenMaxAngleBetweenTeamMembers, kRMFloat);
    AddDefine("NeutralInnerRadius", kRMTokenNeutralInnerRadius, kRMFloat);
    AddDefine("NeutralOuterRadius", kRMTokenNeutralOuterRadius, kRMFloat);
    AddDefine("NumOuterNeutrals", kRMTokenNumOuterNeutrals, kRMLong);
    AddDefine("OuterNeutralPercentLand", kRMTokenOuterNeutralPercentLand, kRMFloat);
    AddDefine("OuterNeutralPercentFlat", kRMTokenOuterNeutralPercentFlat, kRMFloat);
    AddDefine("OuterNeutralTreePercentage", kRMTokenOuterNeutralTreePercentage, kRMFloat);
    AddDefine("OuterNeutralLandChaos", kRMTokenOuterNeutralLandChaos, kRMFloat);
    AddDefine("OuterNeutralLandClumps", kRMTokenOuterNeutralLandClumps, kRMLong);
    AddDefine("OuterNeutralFlatChaos", kRMTokenOuterNeutralFlatChaos, kRMFloat);
    AddDefine("OuterNeutralFlatClumps", kRMTokenOuterNeutralFlatClumps, kRMLong);
    AddDefine("NumInnerNeutrals", kRMTokenNumInnerNeutrals, kRMLong);
    AddDefine("InnerNeutralPercentLand", kRMTokenInnerNeutralPercentLand, kRMFloat);
    AddDefine("InnerNeutralPercentFlat", kRMTokenInnerNeutralPercentFlat, kRMFloat);
    AddDefine("InnerNeutralTreePercentage", kRMTokenInnerNeutralTreePercentage, kRMFloat);
    AddDefine("InnerNeutralLandChaos", kRMTokenInnerNeutralLandChaos, kRMFloat);
    AddDefine("InnerNeutralLandClumps", kRMTokenInnerNeutralLandClumps, kRMLong);
    AddDefine("InnerNeutralFlatChaos", kRMTokenInnerNeutralFlatChaos, kRMFloat);
    AddDefine("InnerNeutralFlatClumps", kRMTokenInnerNeutralFlatClumps, kRMLong);
    AddDefine("NumMoons", kRMTokenNumMoons, kRMLong);
    AddDefine("MoonSize", kRMTokenMoonSize, kRMLong);
    AddDefine("MoonResources", kRMTokenMoonResources, kRMLong);
    AddDefine("ForestFreeRadius", kRMTokenForestFreeRadius, kRMFloat);
    AddDefine("ForestsPerPlayer", kRMTokenForestsPerPlayer, kRMLong);
    AddDefine("ForestChaosLevel", kRMTokenForestChaosLevel, kRMFloat);
    AddDefine("MaxClumpsPerForest", kRMTokenMaxClumpsPerForest, kRMLong);
    AddDefine("ResourceToEdgeDistance", kRMTokenResourceToEdgeDistance, kRMLong);
    AddDefine("ResourceToWaterDistance", kRMTokenResourceToWaterDistance, kRMLong);
    AddDefine("MaxTreeLoopCount", kRMTokenMaxTreeLoopCount, kRMLong);
    AddDefine("MaximumTreesPerClump", kRMTokenMaximumTreesPerClump, kRMLong);
    AddDefine("DefineForest", kRMTokenDefineForest, kRMLong);
    AddDefine("AvailableForests", kRMTokenAvailableForests, kRMLong);
    AddDefine("ResourcePerPlayer", kRMTokenResourcePerPlayer, kRMLong);
    AddDefine("ResourcePerNeutralLand", kRMTokenResourcePerNeutralLand, kRMLong);
    AddDefine("MaxResourceElevation", kRMTokenMaxResourceElevation, kRMLong);
    AddDefine("MaxTreeElevation", kRMTokenMaxTreeElevation, kRMLong);
    AddDefine("ResourcePlacementLimit", kRMTokenResourcePlacementLimit, kRMLong);
    AddDefine("ResourceSeperation", kRMTokenResourceSeperation, kRMFloat);
    AddDefine("ResourceTerrainTexture", kRMTokenResourceTerrainTexture, kRMLong);
    AddDefine("PlaceResourceInRing", kRMTokenPlaceResourceInRing, kRMLong);
    AddDefine("DefineHerd", kRMTokenDefineHerd, kRMLong);
    AddDefine("DefineRandomHerdSet", kRMTokenDefineRandomHerdSet, kRMLong);
    AddDefine("HerdPerPlayer", kRMTokenHerdPerPlayer, kRMLong);
    AddDefine("HerdPerNeutral", kRMTokenHerdPerNeutral, kRMLong);
    AddDefine("RandomHerdPerPlayer", kRMTokenRandomHerdPerPlayer, kRMLong);
    AddDefine("RandomHerdPerNeutral", kRMTokenRandomHerdPerNeutral, kRMLong);
    AddDefine("ConvergentSampleSize", kRMTokenConvergentSampleSize, kRMFloat);
    AddDefine("ConvergentThreshold", kRMTokenConvergentThreshold, kRMFloat);
    AddDefine("MaxLoopCount", kRMTokenMaxLoopCount, kRMLong);
    AddDefine("MaxIterationsClumpGrowth", kRMTokenMaxIterationsClumpGrowth, kRMLong);
    AddDefine("FilterHeightMap", kRMTokenFilterHeightMap, kRMLong);
    AddDefine("TestingLandDistribution", kRMTokenTestingLandDistribution, kRMLong);
    AddDefine("TerrainRegion", kRMTokenTerrainRegion, kRMLong);
    AddDefine("TerrainChaos", kRMTokenTerrainChaos, kRMFloat);
    AddDefine("CliffTexture", kRMTokenCliffTexture, kRMLong);
    AddDefine("DeclareRandomMapTerrain", kRMTokenDeclareRandomMapTerrain, kRMLong);
    AddDefine("RandomMapTerrainRange", kRMTokenRandomMapTerrainRange, kRMLong);
    AddDefine("PlaceUnit", kRMTokenPlaceUnit, kRMFloat);
    AddDefine("PlaceWall", kRMTokenPlaceWall, kRMLong);
    AddDefine("PlaceWorldUnit", kRMTokenPlaceWorldUnit, kRMFloat);
    AddDefine("PlaceUnitInRing", kRMTokenPlaceUnitInRing, kRMFloat);
    AddDefine("PlaceWorldUnitInRing", kRMTokenPlaceWorldUnitInRing, kRMFloat);
    AddDefine("UseMapGenerator", kRMTokenUseMapGenerator, 0);
    AddDefine("StringTableID", kRMTokenStringTableID, 0);
    AddDefine("ShallowsElevation", kRMTokenShallowsElevation, kRMLong);
    AddDefine("ShallowsWidth", kRMTokenShallowsWidth, kRMLong);
    AddDefine("MaxShallowConnectionsPerPlayer", kRMTokenMaxShallowConnectionsPerPlayer, kRMLong);
    AddDefine("GenerateShallows", kRMTokenGenerateShallows, kRMLong);
    AddDefine("IsSpaceMap", kRMTokenIsSpaceMap, kRMLong);
    AddDefine("AvailableAnimals", kRMTokenAvailableAnimals, kRMLong);
    AddDefine("OuterPushFromMapCenter", kRMTokenOuterPushFromMapCenter, kRMLong);
    AddDefine("InnerPushFromMapCenter", kRMTokenInnerPushFromMapCenter, kRMLong);
    AddDefine("MinimumStartPositionToMapEdge", kRMTokenMinimumStartPositionToMapEdge, kRMLong);
    AddDefine("MapCharacteristics", kRMTokenMapCharacteristics, kRMLong);
}

// FUNCTION: DEFAULT 0x1000e68f
// Adds a script attribute or directive name with its token and value type
static void AddDefine(const UString& inName, long inToken, long inType)
{
    std::pair<UString, RMDefine> thePair;
    RMDefine theDefine;
    theDefine.mToken = inToken;
    theDefine.mType = inType;
    thePair.first = inName;
    thePair.second = theDefine;
    gDefines.insert(thePair);
}

// FUNCTION: DEFAULT 0x1000e703
// Fills the script constants: map types, booleans, map sizes, resource kinds, epochs, the game's
// start epoch, player count and map size, and every unit, terrain and cliff name
void InitializeMapTypes(EERandomMap* ioMap)
{
    UString theValue;
    unsigned long i;
    gMapTypes.clear();
    AddMapType("IslandMap", "1");
    AddMapType("WaterlessMap", "2");
    AddMapType("PlanetsMap", "4");
    AddMapType("EarthMap", "8");
    AddMapType("MarsMap", "16");
    AddMapType("true", "1");
    AddMapType("false", "0");
    AddMapType("Infinity", "0xFFFFFFFF");
    sprintf(theValue, "%d", ioMap->mStartEpoch);
    AddMapType("$StartEpoch", theValue);
    sprintf(theValue, "%d", ioMap->mPlayerCount);
    AddMapType("$PlayerCount", theValue);
    sprintf(theValue, "%d", ioMap->mMapSize);
    AddMapType("$MapSize", theValue);
    sprintf(theValue, "%d", 0);
    AddMapType("TinyMap", theValue);
    sprintf(theValue, "%d", 1);
    AddMapType("SmallMap", theValue);
    sprintf(theValue, "%d", 2);
    AddMapType("MediumMap", theValue);
    sprintf(theValue, "%d", 3);
    AddMapType("LargeMap", theValue);
    sprintf(theValue, "%d", 4);
    AddMapType("HugeMap", theValue);
    sprintf(theValue, "%d", 5);
    AddMapType("GiganticMap", theValue);
    sprintf(theValue, "%d", 0);
    AddMapType("Stone", theValue);
    sprintf(theValue, "%d", 1);
    AddMapType("Gold", theValue);
    sprintf(theValue, "%d", 2);
    AddMapType("Tree", theValue);
    sprintf(theValue, "%d", 3);
    AddMapType("Berry", theValue);
    sprintf(theValue, "%d", 4);
    AddMapType("Animal", theValue);
    sprintf(theValue, "%d", 5);
    AddMapType("Fish", theValue);
    sprintf(theValue, "%d", 6);
    AddMapType("Steel", theValue);
    AddMapType("Iron", theValue);
    AddMapType("PaleoEpoch", "0");
    AddMapType("StoneEpoch", "1");
    AddMapType("CopperEpoch", "2");
    AddMapType("BronzeEpoch", "3");
    AddMapType("DarkEpoch", "4");
    AddMapType("MiddleEpoch", "5");
    AddMapType("RenaissanceEpoch", "6");
    AddMapType("ImperialEpoch", "7");
    AddMapType("IndustrialEpoch", "8");
    AddMapType("WW1Epoch", "9");
    AddMapType("WW2Epoch", "10");
    AddMapType("ModernEpoch", "11");
    AddMapType("InformationEpoch", "12");
    AddMapType("NanoEpoch", "13");
    AddMapType("SpaceEpoch", "14");
    for (i = 0; i < ioMap->mUnitNames.size(); i++) {
        sprintf(theValue, "%d", i);
        AddMapType(ioMap->mUnitNames[i].mName, theValue);
    }
    for (i = 0; i < ioMap->mTerrainNames.size(); i++) {
        sprintf(theValue, "%d", ioMap->mTerrainNames[i].mValue);
        AddMapType(ioMap->mTerrainNames[i].mName, theValue);
    }
    for (i = 0; i < ioMap->mCliffNames.size(); i++) {
        sprintf(theValue, "%d", ioMap->mCliffNames[i].mValue);
        AddMapType(ioMap->mCliffNames[i].mName, theValue);
    }
}

// FUNCTION: DEFAULT 0x1000ef63
// Adds a script constant or #define with its text value
static void AddMapType(const UString& inName, const UString& inValue)
{
    std::pair<UString, UString> thePair;
    thePair.first = inName;
    thePair.second = inValue;
    gMapTypes.insert(thePair);
}


// FUNCTION: DEFAULT 0x1000efd7
// Fills the expression keyword table (Between, OneOf, AND/OR/XOR/NOT, comparisons, PERCENT, DEFINED)
void InitializeKeywords()
{
    gKeywords.clear();
    AddKeyword("Between", kRMBetween);
    AddKeyword("OneOf", kRMOneOf);
    AddKeyword("AND", kRMAnd);
    AddKeyword("OR", kRMOr);
    AddKeyword("XOR", kRMXor);
    AddKeyword("NOT", kRMNot);
    AddKeyword("EQ", kRMEqual);
    AddKeyword("NEQ", kRMNotEqual);
    AddKeyword("LT", kRMLess);
    AddKeyword("LTEQ", kRMLessEqual);
    AddKeyword("GT", kRMGreater);
    AddKeyword("GTEQ", kRMGreaterEqual);
    AddKeyword("PERCENT", kRMPercent);
    AddKeyword("DEFINED", kRMDefined);
    AddKeyword("NOTDEFINED", kRMNotDefined);
}

// FUNCTION: DEFAULT 0x1000f1c4
// Adds an expression keyword with its token
static void AddKeyword(const UString& inName, long inToken)
{
    std::pair<UString, long> thePair;
    thePair.first = inName;
    thePair.second = inToken;
    gKeywords.insert(thePair);
}

// FUNCTION: DEFAULT 0x1000f222
// Opens <name>.rmv in the map's script directory and parses it line by line; a missing file is skipped
void ReadScript(EERandomMap* ioMap, const UString& inName)
{
    FSFileSpec theFile(ioMap->mScriptDirectory);
    UString theLine(0x200);
    char theBuffer[0x200];
    FILE* theStream;
    bool theDone;
    theFile.SetFile(inName + ".rmv");
    theFile.GetVolume() + theFile.GetDirectory() + theFile.GetFile();
    theStream = fopen(theFile.GetVolume() + theFile.GetDirectory() + theFile.GetFile(), "r");
    if (theStream) {
        do {
            theDone = fgets(theBuffer, 0x200, theStream) == NULL;
            if (!theDone) {
                theLine = theBuffer;
                ParseLine(ioMap, theLine);
            }
        } while (!theDone);
        fclose(theStream);
    }
}

// NEAR: frame layout differs (original keeps theLong spilled at [ebp-0x30], 0x1a0 frame, DefineForest temp between theQuote and theUnused); control flow and every case body verified against the dispatch emulation
// FUNCTION: DEFAULT 0x1000f361
// Parses one script line: applies an attribute to the map settings (inside an active #if branch) or
// handles #define, #if/#elif/#else/#endif and #include; comment lines are skipped
void ParseLine(EERandomMap* ioMap, const UString& inLine)
{
    std::vector<float> theFloats;
    std::vector<long> theLongs;
    std::vector<UString> theTokens;
    std::vector<UString> theExpanded;
    UString theSeparators(" \t\n,");
    UString theQuote("\"");
    UString theBrackets("()");
    UString theUnused;
    float theFloat = 0.0f;
    long theLong = 0;
    long theSecond = 0;
    float theSecondFloat;
    unsigned long theToken;
    long theType;
    unsigned long i;
    unsigned long j;
    unsigned long theLow;
    unsigned long theHigh;
    EERMResourceKind theKind;
    EERMResourceSlot theSlot;
    EERMSpacingRule theRule;
    EERMFeature theFeature;
    EERMWall theWall;
    EERMTerrainRegion theRegion;
    EERMTerrain theTerrain;
    inLine.NestedTokenize(theSeparators, theBrackets, theTokens);
    if (theTokens.size() < 1 || theTokens[0].Find("//", 0) != theTokens[0].GetLength())
        return;
    ExpandMapTypes(theTokens, theExpanded);
    theToken = GetAttributeToken(theExpanded[0]);
    theType = GetAttributeType(theExpanded[0]);
    if (theToken >= kRMTokenMaxIntElevation && theToken != kRMTokenUseMapGenerator && theToken != kRMTokenStringTableID
        && (gIfStack.size() == 0 || gIfStack.back() == 1)) {
        if (theType == kRMLong) {
            ParseLongList(ioMap, theExpanded[1], theLongs);
            if (theLongs.size() >= 1) {
                theLong = theLongs[0];
                if (theLongs.size() >= 2)
                    theSecond = theLongs[1];
            }
        } else {
            ParseFloatList(ioMap, theExpanded[1], theFloats);
            if (theFloats.size() >= 1) {
                theFloat = theFloats[0];
                if (theFloats.size() >= 2)
                    theSecondFloat = theFloats[1];
            }
        }
        if (theToken > kRMFirstCustomToken) {
            if (gAttributeHandler)
                gAttributeHandler(ioMap, theToken, theType, theLongs, theFloats);
        } else switch (theToken) {
    case kRMTokenMaxIntElevation:  // MaxIntElevation
        ioMap->mMaxIntElevation = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenMinIntElevation:  // MinIntElevation
        ioMap->mMinIntElevation = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenInitialElevation:  // InitialElevation
        ioMap->mInitialElevation = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenFlatElevation:  // FlatElevation
        ioMap->mFlatElevation = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenElevationScale:  // ElevationScale
        ioMap->mElevationScale = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenHeightMapChaos:  // HeightMapChaos
        ioMap->mHeightMapChaos = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenHeightMapSmoothness:  // HeightMapSmoothness
        ioMap->mHeightMapSmoothness = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPercentLand:  // PercentLand
        ioMap->mPercentLand = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenLandElevationThreshold:  // LandElevationThreshold
        ioMap->mLandElevationThreshold = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenWaterBorder:  // WaterBorder
        ioMap->mWaterBorder = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenRiverWidth:  // RiverWidth
        ioMap->mRiverWidth = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenCoastalToDeeperWaterPercentage:  // CoastalToDeeperWaterPercentage
        ioMap->mCoastalToDeeperWaterPercentage = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenTeamGrowth:  // TeamGrowth
        ioMap->mTeamGrowth = theLong > 0;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenShallowsWidth:  // ShallowsWidth
        ioMap->mShallowsWidth = theLong;
        break;
    case kRMTokenMaxShallowConnectionsPerPlayer:  // MaxShallowConnectionsPerPlayer
        ioMap->mMaxShallowConnectionsPerPlayer = theLong;
        break;
    case kRMTokenGenerateShallows:  // GenerateShallows
        ioMap->mGenerateShallows = theLong == 1;
        break;
    case kRMTokenLandfillPercentage:  // LandfillPercentage
        ioMap->mLandfillPercentage = theFloat;
        break;
    case kRMTokenIsSpaceMap:  // IsSpaceMap
        ioMap->mIsSpaceMap = theLong == 1;
        break;
    case kRMTokenNumMoons:  // NumMoons
        ioMap->mNumMoons = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenMoonSize:  // MoonSize
        ioMap->mMoonSize = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenMoonResources:  // MoonResources
        ioMap->mMoonResources = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlayerInnerRadius:  // PlayerInnerRadius
        ioMap->mPlayerInnerRadius = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlayerOuterRadius:  // PlayerOuterRadius
        ioMap->mPlayerOuterRadius = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlayerPercentFlat:  // PlayerPercentFlat
        ioMap->mPlayerPercentFlat = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlayerTreePercentage:  // PlayerTreePercentage
        ioMap->mPlayerTreePercentage = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlayerFlatChaos:  // PlayerFlatChaos
        ioMap->mPlayerFlatChaos = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlayerFlatClumps:  // PlayerFlatClumps
        ioMap->mPlayerFlatClumps = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlayerLandChaos:  // PlayerLandChaos
        ioMap->mPlayerLandChaos = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlayerLandClumps:  // PlayerLandClumps
        ioMap->mPlayerLandClumps = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenMaxAngleBetweenTeamMembers:  // MaxAngleBetweenTeamMembers
        ioMap->mMaxAngleBetweenTeamMembers = theFloat * 0.017453292f;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenOuterPushFromMapCenter:  // OuterPushFromMapCenter
        ioMap->mOuterPushFromMapCenter = theLong;
        break;
    case kRMTokenInnerPushFromMapCenter:  // InnerPushFromMapCenter
        ioMap->mInnerPushFromMapCenter = theLong;
        break;
    case kRMTokenMinimumStartPositionToMapEdge:  // MinimumStartPositionToMapEdge
        ioMap->mMinimumStartPositionToMapEdge = theLong;
        break;
    case kRMTokenNeutralInnerRadius:  // NeutralInnerRadius
        ioMap->mNeutralInnerRadius = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenNeutralOuterRadius:  // NeutralOuterRadius
        ioMap->mNeutralOuterRadius = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenNumOuterNeutrals:  // NumOuterNeutrals
        ioMap->mNumOuterNeutrals = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenOuterNeutralPercentLand:  // OuterNeutralPercentLand
        ioMap->mOuterNeutralPercentLand = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenOuterNeutralPercentFlat:  // OuterNeutralPercentFlat
        ioMap->mOuterNeutralPercentFlat = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenOuterNeutralTreePercentage:  // OuterNeutralTreePercentage
        ioMap->mOuterNeutralTreePercentage = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenOuterNeutralLandChaos:  // OuterNeutralLandChaos
        ioMap->mOuterNeutralLandChaos = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenOuterNeutralLandClumps:  // OuterNeutralLandClumps
        ioMap->mOuterNeutralLandClumps = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenOuterNeutralFlatChaos:  // OuterNeutralFlatChaos
        ioMap->mOuterNeutralFlatChaos = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenOuterNeutralFlatClumps:  // OuterNeutralFlatClumps
        ioMap->mOuterNeutralFlatClumps = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenNumInnerNeutrals:  // NumInnerNeutrals
        ioMap->mNumInnerNeutrals = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenInnerNeutralPercentLand:  // InnerNeutralPercentLand
        ioMap->mInnerNeutralPercentLand = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenInnerNeutralPercentFlat:  // InnerNeutralPercentFlat
        ioMap->mInnerNeutralPercentFlat = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenInnerNeutralTreePercentage:  // InnerNeutralTreePercentage
        ioMap->mInnerNeutralTreePercentage = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenInnerNeutralLandChaos:  // InnerNeutralLandChaos
        ioMap->mInnerNeutralLandChaos = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenInnerNeutralLandClumps:  // InnerNeutralLandClumps
        ioMap->mInnerNeutralLandClumps = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenInnerNeutralFlatChaos:  // InnerNeutralFlatChaos
        ioMap->mInnerNeutralFlatChaos = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenInnerNeutralFlatClumps:  // InnerNeutralFlatClumps
        ioMap->mInnerNeutralFlatClumps = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenForestFreeRadius:  // ForestFreeRadius
        ioMap->mForestFreeRadius = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenForestsPerPlayer:  // ForestsPerPlayer
        ioMap->mForestsPerPlayer = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenForestChaosLevel:  // ForestChaosLevel
        ioMap->mForestChaosLevel = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenMaxClumpsPerForest:  // MaxClumpsPerForest
        ioMap->mMaxClumpsPerForest = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenMaxTreeLoopCount:  // MaxTreeLoopCount
        ioMap->mMaxTreeLoopCount = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenMaximumTreesPerClump:  // MaximumTreesPerClump
        ioMap->mMaximumTreesPerClump = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenDefineForest:  // DefineForest
        if (theLongs.size() > 2) {
            ioMap->mForests.push_back(EERMForest());
            ioMap->mForests.back().mId = theLongs[0];
            ioMap->mForests.back().mTerrainTexture = theLongs[1];
            for (i = 2; i < theLongs.size(); i++)
                ioMap->mForests.back().mTreeTypes.push_back(theLongs[i]);
        }
        break;
    case kRMTokenAvailableForests:  // AvailableForests
        ioMap->mTreeTypes.clear();
        for (i = 0; i < theLongs.size(); i++)
            ioMap->mTreeTypes.push_back(theLongs[i]);
        break;
    case kRMTokenResourcePerPlayer:  // ResourcePerPlayer
        if (theLong < 7 && theSecond >= 0)
            ioMap->mResourcePerPlayer[theLong] = theSecond;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenResourcePerNeutralLand:  // ResourcePerNeutralLand
        if (theLong < 7 && theSecond >= 0)
            ioMap->mResourcePerNeutralLand[theLong] = theSecond;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenMaxResourceElevation:  // MaxResourceElevation
        ioMap->mMaxResourceElevation = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenResourceToEdgeDistance:  // ResourceToEdgeDistance
        ioMap->mResourceToEdgeDistance = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenResourceToWaterDistance:  // ResourceToWaterDistance
        ioMap->mResourceToWaterDistance = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenAvailableAnimals:  // AvailableAnimals
        ioMap->mAvailableAnimals.clear();
        for (i = 0; i < theLongs.size(); i++)
            ioMap->mAvailableAnimals.push_back(theLongs[i]);
        break;
    case kRMTokenResourcePlacementLimit:  // ResourcePlacementLimit
        if (theLongs.size() == 3) {
            ioMap->mKindSpacing[theLongs[0]][0] = theLongs[1] * theLongs[1];
            ioMap->mKindSpacing[theLongs[0]][1] = theLongs[2] * theLongs[2];
        }
        break;
    case kRMTokenDefineHerd:  // DefineHerd
        if (theLongs.size() == 5) {
            theKind.mId = theLongs[0];
            theKind.mUnitType = theLongs[1];
            theKind.mArea = theLongs[2];
            theKind.mMinimum = theLongs[3];
            theKind.mMaximum = theLongs[4];
            ioMap->mResourceKinds.push_back(theKind);
        }
        break;
    case kRMTokenDefineRandomHerdSet:  // DefineRandomHerdSet
        ioMap->mResourceKindIds.clear();
        for (i = 0; i < theLongs.size(); i++)
            ioMap->mResourceKindIds.push_back(theLongs[i]);
        break;
    case kRMTokenHerdPerPlayer:  // HerdPerPlayer
        if (theLongs.size() == 2) {
            theSlot.mId = theLongs[0];
            theSlot.mPerPlayer = true;
            theSlot.mRandomKind = false;
            for (i = 0; i < theLongs[1] && theLongs[1] > 0; i++)
                ioMap->mResourceSlots.push_back(theSlot);
        }
        break;
    case kRMTokenHerdPerNeutral:  // HerdPerNeutral
        if (theLongs.size() == 2) {
            theSlot.mId = theLongs[0];
            theSlot.mPerPlayer = false;
            theSlot.mRandomKind = false;
            for (i = 0; i < theLongs[1] && theLongs[1] > 0; i++)
                ioMap->mResourceSlots.push_back(theSlot);
        }
        break;
    case kRMTokenRandomHerdPerPlayer:  // RandomHerdPerPlayer
        if (theLongs.size() == 1) {
            theSlot.mId = 0;
            theSlot.mPerPlayer = true;
            theSlot.mRandomKind = true;
            for (i = 0; i < theLongs[0] && theLongs[0] > 0; i++)
                ioMap->mResourceSlots.push_back(theSlot);
        }
        break;
    case kRMTokenRandomHerdPerNeutral:  // RandomHerdPerNeutral
        if (theLongs.size() == 1) {
            theSlot.mId = 0;
            theSlot.mPerPlayer = false;
            theSlot.mRandomKind = true;
            for (i = 0; i < theLongs[0] && theLongs[0] > 0; i++)
                ioMap->mResourceSlots.push_back(theSlot);
        }
        break;
    case kRMTokenMaxTreeElevation:  // MaxTreeElevation
        ioMap->mMaxTreeElevation = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenResourceSeperation:  // ResourceSeperation
        ioMap->mResourceSeperation = theFloat;
        break;
    case kRMTokenResourceTerrainTexture:  // ResourceTerrainTexture
        if (theLongs.size() == 2 && theLongs[0] < 7)
            ioMap->mResourceTerrainTexture[theLongs[0]] = theLongs[1];
        break;
    case kRMTokenPlaceResourceInRing:  // PlaceResourceInRing
        if (theLongs.size() == 3) {
            theRule.mKind = theLongs[0];
            if ((unsigned long)theRule.mKind < kRMObjectCount) {
                theRule.mMinimum = theLongs[1];
                theRule.mMaximum = theLongs[2];
                ioMap->mSpacingRules.push_back(theRule);
            }
        }
        break;
    case kRMTokenPlaceUnit:  // PlaceUnit
        if (theFloats.size() >= 3) {
            theFeature.mUnitType = (long)theFloats[0];
            theFeature.mPosition.mX = theFloats[1];
            theFeature.mPosition.mY = theFloats[2];
            theFeature.mIsWorldUnit = false;
            theFeature.mInRing = false;
            theFeature.mIsHerd = false;
            if (theFloats.size() == 4)
                theFeature.mAngle = theFloats[3] * 0.017453292f;
            else
                theFeature.mAngle = 3.1415927f;
            ioMap->mFeatures.push_back(theFeature);
        }
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlaceWall:  // PlaceWall
        if (theLongs.size() == 5) {
            theWall.mUnitType = theLongs[0];
            theWall.mStartX = theLongs[1];
            theWall.mStartY = theLongs[2];
            theWall.mEndX = theLongs[3];
            theWall.mEndY = theLongs[4];
            ioMap->mWalls.push_back(theWall);
        }
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlaceWorldUnit:  // PlaceWorldUnit
        if (theFloats.size() >= 3) {
            theFeature.mUnitType = (long)theFloats[0];
            theFeature.mPosition.mX = theFloats[1];
            theFeature.mPosition.mY = theFloats[2];
            theFeature.mIsWorldUnit = true;
            theFeature.mInRing = false;
            theFeature.mIsHerd = false;
            if (theFloats.size() == 4)
                theFeature.mAngle = theFloats[3] * 0.017453292f;
            else
                theFeature.mAngle = 3.1415927f;
            ioMap->mFeatures.push_back(theFeature);
        }
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlaceUnitInRing:  // PlaceUnitInRing
        if (theFloats.size() >= 3) {
            theFeature.mUnitType = (long)theFloats[0];
            theFeature.mPosition.mX = theFloats[1];
            theFeature.mPosition.mY = theFloats[2];
            theFeature.mIsWorldUnit = false;
            theFeature.mInRing = true;
            theFeature.mIsHerd = false;
            if (theFloats.size() == 4)
                theFeature.mAngle = theFloats[3] * 0.017453292f;
            else
                theFeature.mAngle = 3.1415927f;
            ioMap->mFeatures.push_back(theFeature);
        }
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenPlaceWorldUnitInRing:  // PlaceWorldUnitInRing
        if (theFloats.size() >= 3) {
            theFeature.mUnitType = (long)theFloats[0];
            theFeature.mPosition.mX = theFloats[1];
            theFeature.mPosition.mY = theFloats[2];
            theFeature.mIsWorldUnit = true;
            theFeature.mInRing = true;
            theFeature.mIsHerd = false;
            if (theFloats.size() == 4)
                theFeature.mAngle = theFloats[3] * 0.017453292f;
            else
                theFeature.mAngle = 3.1415927f;
            ioMap->mFeatures.push_back(theFeature);
        }
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenShallowsElevation:  // ShallowsElevation
        ioMap->mShallowsElevation = theLong;
        break;
    case kRMTokenTerrainRegion:  // TerrainRegion
        if (theLongs.size() == 3) {
            theRegion.mFrom = theLongs[0];
            theRegion.mTo = theLongs[1];
        }
        RMTrace(theExpanded[0], theLongs[0], theLongs[1], theLongs[2]);
        for (i = 0; i < ioMap->mTerrains.size(); i++)
            if (theLongs[2] == ioMap->mTerrains[i].mId) {
                theRegion.mTerrain = i;
                ioMap->mTerrainRegions.push_back(theRegion);
                break;
            }
        if (i >= ioMap->mTerrains.size())
            RMTrace(theExpanded[0]);
        break;
    case kRMTokenTerrainChaos:  // TerrainChaos
        ioMap->mTerrainChaos = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenCliffTexture:  // CliffTexture
        ioMap->mCliffTexture = theLong;
        break;
    case kRMTokenDeclareRandomMapTerrain:  // DeclareRandomMapTerrain
        if (theLongs.size() == 2) {
            for (i = 0; i < ioMap->mTerrains.size(); i++)
                if (ioMap->mTerrains[i].mId == theLong)
                    break;
            if (i == ioMap->mTerrains.size()) {
                theTerrain.mId = theLong;
                for (j = 0; j < 31; j++)
                    theTerrain.mTextures[j] = theSecond;
                ioMap->mTerrains.push_back(theTerrain);
            }
        }
        break;
    case kRMTokenRandomMapTerrainRange:  // RandomMapTerrainRange
        if (theLongs.size() == 4) {
            for (i = 0; i < ioMap->mTerrains.size(); i++)
                if (ioMap->mTerrains[i].mId == theLongs[0]) {
                    theLow = theLongs[1] + 10;
                    theHigh = theLongs[2] + 10;
                    if (theLow <= 30 && theHigh <= 30 && theLow <= theHigh)
                        for (j = theLow; j <= theHigh; j++)
                            ioMap->mTerrains[i].mTextures[j] = theLongs[3];
                    break;
                }
        }
        break;
    case kRMTokenConvergentSampleSize:  // ConvergentSampleSize
        ioMap->mConvergentSampleSize = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenConvergentThreshold:  // ConvergentThreshold
        ioMap->mConvergentThreshold = theFloat;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenMaxLoopCount:  // MaxLoopCount
        ioMap->mMaxLoopCount = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenMaxIterationsClumpGrowth:  // MaxIterationsClumpGrowth
        ioMap->mMaxIterationsClumpGrowth = theLong;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenMapCharacteristics:  // MapCharacteristics
        ioMap->mMapCharacteristics = 0;
        for (i = 0; i < theLongs.size(); i++)
            ioMap->mMapCharacteristics |= theLongs[i];
        break;
    case kRMTokenFilterHeightMap:  // FilterHeightMap
        ioMap->mFilterHeightMap = theLong > 0;
        RMTrace(theExpanded[0]);
        break;
    case kRMTokenTestingLandDistribution:  // TestingLandDistribution
        ioMap->mTestingLandDistribution = theLong > 0;
        RMTrace(theExpanded[0]);
        break;
    default:
        RMTrace(theExpanded[0]);
        break;
    }
    } else {
        switch (theToken) {
        case kRMTokenDefine:  // #define
            if (gIfStack.size() == 0 || gIfStack.back() == 1) {
                theExpanded[1].NestedTokenize(theSeparators, theBrackets, theTokens);
                ExpandMapTypes(theTokens, theExpanded);
                AddMapType(theTokens[0], theExpanded[1]);
            }
            break;
        case kRMTokenIf:  // #if
            if (gIfStack.size() != 0 && gIfStack.back() == 2)
                gIfStack.push_back(2);
            else if (gIfStack.size() != 0 && gIfStack.back() == 0)
                gIfStack.push_back(2);
            else if (EvaluateCondition(ioMap, theExpanded[1]))
                gIfStack.push_back(1);
            else
                gIfStack.push_back(0);
            break;
        case kRMTokenElif:  // #elif
            if (gIfStack.back() != 2) {
                if (gIfStack.back() == 0) {
                    if (EvaluateCondition(ioMap, theExpanded[1]))
                        gIfStack[gIfStack.size() - 1] = 1;
                } else
                    gIfStack[gIfStack.size() - 1] = 2;
            }
            break;
        case kRMTokenElse:  // #else
            if (gIfStack.back() != 2) {
                if (gIfStack.back() == 0)
                    gIfStack[gIfStack.size() - 1] = 1;
                else
                    gIfStack[gIfStack.size() - 1] = 2;
            }
            break;
        case kRMTokenEndif:  // #endif
            gIfStack.pop_back();
            break;
        case kRMTokenInclude:  // #include
            if (gIfStack.size() == 0 || gIfStack.back() == 1) {
                inLine.Tokenize(theQuote, theTokens);
                ReadScript(ioMap, theTokens[1]);
            }
            break;
        }
    }
}

// NEAR: the original inlines begin() for the first theExpanded[0] (operator+ out of line); budget pattern
// FUNCTION: DEFAULT 0x100111dc
// Evaluates an #if condition: a number, a logical/comparison keyword on nested conditions, PERCENT
// (true with that chance), or DEFINED/NOTDEFINED of a constant; returns 1 or 0
long EvaluateCondition(EERandomMap* ioMap, const UString& inText)
{
    std::vector<UString> theTokens;
    std::vector<UString> theExpanded;
    UString theSeparators(" \t(),");
    long theResult = 0;
    unsigned long theToken;
    long theA;
    long theB;
    bool theDefined;
    inText.Tokenize(theSeparators, theTokens);
    ExpandMapTypes(theTokens, theExpanded);
    theToken = GetKeyword(theExpanded[0]);
    if (theExpanded.size() == 1) {
        if (sscanf(theExpanded[0], "%d", &theResult) != 1 && gReportErrors)
            printf("%s (%d) : ERROR, unknown token encountered, %s\n",
                   (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)theExpanded[0]);
    } else if (theToken >= kRMAnd && theToken <= kRMPercent) {
        if (gReportErrors && (theExpanded.size() < 2 || theExpanded.size() > 3))
            printf("%s (%d) : ERROR, too many parameters for keyword %s\n",
                   (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)theExpanded[0]);
        theA = EvaluateCondition(ioMap, theExpanded[1]);
        if (theExpanded.size() == 3)
            theB = EvaluateCondition(ioMap, theExpanded[2]);
        else
            theB = 0;
        switch (theToken) {
        case kRMAnd:
            if (theA && theB)
                theResult = 1;
            else
                theResult = 0;
            break;
        case kRMOr:
            if (theA || theB)
                theResult = 1;
            else
                theResult = 0;
            break;
        case kRMXor:
            if ((theA || theB) && theA != theB)
                theResult = 1;
            else
                theResult = 0;
            break;
        case kRMNot:
            theResult = !theA;
            break;
        case kRMEqual:
            theResult = theA == theB;
            break;
        case kRMNotEqual:
            theResult = theA != theB;
            break;
        case kRMLess:
            theResult = theA < theB;
            break;
        case kRMLessEqual:
            theResult = theA <= theB;
            break;
        case kRMGreater:
            theResult = theA > theB;
            break;
        case kRMGreaterEqual:
            theResult = theA >= theB;
            break;
        case kRMPercent:
            if (theA == 0)
                theResult = 0;
            else if (theA == 100)
                theResult = 1;
            else
                theResult = ioMap->mRandomIntegers.Generate(0, 99) <= theA - 1;
            if (gReportErrors && (theA < 0 || theA > 100))
                printf("%s (%d) : ERROR, out-of-range value specified, %d, in PERCENT. Valid range is 0 to 100.\n",
                       (const char*)gFileNames.back(), gLineNumbers.back(), theA);
            break;
        }
    } else if (theToken < kRMDefined || theToken > kRMNotDefined) {
        // Fixed: tested `< DEFINED && > NOTDEFINED`, never true, so unknown keywords fell into the DEFINED block
        // (which also yields 0 for them)
        theResult = 0;
        if (gReportErrors)
            printf("%s (%d) : ERROR, unknown keyword %s\n",
                   (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)theExpanded[0]);
    } else {
        if (gReportErrors && theExpanded.size() != 2)
            printf("%s (%d) : ERROR, too many parameters for keyword %s\n",
                   (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)theExpanded[0]);
        theDefined = gMapTypes.find(theTokens[1]) != gMapTypes.end();
        switch (theToken) {
        case kRMDefined:
            theResult = theDefined;
            break;
        case kRMNotDefined:
            theResult = !theDefined;
            break;
        }
    }
    return theResult;
}

// FUNCTION: DEFAULT 0x100115e9
// Returns the token of a script attribute or directive, or 0 when it is unknown
long GetAttributeToken(const UString& inName)
{
    long theResult;
    std::map<UString, RMDefine>::iterator theEntry = gDefines.find(inName);
    if (theEntry != gDefines.end())
        theResult = (*theEntry).second.mToken;
    else {
        theResult = 0;
        if (gReportErrors)
            printf("%s (%d) : ERROR, unknown attribute \"%s\" detected.\n",
                   (const char*)gFileNames.back(), gLineNumbers.back(), (const char*)inName);
    }
    return theResult;
}

// FUNCTION: DEFAULT 0x10011641
// Returns the value type of a script attribute (integer, float, 0 for directives)
long GetAttributeType(const UString& inName)
{
    std::map<UString, RMDefine>::iterator theEntry = gDefines.find(inName);
    if (theEntry != gDefines.end())
        return (*theEntry).second.mType;
    return 0;
}

// FUNCTION: DEFAULT 0x10011668
// Sets every random map script setting to its default before a script is read
void SetDefaults(EERandomMap* ioMap)
{
    long i;
    ioMap->mSpacingRules.clear();
    ioMap->mTerrains.clear();
    ioMap->mTerrainRegions.clear();
    for (i = 0; i < kRMObjectCount; i++) {
        ioMap->mKindSpacing[i][0] = 0;
        ioMap->mKindSpacing[i][1] = -1;
        ioMap->mResourcePerPlayer[i] = 0;
        ioMap->mResourcePerNeutralLand[i] = 0;
        ioMap->mResourceTerrainTexture[i] = -1;
    }
    ioMap->mCliffTexture = -1;
    ioMap->mResourceSeperation = kDefaultResourceSeperation;
    ioMap->mMapCharacteristics = 0;
    ioMap->mLandElevationThreshold = 0.0f;
    ioMap->mInitialElevation = 0.0f;
    ioMap->mHeightMapChaos = kDefaultHeightMapChaos;
    ioMap->mHeightMapSmoothness = kDefaultHeightMapSmoothness;
    ioMap->mConvergentSampleSize = kDefaultConvergentSampleSize;
    ioMap->mConvergentThreshold = kDefaultConvergentThreshold;
    ioMap->mPercentLand = kDefaultPercentLand;
    ioMap->mPlayerPercentFlat = kDefaultPlayerPercentFlat;
    ioMap->mPlayerInnerRadius = kDefaultPlayerInnerRadius;
    ioMap->mPlayerOuterRadius = kDefaultPlayerOuterRadius;
    ioMap->mLandfillPercentage = kDefaultF2FC;
    ioMap->mNumMoons = 0;
    ioMap->mMoonSize = 5;
    ioMap->mMoonResources = 5;
    ioMap->mNeutralInnerRadius = kDefaultNeutralInnerRadius;
    ioMap->mNeutralOuterRadius = kDefaultNeutralOuterRadius;
    ioMap->mMaxAngleBetweenTeamMembers = kDefaultMaxAngleBetweenTeamMembers;
    ioMap->mMaxLoopCount = 2000;
    ioMap->mMaxIntElevation = 20;
    ioMap->mMinIntElevation = -10;
    ioMap->mFlatElevation = 0.0f;
    ioMap->mMaxTreeElevation = ioMap->mMaxResourceElevation = 20.0f;
    ioMap->mMaxIterationsClumpGrowth = 1000;
    ioMap->mForestChaosLevel = kDefaultForestChaosLevel;
    ioMap->mMaxClumpsPerForest = 4;
    ioMap->mForestsPerPlayer = 3;
    ioMap->mMaxTreeLoopCount = 1000;
    ioMap->mFilterHeightMap = true;
    ioMap->mTestingLandDistribution = false;
    ioMap->mResourceToWaterDistance = 3;
    ioMap->mResourceToEdgeDistance = 3;
    ioMap->mWaterBorder = 7;
    ioMap->mForestFreeRadius = kDefaultForestFreeRadius;
    ioMap->mMaximumTreesPerClump = 10000;
    ioMap->mInnerNeutralFlatChaos = kDefaultInnerNeutralFlatChaos;
    ioMap->mInnerNeutralLandChaos = kDefaultInnerNeutralLandChaos;
    ioMap->mInnerNeutralTreePercentage = kDefaultInnerNeutralTreePercentage;
    ioMap->mInnerNeutralPercentFlat = kDefaultInnerNeutralPercentFlat;
    ioMap->mInnerNeutralPercentLand = kDefaultInnerNeutralPercentLand;
    ioMap->mOuterNeutralFlatChaos = kDefaultOuterNeutralFlatChaos;
    ioMap->mOuterNeutralLandChaos = kDefaultOuterNeutralLandChaos;
    ioMap->mOuterNeutralTreePercentage = kDefaultOuterNeutralTreePercentage;
    ioMap->mOuterNeutralPercentFlat = kDefaultOuterNeutralPercentFlat;
    ioMap->mOuterNeutralPercentLand = kDefaultOuterNeutralPercentLand;
    ioMap->mPlayerFlatChaos = kDefaultPlayerFlatChaos;
    ioMap->mPlayerLandChaos = kDefaultPlayerLandChaos;
    ioMap->mPlayerTreePercentage = kDefaultPlayerTreePercentage;
    ioMap->mNumInnerNeutrals = 1;
    ioMap->mNumOuterNeutrals = 1;
    ioMap->mInnerNeutralFlatClumps = 3;
    ioMap->mInnerNeutralLandClumps = 3;
    ioMap->mOuterNeutralFlatClumps = 3;
    ioMap->mOuterNeutralLandClumps = 3;
    ioMap->mPlayerFlatClumps = 3;
    ioMap->mPlayerLandClumps = 3;
    ioMap->mCoastalToDeeperWaterPercentage = kDefaultCoastalToDeeperWaterPercentage;
    ioMap->mRiverWidth = 0;
    ioMap->mTeamGrowth = false;
    ioMap->mTerrainChaos = kDefaultTerrainChaos;
    ioMap->mShallowsElevation = -1.0f;
    ioMap->mShallowsWidth = 0;
    ioMap->mMaxShallowConnectionsPerPlayer = 0;
    ioMap->mGenerateShallows = false;
    ioMap->mIsSpaceMap = false;
    ioMap->mAvailableAnimals.clear();
    ioMap->mOuterPushFromMapCenter = 0;
    ioMap->mInnerPushFromMapCenter = 0;
    ioMap->mMinimumStartPositionToMapEdge = 0;
}
