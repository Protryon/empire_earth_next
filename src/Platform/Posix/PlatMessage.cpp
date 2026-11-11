// Platform layer, POSIX: messages to the user (include/platform/PlatMessage.h): the installed handler (the window
// layer's message box), else stderr.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <uuid/uuid.h>
#endif

#include <atomic>
#include <initializer_list>

#include "platform/PlatMessage.h"
#include "platform/PlatSim.h"

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

namespace {
uint32_t Fnv1a(const unsigned char* inBytes, size_t inLength)
{
    uint32_t theHash = 2166136261u;
    for (size_t i = 0; i < inLength; i++) {
        theHash ^= inBytes[i];
        theHash *= 16777619u;
    }
    return theHash;
}
}   // namespace

extern "C" uint32_t PlatSystemMachineId(int* outOk)
{
    if (outOk)
        *outOk = 0;
#if defined(__APPLE__)
    uuid_t theUUID;
    struct timespec theWait = {1, 0};
    if (gethostuuid(theUUID, &theWait) != 0)
        return 0;
    if (outOk)
        *outOk = 1;
    return Fnv1a(theUUID, sizeof(theUUID));
#else
    // systemd's id, else D-Bus's older copy of it
    for (const char* thePath : {"/etc/machine-id", "/var/lib/dbus/machine-id"}) {
        FILE* theFile = fopen(thePath, "rb");
        if (!theFile)
            continue;
        unsigned char theText[128];
        size_t theLength = fread(theText, 1, sizeof(theText), theFile);
        fclose(theFile);
        while (theLength > 0 && (theText[theLength - 1] == '\n' || theText[theLength - 1] == '\r'
                                 || theText[theLength - 1] == ' '))
            theLength--;
        if (theLength == 0)
            continue;
        if (outOk)
            *outOk = 1;
        return Fnv1a(theText, theLength);
    }
    return 0;
#endif
}

extern "C" void PlatMessageShow(const char* inTitle, const char* inText, uint32_t inFlags)
{
    // the headless simulation: stderr (as on Windows), never a box (it has no display, and a box would wait for ever)
    PlatMessageHandler theHandler = gPlatSimMode ? NULL : sHandler.load();
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
    if (PlatDebugHandler theHandler = PlatDebugGetHandler()) {
        theHandler(inText);
        return;
    }
    fputs(inText ? inText : "", stderr);
}

extern "C" int PlatSystemSingleInstance(const char* inName)
{
    // an exclusive lock on <tmp>/<name>-<uid>.lock, kept open for the process's lifetime
    const char* theDirectory = getenv("TMPDIR");
    if (!theDirectory || !*theDirectory)
        theDirectory = "/tmp";
    char thePath[1024];
    snprintf(thePath, sizeof(thePath), "%s/%s-%u.lock", theDirectory, inName, static_cast<unsigned>(getuid()));
    int theFile = open(thePath, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (theFile < 0)
        return 1;   // no lock possible: let the game run
    if (flock(theFile, LOCK_EX | LOCK_NB) != 0) {
        close(theFile);
        return 0;
    }
    return 1;
}
