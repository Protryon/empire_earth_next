// Platform layer, Win32: messages to the user (include/platform/PlatMessage.h): MessageBoxA without an owner, the
// flags passed through (they are the MB_* values).
#include <windows.h>
#include <stdio.h>

#include "platform/PlatMessage.h"
#include "platform/PlatSim.h"

static_assert(PLAT_MESSAGE_ERROR == MB_ICONERROR && PLAT_MESSAGE_SYSTEM_MODAL == MB_SYSTEMMODAL
                  && PLAT_MESSAGE_FOREGROUND == MB_SETFOREGROUND,
              "the flags are the MB_* values");

extern "C" uint16_t PlatSystemLanguageId(void)
{
    return GetUserDefaultLangID();
}

extern "C" uint32_t PlatSystemVersion(void)
{
    return GetVersion();
}

extern "C" void PlatSystemShutdown(void)
{
    CoUninitialize();
}

extern "C" void PlatMessageShow(const char* inTitle, const char* inText, uint32_t inFlags)
{
    if (gPlatSimMode) {   // the headless simulation: no modal box (it would wait for ever)
        fprintf(stderr, "%s%s: %s\n", (inFlags & PLAT_MESSAGE_ERROR) ? "error: " : "", inTitle ? inTitle : "",
                inText ? inText : "");
        fflush(stderr);
        return;
    }
    MessageBoxA(NULL, inText, inTitle, inFlags);
}

extern "C" void PlatMessageSetHandler(PlatMessageHandler inHandler)
{
    (void)inHandler;
}

extern "C" void PlatDebugOutput(const char* inText)
{
    if (PlatDebugHandler theHandler = PlatDebugGetHandler()) {
        theHandler(inText);
        return;
    }
    OutputDebugStringA(inText);
}

extern "C" int PlatSystemSingleInstance(const char* inName)
{
    HANDLE theMutex = CreateMutexA(NULL, FALSE, inName);   // held for the process's lifetime
    return theMutex != NULL && GetLastError() == 0;
}
