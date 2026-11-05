// Platform layer, POSIX: messages to the user (include/platform/PlatMessage.h): the installed handler (the window
// layer's message box), else stderr.
#include <stdio.h>

#include <atomic>

#include "platform/PlatMessage.h"

namespace {
std::atomic<PlatMessageHandler> sHandler(nullptr);
}

extern "C" void PlatMessageShow(const char* inTitle, const char* inText, uint32_t inFlags)
{
    PlatMessageHandler theHandler = sHandler.load();
    if (theHandler) {
        theHandler(inTitle ? inTitle : "", inText ? inText : "", inFlags);
        return;
    }
    fprintf(stderr, "%s%s: %s\n", (inFlags & PLAT_MESSAGE_ERROR) ? "error: " : "", inTitle ? inTitle : "",
            inText ? inText : "");
    fflush(stderr);
}

extern "C" void PlatMessageSetHandler(PlatMessageHandler inHandler)
{
    sHandler.store(inHandler);
}

extern "C" void PlatDebugOutput(const char* inText)
{
    fputs(inText ? inText : "", stderr);
}
