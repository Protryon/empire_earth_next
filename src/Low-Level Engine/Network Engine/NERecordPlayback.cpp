// C:\EEX\Low-Level Engine\Network Engine\NERecordPlayback.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Opens with NERecordPlayback's inline block (0x45535..0x4555d).

#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NETimestampedMessage.h"
#include "lle/NERecordPlayback.h"

// FUNCTION: LLE 0x1004555d
// Creates a message recorder/player in pass-through mode that hands messages to the given callback
NERecordPlayback::NERecordPlayback(void (*inCallbackFunction)(NEMessage*, void*), void* inCallbackData)
{
    ESVERIFY_AT(inCallbackFunction != NULL, 39);
    this->mPlaybackThread = NULL;
    this->mPlaybackThreadQuitEvent = NULL;
    this->mPlaybackThreadExiting = NULL;
    this->mPlaybackThreadSlowModeToggleEvent = NULL;
    this->mPlaybackThreadNormalModeToggleEvent = NULL;
    this->mRecordFile = NULL;
    this->mPlaybackFile = NULL;
    this->mStartTime = 0;
    this->mCallbackData = inCallbackData;
    this->mCallbackFunction = inCallbackFunction;
    this->mMode = kRecordModePassing;
    this->mRecordMode = kRecordTargetFile;
    this->mPlaybackMode = kRecordTargetFile;
}

// FUNCTION: LLE 0x100455b5
// Destroys the recorder, stopping any recording or playback and closing the record file
NERecordPlayback::~NERecordPlayback()
{
    this->SwitchModes(kRecordModePassing);
    if (this->mRecordFile != NULL)
        this->mRecordFile->Close();
}

// FUNCTION: LLE 0x100455cd
// Returns whether the playback thread has played the whole file and exited
bool NERecordPlayback::PlaybackComplete()
{
    ESVERIFY_AT(this->mMode == knerpmPlaying, 86);
    return this->mPlaybackThreadExiting->Wait(0) == kErrorOK;
}

// FUNCTION: LLE 0x100455fb
// Playback thread: replays each recorded message at its recorded time (45x slower in slow mode) via the callback
// until the file ends or quit is signalled; a message pending at quit is never released
unsigned long NERecordPlayback::PlaybackThreadFunction(void* inArgument)
{
    bool theSlow = false;
    unsigned char* thePayload = NULL;
    unsigned long theRecordStartTime = 0;
    unsigned long theMessageSize = 0;
    unsigned long theMessageTime = 0;
    bool theContinue = true;
    unsigned long theRead;
    long theResult;
    long theDelay;
    NEMessage* theMessage;
    NERecordPlayback* thePlayback = static_cast<NERecordPlayback*>(inArgument);
    ESVERIFY_AT(inArgument != NULL, 121);
    FSFileSpec* theFile = thePlayback->mPlaybackFile;
    UMSetFloatingPointPrecision(kFPPrecision53);

    unsigned long theStartTime = TMGetTime();
    switch (thePlayback->mPlaybackMode) {
    case kRecordTargetFile:
        theResult = theFile->Read(theRecordStartTime);
        ESVERIFY_AT(theResult == keseOK, 142);
        break;

    case kRecordTargetNetwork:
        ESVERIFY_AT("Functionality not implemented yet" == NULL, 146);
        break;

    default:
        ESVERIFY_AT("Invalid playback mode" == NULL, 150);
        break;
    }

    do {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        switch (thePlayback->mPlaybackMode) {
        case kRecordTargetFile:
            if (theFile->Read(theMessageTime) != kErrorOK) {
                thePlayback->mPlaybackThreadExiting->Set();
                return 0;
            }
            ESVERIFY_AT(theFile->Read (theMessageSize) == keseOK, 180);
            thePayload = new unsigned char[theMessageSize];
            ESVERIFY_AT(thePayload != NULL, 182);

            theResult = theFile->Read(thePayload, theMessageSize, theRead);
            ESVERIFY_AT(theResult == keseOK, 185);
            break;

        case kRecordTargetNetwork:
            ESVERIFY_AT("Functionality not implemented yet" == NULL, 189);
            break;

        default:
            ESVERIFY_AT("Invalid playback mode" == NULL, 193);
            break;
        }
        theMessage = NEMessage::ConvertMemoryToMessage(thePayload);
        delete thePayload;
        thePayload = NULL;
        theDelay = theMessageTime - theRecordStartTime + theStartTime - TMGetTime();
        if (theDelay < 0)
            theDelay = 0;
        if (theSlow)
            theDelay *= 45;
        while (theDelay > 0) {
            if (thePlayback->mPlaybackThreadQuitEvent->Wait(10) == kErrorOK) {
                theContinue = false;
                break;
            }
            theDelay -= 20;
            if (thePlayback->mPlaybackThreadSlowModeToggleEvent->Wait(10) == kErrorOK && !theSlow) {
                theSlow = true;
                theDelay *= 45;
            }
            theDelay -= 10;
            if (thePlayback->mPlaybackThreadNormalModeToggleEvent->Wait(10) == kErrorOK && theSlow) {
                theSlow = false;
                theDelay = theDelay / 45 + 1;
            }
        }
        if (theDelay <= 0 && theMessage != NULL)
            (*thePlayback->mCallbackFunction)(theMessage, thePlayback->mCallbackData);
    } while (theContinue);
    thePlayback->mPlaybackThreadExiting->Set();
    return 0;
}

