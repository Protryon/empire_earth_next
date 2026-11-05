// Platform layer, POSIX: messages to the user (include/platform/PlatMessage.h): the installed handler (the window
// layer's message box), else stderr.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <initializer_list>

#include "platform/PlatMessage.h"

namespace {
std::atomic<PlatMessageHandler> sHandler(nullptr);
}

extern "C" uint16_t PlatSystemLanguageId(void)
{
    const char* theLocale = NULL;
    for (const char* theName : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        theLocale = getenv(theName);
        if (theLocale && *theLocale)
            break;
    }
    if (!theLocale || strlen(theLocale) < 2)
        return 0x409;
    static const struct { const char* mPrefix; uint16_t mId; } kLanguages[] = {
        {"zh_TW", 0x404}, {"zh_HK", 0xc04}, {"zh", 0x804}, {"de", 0x407}, {"en_GB", 0x809}, {"en", 0x409},
        {"es", 0xc0a}, {"fr", 0x40c}, {"it", 0x410}, {"ja", 0x411}, {"ko", 0x412}, {"nl", 0x413}, {"pl", 0x415},
        {"pt_BR", 0x416}, {"pt", 0x816}, {"ru", 0x419}, {"sv", 0x41d}, {"cs", 0x405}, {"hu", 0x40e}};
    for (const auto& theLanguage : kLanguages)
        if (strncmp(theLocale, theLanguage.mPrefix, strlen(theLanguage.mPrefix)) == 0)
            return theLanguage.mId;
    return 0x409;
}

extern "C" uint32_t PlatSystemVersion(void)
{
    return 0x0a00000au;
}

extern "C" void PlatSystemShutdown(void)
{
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
