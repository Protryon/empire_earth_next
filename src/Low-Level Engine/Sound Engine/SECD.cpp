// C:\EEX\Low-Level Engine\Sound Engine\SECD.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#ifdef _WIN32
#include <mmsystem.h>
#endif
#include <string.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/SECD.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

std::vector<SECD*> SECD::sCDs;
HWND SECD::sWindow;

// FUNCTION: LLE 0x1004b546
// Creates a CD audio drive for an MCI device and volume control, registers it and stops it
SECD::SECD(unsigned long inIndex, unsigned int inDevice, const SECDVolumeControl& inVolume)
{
    this->mIndex = inIndex;
    this->mDevice = inDevice;
    memcpy(&this->mVolume, &inVolume, sizeof(this->mVolume));
    this->mEnd = 0;
    this->mVolume.mDetails.paDetails = &this->mVolume.mValue;
    this->mBusy = false;
    SECD::sCDs.push_back(this);
    this->Stop();
}

// FUNCTION: LLE 0x1004b593
// Stops the CD drive and removes it from the drive list
SECD::~SECD()
{
    this->Stop();
    std::vector<SECD*>::iterator theCD = std::find(SECD::sCDs.begin(), SECD::sCDs.end(), this);
    if (theCD != SECD::sCDs.end())
        SECD::sCDs.erase(theCD);
}

// FUNCTION: LLE 0x1004b5fe
// Returns the CD drive with the given index; kErrorBeyondEnd when there is none
long SECD::GetCD(unsigned long inIndex, SECD*& outCD)
{
    if (inIndex < SECD::sCDs.size()) {
        outCD = SECD::sCDs[inIndex];
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

#ifdef _WIN32
// FUNCTION: LLE 0x1004b62a
// Returns the number of tracks on the CD (stops playback first); 0 while busy or on an MCI error
unsigned long SECD::GetTrackCount()
{
    unsigned long theCount = 0;
    if (!this->mBusy) {
        MCI_SEEK_PARMS theSeek;
        MCI_SET_PARMS theSet;
        MCI_STATUS_PARMS theStatus;
        MCIERROR theError;
        this->Stop();
        if ((theError = mciSendCommand(this->mDevice, MCI_SEEK, MCI_SEEK_TO_START, (DWORD_PTR)&theSeek)) == 0 &&
            (theSet.dwTimeFormat = MCI_FORMAT_TMSF,
             theError = mciSendCommand(this->mDevice, MCI_SET, MCI_SET_TIME_FORMAT | MCI_WAIT, (DWORD_PTR)&theSet)) == 0 &&
            (theStatus.dwItem = MCI_STATUS_NUMBER_OF_TRACKS,
             theError = mciSendCommand(this->mDevice, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR)&theStatus)) == 0)
            theCount = theStatus.dwReturn;
        else
            ESRecordNativeError(theError);
    }
    return theCount;
}

// FUNCTION: LLE 0x1004b6ad
// Returns the CD volume (0..1) from the mixer or aux control; 0 without a control or on an error
float SECD::GetVolume()
{
    float theVolume = 0.0f;
    DWORD theValue;
    switch (this->mVolume.mType) {
    case kSECDMixerVolumeControl:
        if (mixerGetControlDetails((HMIXEROBJ)this->mVolume.mMixer, &this->mVolume.mDetails, MIXER_GETCONTROLDETAILSF_VALUE) == 0)
            theVolume = (float)(this->mVolume.mValue.dwValue - this->mVolume.mMinimum) / (float)(this->mVolume.mMaximum - this->mVolume.mMinimum);
        else
            theVolume = 0.0f;
        break;
    case kSECDAuxVolumeControl:
        if (auxGetVolume(this->mVolume.mAuxDevice, &theValue) == 0)
            theVolume = (theValue & 0xffff) * (1.0f / 65535.0f);
        else
            theVolume = 0.0f;
        break;
    case kSECDNoVolumeControl:
        theVolume = 0.0f;
        break;
    }
    return theVolume;
}

// FUNCTION: LLE 0x1004b72c
// Handles an MCI notification for a device: on a finished play reopens the drive, then marks it not busy
void SECD::Notify(unsigned int inFlags, long inDevice)
{
    for (unsigned long i = 0; i < SECD::sCDs.size(); i++) {
        if (SECD::sCDs[i]->mDevice != (MCIDEVICEID)inDevice)
            continue;
        SECD* theCD = SECD::sCDs[i];
        if (inFlags == MCI_NOTIFY_SUCCESSFUL) {
            MCI_GENERIC_PARMS theClose;
            if (mciSendCommand(theCD->mDevice, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)&theClose) == 0) {
                MCI_OPEN_PARMS theOpen;
                theOpen.lpstrDeviceType = (LPCSTR)(DWORD_PTR)MAKELONG(MCI_DEVTYPE_CD_AUDIO, theCD->mIndex);
                if (mciSendCommand(theCD->mIndex, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID | MCI_WAIT,
                                   (DWORD_PTR)&theOpen) == 0)
                    theCD->mDevice = theOpen.wDeviceID;
            }
        }
        theCD->mBusy = false;
        return;
    }
}

