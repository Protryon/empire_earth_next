// C:\EEX\Low-Level Engine\Sound Engine\Sound Engine.cpp (file name by analogy with Error System.cpp / Thread System.cpp)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#ifdef _WIN32
#include <mmsystem.h>
#endif

#include "lle/LowLevelEngine.h"
#include "GEWindow.h"
#include "lle/SECD.h"
#include "lle/SEMixer.h"
#include "rust/ee_rust.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

static float gCDVolume;                     // 0x1012decc: the first drive's volume at startup, restored at shutdown

// FUNCTION: LLE 0x100532d9
// Initialises the sound engine: registers its shutdown, sets up the CD drives if wanted and loads the mixer plug-ins
long SEInitialize(const FSFileSpec& inDirectory, GEWindow* inWindow, bool inUseCD)
{
    URegisterShutdown(SEShutdown);
    if (inUseCD)
        SEInitializeCD(inWindow);
    return SEInitializeMixers(inDirectory, inWindow);
}

// FUNCTION: LLE 0x10053305
// Finds a CD volume control (an aux CD device, else a mixer CD/digital source line), opens every CD audio drive
// and remembers the first drive's volume
// NEAR: structure and frame match; register choice differs throughout (the original keeps auxGetNumDevs in esi,
// the constant 1 in edi and the device index in ebx)
void SEInitializeCD(GEWindow* inWindow)
{
#ifdef _WIN32
    SECD* theCD;
    SECDVolumeControl theVolume;
    MCI_OPEN_PARMS theOpen;
    MIXERCAPS theMixerCaps;
    MIXERLINECONTROLS theControls;
    AUXCAPS theAuxCaps;
    MIXERLINE theSourceLine;
    MIXERLINE theDestinationLine;
    MIXERCONTROL theControl;
    unsigned long i;
    unsigned long theMixer;
    unsigned long theDestination;
    unsigned long theSource;
    SECD::SetNotificationRecipient(inWindow);
    theVolume.mType = kSECDNoVolumeControl;
    for (i = 0; i < auxGetNumDevs(); i++) {
        if (auxGetDevCaps(i, &theAuxCaps, sizeof(theAuxCaps)) != MMSYSERR_NOERROR ||
            theAuxCaps.wTechnology != AUXCAPS_CDAUDIO)
            continue;
        theVolume.mType = kSECDAuxVolumeControl;
        theVolume.mAuxDevice = i;
        break;
    }
    if (theVolume.mType == kSECDNoVolumeControl) {
        for (theMixer = 0; theMixer < mixerGetNumDevs(); theMixer++) {
            if (mixerGetDevCaps(theMixer, &theMixerCaps, sizeof(theMixerCaps)) == MMSYSERR_NOERROR) {
                for (theDestination = 0; theDestination < theMixerCaps.cDestinations; theDestination++) {
                    theDestinationLine.cbStruct = sizeof(theDestinationLine);
                    theDestinationLine.dwDestination = theDestination;
                    if (mixerGetLineInfo((HMIXEROBJ)(UINT_PTR)theMixer, &theDestinationLine, MIXER_GETLINEINFOF_DESTINATION) ==
                        MMSYSERR_NOERROR) {
                        for (theSource = 0; theSource < theDestinationLine.cConnections; theSource++) {
                            theSourceLine.cbStruct = sizeof(theSourceLine);
                            theSourceLine.dwDestination = theDestination;
                            theSourceLine.dwSource = theSource;
                            if (mixerGetLineInfo((HMIXEROBJ)(UINT_PTR)theMixer, &theSourceLine, MIXER_GETLINEINFOF_SOURCE) !=
                                    MMSYSERR_NOERROR ||
                                theSourceLine.fdwLine != (MIXERLINE_LINEF_ACTIVE | MIXERLINE_LINEF_SOURCE) ||
                                (theSourceLine.dwComponentType != MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC &&
                                 theSourceLine.dwComponentType != MIXERLINE_COMPONENTTYPE_SRC_DIGITAL))
                                continue;
                            theControls.cbStruct = sizeof(theControls);
                            theControls.dwLineID = theSourceLine.dwLineID;
                            theControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
                            theControls.cControls = 1;
                            theControls.cbmxctrl = sizeof(theControl);
                            theControls.pamxctrl = &theControl;
                            if (mixerGetLineControls((HMIXEROBJ)(UINT_PTR)theMixer, &theControls,
                                                     MIXER_GETLINECONTROLSF_ONEBYTYPE) != MMSYSERR_NOERROR)
                                continue;
                            theVolume.mType = kSECDMixerVolumeControl;
                            theVolume.mDetails.cbStruct = sizeof(theVolume.mDetails);
                            theVolume.mDetails.dwControlID = theControl.dwControlID;
                            theVolume.mDetails.cChannels = 1;
                            theVolume.mDetails.cMultipleItems = 0;
                            theVolume.mDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
                            theVolume.mMinimum = theControl.Bounds.dwMinimum;
                            theVolume.mMaximum = theControl.Bounds.dwMaximum;
                            theVolume.mMixer = (HMIXER)(UINT_PTR)theMixer;
                            break;
                        }
                    }
                    if (theVolume.mType != kSECDNoVolumeControl)
                        break;
                }
            }
            if (theVolume.mType != kSECDNoVolumeControl)
                break;
        }
    }
    for (unsigned short theIndex = 0;; theIndex++) {
        theOpen.lpstrDeviceType = (LPCSTR)(DWORD_PTR)MAKELONG(MCI_DEVTYPE_CD_AUDIO, theIndex);
        if (mciSendCommand(theIndex, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID | MCI_WAIT, (DWORD_PTR)&theOpen) != 0)
            break;
        theCD = new SECD(theIndex, theOpen.wDeviceID, theVolume);
    }
    if (SECD::GetCount() > 0) {
        SECD::GetCD(0, theCD);
        gCDVolume = theCD->GetVolume();
    }
#endif   // no CD audio (MCI) off Windows
}

// FUNCTION: LLE 0x1005354b
// Creates a mixer for every valid sound plug-in of the plug-in table; kErrorNotFound when none loads (the choice:
// rust/crates/ee_sound mixers.rs)
long SEInitializeMixers(const FSFileSpec& inDirectory, GEWindow* inWindow)
{
    SEMixerStart theStart = {&inDirectory, inWindow, NULL};
    return ee_mixers_initialize(&theStart);
}

// FUNCTION: LLE 0x100535e2
// Restores the first CD drive's start-up volume, then deletes every CD drive and every mixer (the first one until
// none is left: ee_sound mixers.rs). There is no CD drive on this port: SEInitialize is never asked for one.
void SEShutdown(bool inForced)
{
    SECD* theCD = NULL;
    if (SECD::GetCount() > 0) {
        SECD::GetCD(0, theCD);
        theCD->SetVolume(gCDVolume);
    }
    while (SECD::GetCount() > 0) {
        SECD::GetCD(0, theCD);
        delete theCD;
    }
    ee_mixers_delete_all();
}
