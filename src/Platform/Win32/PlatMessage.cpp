// Platform layer, Win32: messages to the user (include/platform/PlatMessage.h): MessageBoxA without an owner, the
// flags passed through (they are the MB_* values).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

extern "C" uint32_t PlatSystemMachineId(int* outOk)
{
    // CDKey::GetMachineId: the volume of the Windows directory, its serial (0 when GetVolumeInformation fails)
    char thePath[MAX_PATH];
    char theDrive[_MAX_DRIVE];
    char theDirectory[_MAX_DIR];
    char theName[_MAX_FNAME];
    char theExtension[_MAX_EXT];
    if (outOk)
        *outOk = 1;
    GetWindowsDirectoryA(thePath, MAX_PATH);
    _splitpath(thePath, theDrive, theDirectory, theName, theExtension);
    char theRoot[_MAX_DRIVE + 1];
    size_t theLength = strlen(theDrive);
    if (theLength != 0) {
        memcpy(theRoot, theDrive, theLength + 1);
        if (theRoot[theLength - 1] != '\\') {
            theRoot[theLength] = '\\';
            theRoot[theLength + 1] = 0;
        }
        DWORD theSerial = 0;
        GetVolumeInformationA(theRoot, NULL, 0, &theSerial, NULL, NULL, NULL, 0);
        return theSerial;
    }
    if (outOk)
        *outOk = 0;
    return 0;
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
