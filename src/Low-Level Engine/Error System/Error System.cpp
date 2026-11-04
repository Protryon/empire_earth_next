// C:\EEX\Low-Level Engine\Error System\Error System.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// First unit in link order: it also emits the inline members of the headers it is the first to include
// (UWideString/UString 0x10001000.., FSFile/FSBasicFile/FSFileSpec 0x10001279..0x1000144b).
// Complete: the debug-output filter (0x100016fb), the shutdown hook (0x100019cc), the globals' init chains
// (0x1000144b..0x100014f4) and the 0x10001a96.. COMDATs included.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

// Not the lle/LowLevelEngine.h umbrella: the first unit emits the exported inline members of every class it sees,
// and the original's block holds only UWideString (first), UString, FSFile, FSBasicFile and FSFileSpec.
#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSFile.h"
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
#include "lle/TSScheduler.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10001279
// SYNTHETIC: LLE 0x10001282
// SYNTHETIC: LLE 0x10001289
// SYNTHETIC: LLE 0x10001294
// SYNTHETIC: LLE 0x1000130d
// SYNTHETIC: LLE 0x10001336
// SYNTHETIC: LLE 0x1000137b
// SYNTHETIC: LLE 0x100013d6
// SYNTHETIC: LLE 0x100013e8
// SYNTHETIC: LLE 0x10001420
// SYNTHETIC: LLE 0x10001434

namespace std {
// Dinkum 2.33 (sxlrt233.dll export ?gPointerToAssertFunction@std@@3P6AXPBDK0@ZA)
extern __declspec(dllimport) void (*gPointerToAssertFunction)(const char*, unsigned long, const char*);
}

// Globals in definition order (= initializer order 0x1000144b..: the filters, the recording file, the lock holder).
// Addresses in analysis/lle/names.txt (provisional names).
static std::vector<UString*> gFilters;            // 0x1010f6b8: the lines of Filter.txt (ESFilterDebugOutput)
static FSFileSpec gRecordingFile;                 // 0x1010f670

// Owns a critical section for its lifetime. Nothing else references the object (0x1010f668): its constructor and
// destructor are its only users. The ESVERIFY is the original's line 65 (0x41) (provisional name).
class ESLockHolder {
public:
    ESLockHolder()
    {
        ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mLock) == keseOK, 65);
    }
    ~ESLockHolder()
    {
        TSScheduler::DestroyCriticalSection(this->mLock);
    }

    TSCriticalSection* mLock;
};

static ESLockHolder gLockHolder;                  // 0x1010f668
static TSCriticalSection* gLock;                  // 0x1010f6c8
static unsigned long gRecordingSuffix;            // 0x1010f6e0: 0..9, the "%d_" of the next recording
static bool gRecordToDebuggerOnly;                // 0x1010f6ac (provisional name)
static bool gRecording;                           // 0x1010f6d8
static char* gRecordingStart;                     // 0x1010f6cc (copied from 0x1010f6b4) (provisional name)
static char* gX6b4;                               // 0x1010f6b4: never assigned; deleted at shutdown (provisional name)
static bool gInitialized;                         // 0x1010f6da
static HINSTANCE gInstance;                       // 0x1010f6d0
static unsigned long gNativeError;                // 0x1010f6d4
static bool gInVerify;                            // 0x1010f6d9
static long gFilterMode = 1;                      // 0x100d85d0: 1 = Filter.txt lists what to suppress ("-"),
                                                  // 0 = what to show ("+")
static bool gOutputToDebugger;                    // 0x1010f6db ("OutputToDebugger" line; unread) (provisional name)
LLE_API bool gErrorSystemFiltersInitialized;      // 0x1010f6dc (exported)

bool ESFilterDebugOutput(const char* inText);     // 0x100016fb (defined after ESRecordNativeError, as in the original)
void ESShutdown(bool inForced);                   // 0x100019cc

