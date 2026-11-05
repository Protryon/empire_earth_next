// Platform layer, Win32: messages to the user (include/platform/PlatMessage.h): MessageBoxA without an owner, the
// flags passed through (they are the MB_* values).
#include <windows.h>

#include "platform/PlatMessage.h"

static_assert(PLAT_MESSAGE_ERROR == MB_ICONERROR && PLAT_MESSAGE_SYSTEM_MODAL == MB_SYSTEMMODAL
                  && PLAT_MESSAGE_FOREGROUND == MB_SETFOREGROUND,
              "the flags are the MB_* values");

extern "C" void PlatMessageShow(const char* inTitle, const char* inText, uint32_t inFlags)
{
    MessageBoxA(NULL, inText, inTitle, inFlags);
}

extern "C" void PlatMessageSetHandler(PlatMessageHandler inHandler)
{
    (void)inHandler;
}

extern "C" void PlatDebugOutput(const char* inText)
{
    OutputDebugStringA(inText);
}
