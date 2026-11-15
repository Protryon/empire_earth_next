// C:\EEX\Low-Level Engine\Error System\Error System.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// First unit in link order: it also emits the inline members of the headers it is the first to include
// (UWideString/UString 0x10001000.., FSFile/FSBasicFile/FSFileSpec 0x10001279..0x1000144b).
// Complete: the debug-output filter (0x100016fb), the shutdown hook (0x100019cc), the globals' init chains
// (0x1000144b..0x100014f4) and the 0x10001a96.. COMDATs included.
#include <windows.h>
#include "platform/PlatMessage.h"
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
#include "rust/ee_error.h"
#include <string>

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


// The Error System's state and logic are Rust's (rust/crates/ee_error, LLE lane round 2): the Filter.txt filters,
// the verification report and the last native error. This unit keeps the exported names, ESDebugOut's formatting
// and the platform's debug output, and reads Filter.txt through the File System (text mode, as fgets read it).
// The error log the original opened at start-up ("<n>_Error.log", ESBeginRecording/ESEndRecording) went: nothing
// ever wrote it, so it only created an empty file.

// Reads Filter.txt from the startup directory into the Rust filters, once there is a startup path
static void ESLoadFilters()
{
    if (ee_error_filters_loaded() || gStartupVolume.GetLength() == 0 || gStartupDirectory.GetLength() == 0)
        return;
    FSFileSpec theSpec;
    theSpec.SetVolume(gStartupVolume);
    theSpec.SetDirectory(gStartupDirectory);
    theSpec.SetFile(UString("Filter.txt"));
    std::string theText;
    bool theFound = theSpec.FOpen("r") == kErrorOK;
    if (theFound) {
        char theBuffer[4096];
        FILE* theFile = theSpec;
        for (size_t n; (n = fread(theBuffer, 1, sizeof(theBuffer), theFile)) > 0;)
            theText.append(theBuffer, n);
    }
    ee_error_filters_load(theFound, (const uint8_t*)theText.data(), theText.size());
}

// FUNCTION: LLE 0x100015cf
// Formats a debug message and sends it to the debugger when the Filter.txt rules let it through ('!' always passes
// and is not shown)
void ESDebugOut(const char* inFormat, ...)
{
    char theText[1024];
    va_list theArguments;
    va_start(theArguments, inFormat);
    // Fixed: vsprintf overran the buffer past 1023 characters; the message is cut there
    vsnprintf(theText, sizeof(theText), inFormat, theArguments);
    va_end(theArguments);
    ESLoadFilters();
    if (ee_error_filter_passes(theText))
        PlatDebugOutput(theText[0] == '!' ? theText + 1 : theText);
}

// FUNCTION: LLE 0x10001673
// Returns the last nonzero native (Win32) error recorded
unsigned long ESGetNativeError()
{
    return ee_error_native();
}

// FUNCTION: LLE 0x10001679
// Initialises the error system: registers the shutdown hook (ee_error_shutdown_hook, 0x100019cc: frees the filters)
void ESInitialize(HINSTANCE inInstance) { ee_error_initialize(); }

// FUNCTION: LLE 0x100016ed
// Records a nonzero native error and returns it unchanged
unsigned long ESRecordNativeError(unsigned long inError)
{
    return ee_error_record_native(inError);
}

// FUNCTION: LLE 0x10001a4a
// Reports a failed verification (file, line, expression) on stderr, breaks into the debugger and aborts; a nested
// failure is ignored
void ESVerify(const char* inFile, unsigned long inLine, const char* inExpression)
{
    ee_error_verify(inFile, inLine, inExpression);
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