// FUNCTION: LLE 0x10045850
// Passes a message to the callback; in record mode first writes it with a timestamp to the record file
// (in playback mode live messages are dropped without being released)
void NERecordPlayback::Process(NEMessage* inMessage)
{
    unsigned char* theData;
    unsigned long theSize;
    unsigned long theWritten;
    ESVERIFY_AT(this->mCallbackFunction != NULL, 283);
    switch (this->mMode) {
    case kRecordModePassing:
        (*this->mCallbackFunction)(inMessage, this->mCallbackData);
        break;
    case kRecordModeRecording:
        {
            unsigned long theTime = TMGetTime();
            NETimestampedMessage* theTimestampedMessage = new NETimestampedMessage(inMessage, theTime);
            theTimestampedMessage->ExportMessage(&theData, theSize);
            if (theData != NULL) {
                switch (this->mRecordMode) {
                case kRecordTargetFile:
                    this->mRecordFile->Write(theData, theSize, theWritten);
                    break;

                case kRecordTargetNetwork:
                    ESVERIFY_AT("Functionality not implemented yet" == NULL, 308);
                    break;

                default:
                    ESVERIFY_AT("Invalid record mode" == NULL, 312);
                    break;
                }
                delete theData;
            }
            inMessage->Reference();
            (*this->mCallbackFunction)(inMessage, this->mCallbackData);
            delete theTimestampedMessage;
        }
        break;
    }
}

// FUNCTION: LLE 0x1004594d
// Stops recording or playback and passes messages straight through
long NERecordPlayback::PassMode()
{
    this->SwitchModes(kRecordModePassing);
    return kErrorOK;
}

// FUNCTION: LLE 0x10045957
// Starts recording messages to a file (recreated if it exists), writing the start time first
long NERecordPlayback::RecordMode(FSFileSpec* inRecordFile)
{
    this->SwitchModes(kRecordModeRecording);
    if (inRecordFile->Create() == kErrorAlreadyExists) {
        ESVERIFY_AT(inRecordFile->Delete() == keseOK, 368);
        ESVERIFY_AT(inRecordFile->Create() == keseOK, 369);
    }
    ESVERIFY_AT(inRecordFile->Open(kfsaWrite) == keseOK, 371);
    this->mRecordFile = inRecordFile;
    this->mRecordMode = kRecordTargetFile;
    this->mStartTime = TMGetTime();
    return this->mRecordFile->Write(this->mStartTime);
}