// FUNCTION: LLE 0x1004b7b3
// Pauses CD playback and marks the drive not busy
long SECD::Pause()
{
    MCI_GENERIC_PARMS theParms;
    MCIERROR theError = mciSendCommand(this->mDevice, MCI_PAUSE, MCI_WAIT, (DWORD_PTR)&theParms);
    if (theError == 0) {
        this->mBusy = false;
        return kErrorOK;
    }
    ESRecordNativeError(theError);
    return kErrorNativeError;
}

// FUNCTION: LLE 0x1004b7e2
// Plays the CD up to the stored end position with completion notification; kErrorBusy while playing
long SECD::Play()
{
    if (!this->mBusy) {
        MCI_PLAY_PARMS theParms;
        theParms.dwCallback = MAKELONG(SECD::sWindow, 0);
        theParms.dwTo = this->mEnd;
        MCIERROR theError = mciSendCommand(this->mDevice, MCI_PLAY, MCI_NOTIFY | MCI_TO, (DWORD_PTR)&theParms);
        if (theError == 0) {
            this->mBusy = true;
            return kErrorOK;
        }
        ESRecordNativeError(theError);
        return kErrorNativeError;
    }
    return kErrorBusy;
}

// FUNCTION: LLE 0x1004b830
// Seeks to a minute/second position on the CD and sets the end to the disc length; kErrorFailed past the end
long SECD::SeekToTime(const SECDTime& inTime)
{
    if (!this->mBusy) {
        MCI_SEEK_PARMS theSeek;
        MCI_SET_PARMS theSet;
        MCI_STATUS_PARMS theStatus;
        MCIERROR theError;
        this->Stop();
        mciSendCommand(this->mDevice, MCI_SEEK, MCI_SEEK_TO_START, (DWORD_PTR)&theSeek);
        theSet.dwTimeFormat = MCI_FORMAT_MSF;
        theError = mciSendCommand(this->mDevice, MCI_SET, MCI_SET_TIME_FORMAT | MCI_WAIT, (DWORD_PTR)&theSet);
        if (theError == 0) {
            theStatus.dwItem = MCI_STATUS_LENGTH;
            theError = mciSendCommand(this->mDevice, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR)&theStatus);
            if (theError == 0) {
                this->mEnd = theStatus.dwReturn;
                if (!(inTime.mMinutes < MCI_MSF_MINUTE(this->mEnd) ||
                      (inTime.mMinutes == MCI_MSF_MINUTE(this->mEnd) && inTime.mSeconds < MCI_MSF_SECOND(this->mEnd))))
                    return kErrorFailed;
                theSeek.dwTo = MCI_MAKE_MSF(inTime.mMinutes, inTime.mSeconds, 0);
                theError = mciSendCommand(this->mDevice, MCI_SEEK, MCI_WAIT | MCI_TO, (DWORD_PTR)&theSeek);
                if (theError == 0)
                    return kErrorOK;
            }
        }
        ESRecordNativeError(theError);
        return kErrorNativeError;
    }
    return kErrorBusy;
}

// FUNCTION: LLE 0x1004b8f9
// Seeks to the start of a track and sets the end to the end of that track; kErrorFailed for a bad track number
long SECD::SeekToTrack(unsigned long inTrack)
{
    if (!this->mBusy) {
        MCI_SEEK_PARMS theSeek;
        MCI_SET_PARMS theSet;
        MCI_STATUS_PARMS theStatus;
        MCIERROR theError;
        this->Stop();
        theError = mciSendCommand(this->mDevice, MCI_SEEK, MCI_SEEK_TO_START, (DWORD_PTR)&theSeek);
        if (theError == 0) {
            theSet.dwTimeFormat = MCI_FORMAT_TMSF;
            theError = mciSendCommand(this->mDevice, MCI_SET, MCI_SET_TIME_FORMAT | MCI_WAIT, (DWORD_PTR)&theSet);
            if (theError == 0) {
                theStatus.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
                theError = mciSendCommand(this->mDevice, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR)&theStatus);
                if (theError == 0) {
                    if (inTrack < 1 || inTrack > theStatus.dwReturn)
                        return kErrorFailed;
                    theStatus.dwTrack = inTrack;
                    theStatus.dwItem = MCI_STATUS_LENGTH;
                    theError = mciSendCommand(this->mDevice, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT,
                                              (DWORD_PTR)&theStatus);
                    if (theError == 0) {
                        theSeek.dwTo = MCI_MAKE_TMSF(inTrack, 0, 0, 0);
                        this->mEnd = MCI_MAKE_TMSF(inTrack, MCI_MSF_MINUTE(theStatus.dwReturn),
                                             MCI_MSF_SECOND(theStatus.dwReturn), MCI_MSF_FRAME(theStatus.dwReturn));
                        theError = mciSendCommand(this->mDevice, MCI_SEEK, MCI_WAIT | MCI_TO, (DWORD_PTR)&theSeek);
                        if (theError == 0)
                            return kErrorOK;
                    }
                }
            }
        }
        ESRecordNativeError(theError);
        return kErrorNativeError;
    }
    return kErrorBusy;
}

