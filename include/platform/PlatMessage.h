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

#ifdef __cplusplus
}
#endif

#endif