// FUNCTION: LLE 0x100459f7
// Starts recording to a network address (not implemented: asserts, then only switches mode)
long NERecordPlayback::RecordMode(UString& inAddress, unsigned long& inPort)
{
    bool theFeatureImplemented = false;
    ESVERIFY_AT(theFeatureImplemented, 396);
    this->SwitchModes(kRecordModeRecording);
    this->mRecordMode = kRecordTargetNetwork;
    this->mStartTime = TMGetTime();
    return kErrorOK;
}

// FUNCTION: LLE 0x10045a2c
// Starts playing back a recorded file on a new "CommRecPlay" thread with its quit/slow/normal events
long NERecordPlayback::PlaybackMode(FSFileSpec* inPlaybackFile)
{
    this->SwitchModes(kRecordModePlaying);
    ESVERIFY_AT(inPlaybackFile->Open(kfsaRead) == keseOK, 428);

    this->mPlaybackFile = inPlaybackFile;
    this->mPlaybackMode = kRecordTargetFile;
    ESVERIFY_AT(TSScheduler::CreateEvent (this->mPlaybackThreadQuitEvent) == keseOK, 435);
    ESVERIFY_AT(TSScheduler::CreateEvent (this->mPlaybackThreadExiting) == keseOK, 436);
    ESVERIFY_AT(TSScheduler::CreateEvent (this->mPlaybackThreadSlowModeToggleEvent) == keseOK, 437);
    ESVERIFY_AT(TSScheduler::CreateEvent (this->mPlaybackThreadNormalModeToggleEvent) == keseOK, 438);
    long theResult = TSScheduler::CreateThread(this->mPlaybackThread, NERecordPlayback::PlaybackThreadFunction, this, 0, false);
    this->mPlaybackThread->SetName(UString("CommRecPlay"));
    return theResult;
}

// FUNCTION: LLE 0x10045b3a
// Starts playback from a network address (not implemented: asserts, then only switches mode)
long NERecordPlayback::PlaybackMode(UString& inAddress, unsigned long& inPort)
{
    bool theFeatureImplemented = false;
    ESVERIFY_AT(theFeatureImplemented, 463);
    this->SwitchModes(kRecordModePlaying);
    this->mPlaybackMode = kRecordTargetNetwork;
    return kErrorOK;
}

// FUNCTION: LLE 0x10045b67
// Switches mode, first stopping the playback thread and closing the playback or record file of the old mode
void NERecordPlayback::SwitchModes(unsigned char inMode)
{
    switch (this->mMode) {
    case kRecordModePlaying:
        if (this->mPlaybackThread != NULL) {
            if (!this->PlaybackComplete()) {
                this->mPlaybackThreadQuitEvent->Set();
                this->mPlaybackThreadExiting->Wait(kTSWaitInfinite);
            }
            TSScheduler::DestroyThread(this->mPlaybackThread);
            TSScheduler::DestroyEvent(this->mPlaybackThreadQuitEvent);
            TSScheduler::DestroyEvent(this->mPlaybackThreadExiting);
            TSScheduler::DestroyEvent(this->mPlaybackThreadSlowModeToggleEvent);
            TSScheduler::DestroyEvent(this->mPlaybackThreadNormalModeToggleEvent);
            this->mPlaybackThread = NULL;
            this->mPlaybackThreadQuitEvent = NULL;
            this->mPlaybackThreadExiting = NULL;
            this->mPlaybackThreadSlowModeToggleEvent = NULL;
            this->mPlaybackThreadNormalModeToggleEvent = NULL;
        }
        if (this->mPlaybackFile != NULL) {
            this->mPlaybackFile->Close();
            this->mPlaybackFile = NULL;
        }
        break;
    case kRecordModeRecording:
        if (this->mRecordFile != NULL) {
            this->mRecordFile->Close();
            this->mRecordFile = NULL;
        }
        break;
    }
    this->mMode = inMode;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10045535
// SYNTHETIC: LLE 0x10045539
// SYNTHETIC: LLE 0x10045541
// SYNTHETIC: LLE 0x10045549
