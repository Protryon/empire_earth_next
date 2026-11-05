// C:\EEX\Low-Level Engine\Sound Engine\SECD.h (RECONSTRUCTION; SECD.cpp 0x4b4b7..0x4bbce). Red Book audio through
// MCI, volume through the mixer or aux API (Windows only: no drives elsewhere). Requires <windows.h>.
#ifndef LLE_SECD_H
#define LLE_SECD_H

#include <vector>
#ifdef _WIN32
#include <mmsystem.h>
#else
// CD audio is Windows' MCI: elsewhere there are no drives (SEInitializeCD opens none) and these stand for the
// Windows types in the class layout only
typedef struct { uint32_t mOpaque[6]; void* paDetails; } MIXERCONTROLDETAILS;
typedef struct { uint32_t dwValue; } MIXERCONTROLDETAILS_UNSIGNED;
typedef void* HMIXER;
typedef uint32_t MCIDEVICEID;
#endif
#include "GEWindow.h"

// How a drive's volume is controlled (SECDVolumeControl::mType). Names are provisional.
enum {
    kSECDNoVolumeControl = 1,
    kSECDAuxVolumeControl = 2,
    kSECDMixerVolumeControl = 3
};

struct SECDVolumeControl {                  // proven sizeof 0x30 (SECD ctor memcpy)
    MIXERCONTROLDETAILS mDetails;           // +0x00 (paDetails -> mValue, set by the SECD ctor)
    MIXERCONTROLDETAILS_UNSIGNED mValue;    // +0x18
    unsigned long mType;                    // +0x1c kSECD*VolumeControl
    unsigned long mMinimum;                 // +0x20 control range
    unsigned long mMaximum;                 // +0x24
    HMIXER mMixer;                          // +0x28
    UINT mAuxDevice;                        // +0x2c
};

struct SECDTime {                           // SeekToTime: minutes/seconds into the disc. Names are provisional.
    unsigned long mMinutes;                 // +0x00
    unsigned long mSeconds;                 // +0x04
};

class FSFileSpec;
void SEInitializeCD(GEWindow* inWindow);   // Sound Engine.cpp; (provisional names)
void SEShutdown(bool inForced);

class LLE_API SECD {                        // proven sizeof 0x40 (implicit operator=: rep movsd x 0x10)
    friend void SEInitializeCD(GEWindow* inWindow);     // creates the drives
    friend void SEShutdown(bool inForced);              // deletes them
public:
    // inline block emitted first in SECD.cpp (0x4b4b7..0x4b4fd): GetCount, GetHasVolumeControl, IsBusy,
    // SetNotificationRecipient, then the implicit operator=
#ifdef LOWLEVELENGINE_EXPORTS
    static unsigned long GetCount() { return SECD::sCDs.size(); }
#else
    static unsigned long GetCount();
#endif
    bool GetHasVolumeControl() { return this->mVolume.mType != kSECDNoVolumeControl; }
    bool IsBusy() const { return this->mBusy; }
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)   // the exe stores __imp_sWindow inline (0x1437d1)
    static void SetNotificationRecipient(GEWindow* inWindow) { SECD::sWindow = (HWND)inWindow->GetNativeHandle(); }
#else
    static void SetNotificationRecipient(GEWindow* inWindow);
#endif
    static long GetCD(unsigned long inIndex, SECD*& outCD);
    static void Notify(unsigned int inFlags, long inDevice);
    unsigned long GetTrackCount();
    float GetVolume();
    long Pause();
    long Play();
    long SeekToTime(const SECDTime& inTime);
    long SeekToTrack(unsigned long inTrack);
    long SetVolume(float inVolume);
    long Stop();
private:
    SECD(unsigned long inIndex, unsigned int inDevice, const SECDVolumeControl& inVolume);
    ~SECD();

    SECDVolumeControl mVolume;              // +0x00
    MCIDEVICEID mDevice;                    // +0x30 open cdaudio device
    unsigned long mEnd;                     // +0x34 play-to position (MSF / TMSF)
    unsigned long mIndex;                   // +0x38 drive index (also passed as the MCI_OPEN device id)
    volatile bool mBusy;                    // +0x3c playing (cleared by Notify; volatile: stored with mov, not and)

    // Inferred engine-only visibility (a static data member consumes a VC6 $E number in every unit that sees the
    // class; the plug-ins' numbering shows they saw none; see TSScheduler.h)
#ifdef LOWLEVELENGINE_EXPORTS
    static std::vector<SECD*> sCDs;         // 0x1012de50
#endif
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)
    static HWND sWindow;                    // 0x1012de60 MCI notification window (exe import)
#endif
};

#endif
