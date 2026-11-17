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
#include "rust/base/error.h"
#include <string>

// The Error System's state and logic are Rust's (rust/crates/ee_base/src/error, LLE lane round 2): the Filter.txt filters,
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

// Returns the last nonzero native (Win32) error recorded
unsigned long ESGetNativeError()
{
    return ee_error_native();
}

// Initialises the error system: registers the shutdown hook (ee_error_shutdown_hook, 0x100019cc: frees the filters)
void ESInitialize(HINSTANCE inInstance) { ee_error_initialize(); }

// Records a nonzero native error and returns it unchanged
unsigned long ESRecordNativeError(unsigned long inError)
{
    return ee_error_record_native(inError);
}

// Reports a failed verification (file, line, expression) on stderr, breaks into the debugger and aborts; a nested
// failure is ignored
void ESVerify(const char* inFile, unsigned long inLine, const char* inExpression)
{
    ee_error_verify(inFile, inLine, inExpression);
}
