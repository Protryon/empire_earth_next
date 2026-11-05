// C:\EEX\Low-Level Engine\Utilities\Utilities.cpp (file name inferred: no __FILE__; by analogy with Error System.cpp:
// sorts between UString.cpp and UWideString.cpp)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <stdlib.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "platform/PlatMessage.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x100837ad
// SYNTHETIC: LLE 0x100837b7
// SYNTHETIC: LLE 0x100837cf
// SYNTHETIC: LLE 0x100837db
// TEMPLATE: LLE 0x10083897
// TEMPLATE: LLE 0x100838c3

// Globals in definition order (provisional names)
std::vector<void (*)(bool)> gShutdownFunctions;     // 0x12ed08
bool gShutdownInProgress;                           // 0x12ed18

// FUNCTION: LLE 0x100837e5
// Returns the user's default language id
unsigned short UGetOSLanguage()
{
    return PlatSystemLanguageId();
}

// FUNCTION: LLE 0x100837eb
// Returns the Windows family: 1 = 95, 2 = 98/ME, 3 = NT 5 or later, 4 = NT 4 or older (or a 9x major below 4)
// Written with a result variable (the 4 shares the compare in eax); the major version declared first, both as
// MSDN's (DWORD)(LOBYTE(LOWORD(v))) casts (the up-front movzx of the minor)
unsigned long UGetOSType()
{
    unsigned long theVersion = PlatSystemVersion();
    unsigned long theMajor = (unsigned long)(LOBYTE(LOWORD(theVersion)));
    unsigned long theMinor = (unsigned long)(HIBYTE(LOWORD(theVersion)));
    unsigned long theResult;
    if (theVersion < 0x80000000) {
        if (theMajor < 5)
            theResult = 4;
        else
            theResult = 3;
    } else if (theMajor < 4) {
        theResult = 4;
    } else if (theMajor > 4 || (theMajor == 4 && theMinor >= 10)) {
        theResult = 2;
    } else {
        theResult = 1;
    }
    return theResult;
}

// FUNCTION: LLE 0x10083826
// Registers a function to call at engine shutdown; always returns kErrorOK
long URegisterShutdown(void (*inFunction)(bool))
{
    gShutdownFunctions.push_back(inFunction);
    return kErrorOK;
}

// FUNCTION: LLE 0x10083838
// Calls every registered shutdown function, last registered first; with inTerminate, uninitialises COM
// and exits (inForced: exit(0), else abort)
void UShutdown(bool inForced, bool inTerminate)
{
    gShutdownInProgress = true;
    unsigned long theCount;
    while ((theCount = gShutdownFunctions.size()) != 0) {
        void (*theFunction)(bool) = gShutdownFunctions[theCount - 1];
        gShutdownFunctions.pop_back();
        theFunction(inForced);
    }
    if (!inTerminate)
        return;
    PlatSystemShutdown();
    if (inForced)
        exit(0);
    else
        abort();
}

// FUNCTION: LLE 0x10083891
// Returns whether UShutdown has started
bool UShutdownInProgress()
{
    return gShutdownInProgress;
}
