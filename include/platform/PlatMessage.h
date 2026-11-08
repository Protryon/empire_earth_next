// Platform layer: messages to the user (see platform/Platform.h). A modal message box with one OK button, for
// fatal errors before or outside the game's own interface (Win32: MessageBoxA with no owner window).
#ifndef PLATFORM_PLATMESSAGE_H
#define PLATFORM_PLATMESSAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Flags (the Win32 MB_* values they stand for)
#define PLAT_MESSAGE_ERROR 0x10u            // MB_ICONERROR (= MB_ICONHAND)
#define PLAT_MESSAGE_SYSTEM_MODAL 0x1000u   // MB_SYSTEMMODAL: above every window
#define PLAT_MESSAGE_FOREGROUND 0x10000u    // MB_SETFOREGROUND

// Shows inText under inTitle and waits until the user dismisses it.
void PlatMessageShow(const char* inTitle, const char* inText, uint32_t inFlags);

// Where the messages go (POSIX): a handler installed by the window layer (e.g. SDL_ShowSimpleMessageBox); without
// one, stderr. Win32 always shows MessageBoxA and ignores the handler. NULL restores the default.
typedef void (*PlatMessageHandler)(const char* inTitle, const char* inText, uint32_t inFlags);
void PlatMessageSetHandler(PlatMessageHandler inHandler);

// A line for a developer: the debugger's output (Win32: OutputDebugStringA), stderr elsewhere. inText as given (the
// caller ends lines).
void PlatDebugOutput(const char* inText);

// The system, as the engine asks about it (Utilities.cpp):
// - the user's language as a Windows LANGID (Win32: GetUserDefaultLangID; POSIX: from LC_ALL/LC_MESSAGES/LANG, e.g.
//   "ko_KR.UTF-8" -> 0x412, 0x409 when unset or unknown);
// - the system version in GetVersion's packing (low byte major, next byte minor, bit 31 set for the Windows 9x
//   family; POSIX: 0x0a00000a, an NT 10.0 system);
// - the end of the process's use of system services before exit (Win32: CoUninitialize; POSIX: nothing).
uint16_t PlatSystemLanguageId(void);
// One running copy per user: takes the named lock for the process's lifetime -> nonzero when this is the only copy
// (Win32: a named mutex that did not exist; POSIX: an exclusive flock on a file in the temporary directory)
int PlatSystemSingleInstance(const char* inName);
uint32_t PlatSystemVersion(void);
// A 32-bit number that identifies this machine and stays the same across runs (the WON library keys its CD key
// store with it). Win32: the serial number of the volume holding the Windows directory (CDKey::GetMachineId's
// GetWindowsDirectory + GetVolumeInformation); Linux: FNV-1a of /etc/machine-id; macOS: FNV-1a of gethostuuid().
// *outOk (may be NULL) -> nonzero when the id was found; the result is 0 when it was not.
uint32_t PlatSystemMachineId(int* outOk);
void PlatSystemShutdown(void);

#ifdef __cplusplus
}
#endif

#endif