// FUNCTION: LLE 0x100014f4
// Starts a numbered error log (0..9, rotating) in the given directory; opens it for writing unless debugger-only
long ESBeginRecording(FSFileSpec& inSpec)
{
    UString theName;
    TMTime theTime;
    gLock->Enter();
    TMGetTimeOfDay(theTime);
    sprintf(theName, "%d_%s", gRecordingSuffix, "Error.log");
    gRecordingSuffix++;
    if (gRecordingSuffix >= 10)
        gRecordingSuffix = 0;
    gRecordingFile = inSpec;
    gRecordingFile.SetFile(theName);
    long theResult = gRecordingFile.Create();
    if (theResult == kErrorOK || theResult == kErrorAlreadyExists) {
        gRecording = true;
        if (!gRecordToDebuggerOnly) {
            if (gRecordingFile.FOpen("w") != kErrorOK) {
                ESDebugOut("!::ESReport: error opening error log with write access.\n");
                gRecording = false;
            }
            gRecordingStart = gX6b4;
        }
        theResult = kErrorOK;
    }
    gLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100015cf
// Formats a debug message and sends it to the debugger when the Filter.txt rules let it through
void ESDebugOut(const char* inFormat, ...)
{
    char theText[1024];
    va_list theArguments;
    va_start(theArguments, inFormat);
    vsprintf(theText, inFormat, theArguments);
    if (ESFilterDebugOutput(theText))
    {
        const char* theOutput = theText;
        if (theText[0] == '!')
            theOutput = theText + 1;
        OutputDebugStringA(theOutput);
    }
}

// No marker: exported, empty in this build; ICF-folded into the bare ret at 0x10080c07 (UMath.cpp).
void ESDumpStack(unsigned long inDepth)
{
}

// FUNCTION: LLE 0x1000161d
// Stops error-log recording and closes the log file
long ESEndRecording()
{
    long theResult = kErrorOK;
    gLock->Enter();
    if (gRecording) {
        // Fixed: the log was closed only in debugger-only mode, the one mode in which ESBeginRecording does not
        // open it, so the opened log was never closed (Close does nothing for a file that is not open)
        theResult = gRecordingFile.Close();
        if (theResult != kErrorOK)
            ESDebugOut("!::ESReport: error closing error log file.\n");
        gRecording = false;
    } else
        theResult = kErrorAlreadyExists;
    gLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10001673
// Returns the last nonzero native (Win32) error recorded
unsigned long ESGetNativeError()
{
    return gNativeError;
}

// FUNCTION: LLE 0x10001679
// Initialises the error system: records the instance, registers the shutdown hook, creates the lock
void ESInitialize(HINSTANCE inInstance)
{
    gInitialized = true;
    gInstance = inInstance;
    URegisterShutdown(ESShutdown);
    std::gPointerToAssertFunction = NULL;
    long theResult = TSScheduler::CreateCriticalSection(gLock);
    ESVERIFY_AT(theResult == keseOK, 671);
}

// FUNCTION: LLE 0x100016c4
// Formats a quiet verification failure and discards it (no output in the retail build)
void ESQuietVerify(const char* inFile, unsigned long inLine, const char* inExpression)
{
    char theText[1024];
    sprintf(theText, "%s (%d) : Quiet verification failure (%s)", inFile, inLine, inExpression);
}

// FUNCTION: LLE 0x100016ed
// Records a nonzero native error and returns it unchanged
unsigned long ESRecordNativeError(unsigned long inError)
{
    if (inError != 0)
        gNativeError = inError;
    return inError;
}

// No marker: exported, empty in this build; ICF-folded into the bare ret at 0x10080c07 (UMath.cpp).
void ESReportMessage(const char* inFormat, ...)
{
}

// FUNCTION: LLE 0x100016fb
// Loads Filter.txt once, then returns whether a debug message passes the filters ('!' always passes)
// Filter.txt lives in the startup directory; "-" / "+" lines set the filter mode, "OutputToDebugger" sets a flag
// nothing reads.
bool ESFilterDebugOutput(const char* inText)
{
    char theLine[1025];                         // 0x404 bytes; fgets is told 1024
    FSFileSpec* theSpec;
    FILE* theFile;
    UString* theFilter;
    bool theResult;
    if (!gErrorSystemFiltersInitialized && gStartupVolume.GetLength() > 0 && gStartupDirectory.GetLength() > 0) {
        theSpec = new FSFileSpec;
        theSpec->SetVolume(gStartupVolume);
        theSpec->SetDirectory(gStartupDirectory);
        theSpec->SetFile(UString("Filter.txt"));
        gFilters.clear();
        if (theSpec->FOpen("r") == kErrorOK) {
            theFile = *theSpec;
            while (fgets(theLine, 1024, theFile) != NULL) {
                unsigned long theLength = strlen(theLine);
                if (theLength <= 0)
                    continue;
                if (theLine[theLength - 1] == '\n')
                    theLine[theLength - 1] = 0;
                theFilter = new UString(theLine);
                if (gFilters.size() == 0) {
                    if (strcmp(theLine, "-") == 0) {
                        gFilterMode = 1;
                        delete theFilter;
                    } else if (strcmp(theLine, "+") == 0) {
                        gFilterMode = 0;
                        delete theFilter;
                    } else if (strcmp(theLine, "OutputToDebugger") == 0) {
                        gOutputToDebugger = true;
                        delete theFilter;
                    } else if (theFilter != NULL)
                        gFilters.push_back(theFilter);
                // Reproduced as found: ';' converts through UString(unsigned long), the capacity
                // constructor, i.e. the filter is compared with an empty string
                } else if (theFilter != NULL && *theFilter != ';')
                    gFilters.push_back(theFilter);
            }
        }
        gErrorSystemFiltersInitialized = true;
        delete theSpec;
    }
    if (inText[0] == '!')
        theResult = true;
    else if (gFilters.size() == 0)
        theResult = gFilterMode == 1;
    else if (gFilterMode == 1) {
        theResult = true;
        for (unsigned long i = 0; i < gFilters.size() && theResult; i++) {
            theFilter = gFilters[i];
            if (strstr(inText, *theFilter))
                theResult = false;
        }
    } else {
        theResult = false;
        for (unsigned long i = 0; i < gFilters.size() && !theResult; i++) {
            theFilter = gFilters[i];
            if (strstr(inText, *theFilter))
                theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x100019cc
// Shuts the error system down: frees the lock and filters, closes the log if it is still open
// URegisterShutdown hook (ESInitialize)
void ESShutdown(bool inForced)
{
    TSScheduler::DestroyCriticalSection(gLock);
    while (gFilters.size() > 0) {
        UString* theFilter = gFilters.back();
        gFilters.pop_back();
        delete theFilter;
    }
    // Fixed: closed the log only in debugger-only mode, in which it is never opened
    if (gRecordingFile.Close() != kErrorOK)
        ESDebugOut("!::ESReport: error closing error log file.\n");
    gInitialized = false;
    gInstance = NULL;
    delete gX6b4;
}

// FUNCTION: LLE 0x10001a8f
// Breaks into the debugger, then aborts the process (retail build)
// int3; jmp [__imp__abort] (provisional name)
static void ESShowVerification(const char* inText)
{
    __asm int 3
    abort();
}

// FUNCTION: LLE 0x10001a4a
// Reports a failed verification (file, line, expression) and aborts; a nested failure is ignored
void ESVerify(const char* inFile, unsigned long inLine, const char* inExpression)
{
    char theText[1024];
    if (gInVerify)
        return;
    gInVerify = true;
    sprintf(theText, "%s (%d) : Verification failure (%s)", inFile, inLine, inExpression);
    ESShowVerification(theText);
}

// Inline members of UWideString/UString emitted here (first unit to use them; markers at the end):
// SYNTHETIC: LLE 0x10001000
// SYNTHETIC: LLE 0x1000100e
// SYNTHETIC: LLE 0x1000102a
// SYNTHETIC: LLE 0x1000104b
// SYNTHETIC: LLE 0x10001063
// SYNTHETIC: LLE 0x10001080
// SYNTHETIC: LLE 0x1000109c
// SYNTHETIC: LLE 0x100010bd
// SYNTHETIC: LLE 0x100010d5
// SYNTHETIC: LLE 0x100010f2
// SYNTHETIC: LLE 0x100010fd
// SYNTHETIC: LLE 0x10001108
// SYNTHETIC: LLE 0x10001136
// SYNTHETIC: LLE 0x10001140
// SYNTHETIC: LLE 0x10001159
// SYNTHETIC: LLE 0x10001178
// SYNTHETIC: LLE 0x1000118e
// SYNTHETIC: LLE 0x100011aa
// SYNTHETIC: LLE 0x100011c4
// SYNTHETIC: LLE 0x100011e3
// SYNTHETIC: LLE 0x100011fa
// SYNTHETIC: LLE 0x10001216
// SYNTHETIC: LLE 0x10001220
// SYNTHETIC: LLE 0x10001227
// SYNTHETIC: LLE 0x10001250

// Init chains (gFilters, gRecordingFile, gLockHolder), the vector destructor iterator and the vector<UString*> /
// exception COMDATs this unit is the first to emit:
// SYNTHETIC: LLE 0x100012e3
// SYNTHETIC: LLE 0x1000144b
// SYNTHETIC: LLE 0x10001455
// SYNTHETIC: LLE 0x1000146d
// SYNTHETIC: LLE 0x10001479
// SYNTHETIC: LLE 0x10001483
// SYNTHETIC: LLE 0x1000148d
// SYNTHETIC: LLE 0x10001497
// SYNTHETIC: LLE 0x100014a3
// SYNTHETIC: LLE 0x100014ad
// SYNTHETIC: LLE 0x100014b7
// SYNTHETIC: LLE 0x100014db
// SYNTHETIC: LLE 0x100014e7
// TEMPLATE: LLE 0x10001a96
// TEMPLATE: LLE 0x10001ac2
// TEMPLATE: LLE 0x10001b02
// TEMPLATE: LLE 0x10001b5f
// SYNTHETIC: LLE 0x10001ae5
// SYNTHETIC: LLE 0x10001b49
// SYNTHETIC: LLE 0x10001b56