// FUNCTION: LLE 0x1004b9f1
// Sets the CD volume (0..1) on the mixer or aux control; kErrorBeyondEnd when out of range
// NEAR: the original keeps a zero in eax across the switch (xor eax,eax before the dec chain) and shares the
// kErrorBeyondEnd exit; return-in-case, break and theResult variants tried
long SECD::SetVolume(float inVolume)
{
    MMRESULT theError = MMSYSERR_NOERROR;
    switch (this->mVolume.mType) {
    case kSECDNoVolumeControl:
        return kErrorOK;
    case kSECDMixerVolumeControl:
        if (inVolume < 0.0f || inVolume > 1.0f)
            return kErrorBeyondEnd;
        this->mVolume.mValue.dwValue = (DWORD)((float)(this->mVolume.mMaximum - this->mVolume.mMinimum) * inVolume) + this->mVolume.mMinimum;
        theError = mixerSetControlDetails((HMIXEROBJ)this->mVolume.mMixer, &this->mVolume.mDetails, MIXER_SETCONTROLDETAILSF_VALUE);
        break;
    case kSECDAuxVolumeControl: {
        if (inVolume < 0.0f || inVolume > 1.0f)
            return kErrorBeyondEnd;
        WORD theValue = (WORD)(inVolume * 65535.0f);
        theError = auxSetVolume(this->mVolume.mAuxDevice, MAKELONG(theValue, theValue));
        break;
    }
    }
    if (theError != MMSYSERR_NOERROR) {
        ESRecordNativeError(theError);
        return kErrorNativeError;
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x1004bab7
// Stops CD playback, closes and reopens the MCI device, and marks the drive not busy
long SECD::Stop()
{
    MCI_GENERIC_PARMS theStop;
    MCIERROR theError = mciSendCommand(this->mDevice, MCI_STOP, MCI_WAIT, (DWORD_PTR)&theStop);
    if (theError == 0) {
        MCI_GENERIC_PARMS theClose;
        if (mciSendCommand(this->mDevice, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)&theClose) == 0) {
            MCI_OPEN_PARMS theOpen;
            theOpen.lpstrDeviceType = (LPCSTR)(DWORD_PTR)MAKELONG(MCI_DEVTYPE_CD_AUDIO, this->mIndex);
            if (mciSendCommand(this->mIndex, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID | MCI_WAIT, (DWORD_PTR)&theOpen) == 0)
                this->mDevice = theOpen.wDeviceID;
        }
        this->mBusy = false;
        return kErrorOK;
    }
    ESRecordNativeError(theError);
    return kErrorNativeError;
}
#else
// No CD audio off Windows (MCI): SEInitializeCD opens no drive, so none of these is reached
unsigned long SECD::GetTrackCount() { return 0; }
float SECD::GetVolume() { return 0.0f; }
void SECD::Notify(unsigned int inFlags, long inDevice) {}
long SECD::Pause() { return kErrorNotFound; }
long SECD::Play() { return kErrorNotFound; }
long SECD::SeekToTime(const SECDTime& inTime) { return kErrorNotFound; }
long SECD::SeekToTrack(unsigned long inTrack) { return kErrorNotFound; }
long SECD::SetVolume(float inVolume) { return kErrorNotFound; }
long SECD::Stop() { return kErrorNotFound; }
#endif

// Header inline block, init chain and template COMDATs emitted in this unit (markers at the end):
// SYNTHETIC: LLE 0x1004b4b7
// SYNTHETIC: LLE 0x1004b4cf
// SYNTHETIC: LLE 0x1004b4d9
// SYNTHETIC: LLE 0x1004b4dd
// SYNTHETIC: LLE 0x1004b4e9
// SYNTHETIC: LLE 0x1004b4fd
// SYNTHETIC: LLE 0x1004b507
// SYNTHETIC: LLE 0x1004b51f
// SYNTHETIC: LLE 0x1004b52b
// TEMPLATE: LLE 0x1004bb31
// TEMPLATE: LLE 0x1004bb5d
