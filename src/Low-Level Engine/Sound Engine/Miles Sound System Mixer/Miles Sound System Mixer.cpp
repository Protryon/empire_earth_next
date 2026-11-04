// C:\EEX\Low-Level Engine\Sound Engine\Miles Sound System Mixer\Miles Sound System Mixer.cpp
// Unit flags (hypothesis, docs/CALIBRATION.md): -O1 -G6 -MD -GR
#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include <vector>

#include "mss/mss.h"
#include "lle/LowLevelEngine.h"
#include "lle/U2DGeometry.h"

// ---- Result codes returned by the SPIM entry points (provisional names); values proven by codegen.
enum {
    kSPIMOK = 0,
    kSPIMNativeError = 1,
    kSPIMChannelBusy = 2,           // channels already created / sample still playing / channel empty on stop
    kSPIMNoSample = 3,
    kSPIMChannelPlaying = 5,
    kSPIMChannelIdle = 6,
    kSPIMOutOfMemory = 8,
    kSPIMFailed = 9,
    kSPIMBadParameter = 10,
    kSPIMSampleOnOtherChannel = 13
};

// ---- Unit-local types
struct SPIMChannel {                        // proven sizeof 8 (size() shifts by 3)
    HSAMPLE mHandle;                        // proven +0x00
    SESample* mSample;                      // proven +0x04 (0: channel free)
};

struct SPIMChannelInfo {                    // inferred: a 0xc-byte SPIMActivateMixer local passed by address,
    unsigned long _provisional[3];          // never read by SPIMCreateChannels
};

class SampleData : public UReferenceCounter {   // RTTI .?AVSampleData@@; proven sizeof 0xc (operator new(0xc))
public:
    SampleData(void* inData) { this->mData = inData; }  // body assignment: stored after the vptr (0x1788)
    virtual ~SampleData()
    {
        if (this->mData)
            AIL_mem_free_lock(this->mData);
    }

    void* mData;                            // proven +0x08: AIL_mem_alloc_lock / AIL_file_read memory
};

// Compiler-generated for this unit (names in analysis/mixer/names.txt):
// SYNTHETIC: MIXER 0x10001000
// SYNTHETIC: MIXER 0x1000100a
// SYNTHETIC: MIXER 0x1000101b
// SYNTHETIC: MIXER 0x10001027
// SYNTHETIC: MIXER 0x10001032
// SYNTHETIC: MIXER 0x1000103c
// SYNTHETIC: MIXER 0x1000104d
// SYNTHETIC: MIXER 0x10001059
// SYNTHETIC: MIXER 0x10001064
// SYNTHETIC: MIXER 0x1000106e
// SYNTHETIC: MIXER 0x1000107f
// SYNTHETIC: MIXER 0x1000108b
// SYNTHETIC: MIXER 0x10001096
// SYNTHETIC: MIXER 0x100010a0
// SYNTHETIC: MIXER 0x100010b1
// SYNTHETIC: MIXER 0x100010bd
// SYNTHETIC: MIXER 0x100010c8
// SYNTHETIC: MIXER 0x100010d2
// SYNTHETIC: MIXER 0x100010e3
// SYNTHETIC: MIXER 0x100010ef
// SYNTHETIC: MIXER 0x100010fa
// SYNTHETIC: MIXER 0x10001104
// SYNTHETIC: MIXER 0x10001115
// SYNTHETIC: MIXER 0x10001121
// SYNTHETIC: MIXER 0x1000112c
// SYNTHETIC: MIXER 0x10001136
// SYNTHETIC: MIXER 0x10001147
// SYNTHETIC: MIXER 0x10001153
// SYNTHETIC: MIXER 0x1000115e
// SYNTHETIC: MIXER 0x10001168
// SYNTHETIC: MIXER 0x10001180
// SYNTHETIC: MIXER 0x1000118c
// SYNTHETIC: MIXER 0x10001196
// SYNTHETIC: MIXER 0x100011a0
// SYNTHETIC: MIXER 0x100011b1
// SYNTHETIC: MIXER 0x100011bd
// SYNTHETIC: MIXER 0x100017dd
// TEMPLATE: MIXER 0x10001ba0
// TEMPLATE: MIXER 0x10001bdb
// TEMPLATE: MIXER 0x10001be7
// TEMPLATE: MIXER 0x10001bf3
// TEMPLATE: MIXER 0x10001c9e
// TEMPLATE: MIXER 0x10001ce2
// TEMPLATE: MIXER 0x10001d1b
// TEMPLATE: MIXER 0x10001d40
// TEMPLATE: MIXER 0x10001d4d
// TEMPLATE: MIXER 0x10001d5f
// TEMPLATE: MIXER 0x10001ffc
// SYNTHETIC: MIXER 0x1000206d
// SYNTHETIC: MIXER 0x10002071
// SYNTHETIC: MIXER 0x10002094
// SYNTHETIC: MIXER 0x10002095
// SYNTHETIC: MIXER 0x100020b2
// SYNTHETIC: MIXER 0x100020f9
// SYNTHETIC: MIXER 0x10002106
// SYNTHETIC: MIXER 0x1000210f
// TEMPLATE: MIXER 0x1000213d
// TEMPLATE: MIXER 0x10002151
// TEMPLATE: MIXER 0x10002176
// TEMPLATE: MIXER 0x100021a1
// TEMPLATE: MIXER 0x100021c3
// TEMPLATE: MIXER 0x100021ee
// SYNTHETIC: MIXER 0x10002205
// SYNTHETIC: MIXER 0x1000220f
// SYNTHETIC: MIXER 0x1000222a
// SYNTHETIC: MIXER 0x10002236
// TEMPLATE: MIXER 0x1000225b
// TEMPLATE: MIXER 0x1000227d
// TEMPLATE: MIXER 0x100022e1
// TEMPLATE: MIXER 0x100022fc
// TEMPLATE: MIXER 0x1000232e
// TEMPLATE: MIXER 0x10002363

// ---- Globals in definition order (= dynamic initializer order 0x1000.., after the U2D header keys).
// Addresses in analysis/mixer/names.txt (provisional names).
std::vector<SPIMChannel> gChannels;                 // 0x52a8
UString gMixerName("Miles Sound System Mixer");     // 0x52d0
const float kMaximumVolume = 127.0f;                // Miles volume/pan scale (a variable: loaded before the operand)
float gVolume = 1.0f;                               // 0x5040
unsigned long gMixerType = ' snd';                  // 0x5044
HSAMPLE gScratchSample;                             // 0x52b8 (sample handle used to parse loaded files)
TSCriticalSection* gLoadLock;                       // 0x52bc
unsigned long gFragmentSize;                        // 0x5304 (DIG_DS_FRAGMENT_SIZE)
unsigned long gMixFragmentCount;                    // 0x5308 (DIG_DS_MIX_FRAGMENT_CNT at activation)
unsigned long gFragmentCount;                       // 0x530c (DIG_DS_FRAGMENT_CNT)
HWND* gWindow;                                      // 0x5310 (SPIMInitializeMixer argument)
LPDIRECTSOUND gDirectSound;                         // 0x5314
HDIGDRIVER gDriver;                                 // 0x5318
HWND gWindowHandle;                                 // 0x531c
unsigned long gMaximumChannelCount;                 // 0x5320
bool gActive;                                       // 0x5324

extern "C" long SPIMActivateMixer();
extern "C" long SPIMCreateChannels(unsigned long inCount, SPIMChannelInfo* outInfo);
extern "C" long SPIMDeactivateMixer();
extern "C" long SPIMDestroyChannels();
extern "C" long SPIMIsChannelBusyPlaying(unsigned long inChannel);
extern "C" long SPIMSetLatency(unsigned long inMilliseconds);
extern "C" void SPIMSetSampleLooping(SESample* inSample, bool inLooping);
extern "C" long SPIMSetSamplePan(SESample* inSample, float inPan);
extern "C" long SPIMSetSamplePitch(SESample* inSample, float inPitch);
extern "C" long SPIMSetSampleVolume(SESample* inSample, float inVolume);
extern "C" long SPIMStopChannel(unsigned long inChannel);

// FUNCTION: MIXER 0x100011c8
// DLL entry point; does nothing and reports success
BOOL WINAPI DllMain(HINSTANCE inInstance, DWORD inReason, LPVOID inReserved)
{
    return TRUE;
}

// FUNCTION: MIXER 0x100011ce
// Returns the plug-in's name ("Miles Sound System Mixer")
extern "C" const UString* PIMGetName()
{
    return &gMixerName;
}

// FUNCTION: MIXER 0x100011d4
// Returns the plug-in type (' snd')
extern "C" unsigned long PIMGetType()
{
    return gMixerType;
}

// FUNCTION: MIXER 0x100011da
// Returns whether the mixer works by activating and deactivating it once
extern "C" bool PIMIsValid()
{
    bool theResult;
    if (SPIMActivateMixer() == kSPIMOK) {
        theResult = true;
        SPIMDeactivateMixer();
    } else
        theResult = false;
    return theResult;
}

// FUNCTION: MIXER 0x100011f3
// Starts Miles with a 22 kHz 16-bit stereo DirectSound driver, takes the free hardware buffers as the channel
// limit (16 when 0 or over 64), creates the channels and records the fragment settings
extern "C" long SPIMActivateMixer()
{
    long theResult;
    UString theRedistDirectory = gStartupVolume + gStartupDirectory + "redist\\win32";
    DSCAPS theCaps;
    SPIMChannelInfo theInfo;
    HSAMPLE theSample;
    AIL_set_redist_directory(theRedistDirectory);
    AIL_startup();
    AIL_set_preference(DIG_USE_WAVEOUT, 0);
    AIL_set_preference(DIG_MIXER_CHANNELS, 64);
    gDriver = AIL_open_digital_driver(22050, 16, 2, 1);
    if (gDriver) {
        TSScheduler::CreateCriticalSection(gLoadLock);
        AIL_set_DirectSound_HWND(gDriver, gWindowHandle);
        theSample = AIL_allocate_sample_handle(gDriver);
        AIL_get_DirectSound_info(theSample, (AILLPDIRECTSOUND*)&gDirectSound, NULL);
        gDirectSound->AddRef();
        theCaps.dwSize = sizeof(theCaps);
        gDirectSound->GetCaps(&theCaps);
        gMaximumChannelCount = theCaps.dwFreeHwMixingAllBuffers;
        if (gMaximumChannelCount > 64 || gMaximumChannelCount == 0)
            gMaximumChannelCount = 16;
        AIL_release_sample_handle(theSample);
        gScratchSample = AIL_allocate_sample_handle(gDriver);
        gActive = true;
        SPIMCreateChannels(gMaximumChannelCount, &theInfo);
        gFragmentSize = AIL_get_preference(DIG_DS_FRAGMENT_SIZE);
        gMixFragmentCount = AIL_get_preference(DIG_DS_MIX_FRAGMENT_CNT);
        gFragmentCount = AIL_get_preference(DIG_DS_FRAGMENT_CNT);
        theResult = kSPIMOK;
    } else {
        AIL_last_error();
        theResult = kSPIMNativeError;
    }
    return theResult;
}

// FUNCTION: MIXER 0x1000135e
// Allocates up to the channel limit of Miles sample handles as empty channels; kSPIMChannelBusy if channels exist
extern "C" long SPIMCreateChannels(unsigned long inCount, SPIMChannelInfo* outInfo)
{
    if (gChannels.size() == 0) {
        if (inCount > gMaximumChannelCount)
            inCount = gMaximumChannelCount;
        SPIMChannel theChannel;
        gChannels.resize(inCount, theChannel);
        for (unsigned long i = 0; i < inCount; i++) {
            gChannels[i].mSample = NULL;
            gChannels[i].mHandle = AIL_allocate_sample_handle(gDriver);
        }
        return kSPIMOK;
    }
    return kSPIMChannelBusy;
}

// FUNCTION: MIXER 0x100013d5
// Destroys the channels and shuts Miles and DirectSound down; kSPIMFailed if the channels could not be destroyed
extern "C" long SPIMDeactivateMixer()
{
    long theResult = SPIMDestroyChannels();
    if (theResult == kSPIMOK) {
        AIL_release_sample_handle(gScratchSample);
        gDirectSound->Release();
        AIL_close_digital_driver(gDriver);
        AIL_shutdown();
        TSScheduler::DestroyCriticalSection(gLoadLock);
        gDriver = NULL;
        gDirectSound = NULL;
        gMaximumChannelCount = 0;
        gActive = false;
    } else {
        ESGetNativeError();
        theResult = kSPIMFailed;
    }
    return theResult;
}

// FUNCTION: MIXER 0x1000143f
// Stops the sample's channel and drops its reference to the wave data
extern "C" void SPIMDeleteWaveData(SESample* inSample)
{
    if (inSample->GetMixerChannel() != -1)
        SPIMStopChannel(inSample->GetMixerChannel());
    if (inSample->GetData()) {
        static_cast<UReferenceCounter*>(inSample->GetData())->Dereference();
        inSample->SetData(NULL);
    }
}

// FUNCTION: MIXER 0x10001466
// Ends and releases every channel's Miles sample handle and empties the channel list
extern "C" long SPIMDestroyChannels()
{
    for (unsigned long i = 0; i < gChannels.size(); i++) {
        AIL_end_sample(gChannels[i].mHandle);
        AIL_release_sample_handle(gChannels[i].mHandle);
    }
    gChannels.clear();
    return kSPIMOK;
}

// FUNCTION: MIXER 0x100014a9
// Creates a copy of the sample that shares (references) the same wave data
extern "C" long SPIMDuplicateSample(SESample* inSample, SESample** outSample)
{
    UReferenceCounter* theWaveData = static_cast<UReferenceCounter*>(inSample->GetData());
    SESample* theSample = new SESample(*inSample);
    theWaveData->Reference();
    theSample->SetData(theWaveData);
    *outSample = theSample;
    return kSPIMOK;
}

// FUNCTION: MIXER 0x100014dc
// Returns the number of channels
extern "C" unsigned long SPIMGetChannelCount()
{
    return gChannels.size();
}

// FUNCTION: MIXER 0x100014f4
// Returns the Miles mixer CPU load as a fraction (0..1)
extern "C" float SPIMGetCPUUsage()
{
    return AIL_digital_CPU_percent(gDriver) * 0.01f;
}

// FUNCTION: MIXER 0x10001512
// Returns the DirectSound interface Miles uses
extern "C" LPDIRECTSOUND SPIMGetDirectSoundInterface()
{
    return gDirectSound;
}

// FUNCTION: MIXER 0x10001518
// Returns the Miles timer's highest delay
extern "C" long SPIMGetHighestTimerDelay()
{
    return AIL_get_timer_highest_delay();
}

// FUNCTION: MIXER 0x1000151e
// Returns the channel limit
extern "C" unsigned long SPIMGetMaximumChannelCount()
{
    return gMaximumChannelCount;
}

// FUNCTION: MIXER 0x10001524
// Returns the playback position of the sample's channel (0 when it has none)
extern "C" unsigned long SPIMGetSampleOffset(SESample* inSample)
{
    unsigned long theOffset = 0;
    if (inSample->GetMixerChannel() != -1)
        theOffset = AIL_sample_position(gChannels[inSample->GetMixerChannel()].mHandle);
    return theOffset;
}

// FUNCTION: MIXER 0x10001544
// Returns the sample's total and remaining play time in ms; zeros and kSPIMNativeError when it has no channel
extern "C" long SPIMGetSamplePlayBackTime(SESample* inSample, long* outTotal, long* outRemaining)
{
    if (inSample->GetMixerChannel() != -1) {
        AIL_sample_ms_position(gChannels[inSample->GetMixerChannel()].mHandle, outTotal, outRemaining);
        *outRemaining = *outTotal - *outRemaining;
        return kSPIMOK;
    }
    *outTotal = 0;
    *outRemaining = 0;
    return kSPIMNativeError;
}

// FUNCTION: MIXER 0x10001588
// Returns the master volume (0..1)
extern "C" float SPIMGetVolume()
{
    return gVolume;
}

// FUNCTION: MIXER 0x1000158f
// Stores the game window for DirectSound
extern "C" void SPIMInitializeMixer(HWND* inWindow)
{
    gWindow = inWindow;
    gWindowHandle = *inWindow;
}

// FUNCTION: MIXER 0x100015a0
// Returns kSPIMChannelPlaying while the channel's sample plays, else kSPIMChannelIdle (kSPIMBadParameter: bad index)
extern "C" long SPIMIsChannelBusyPlaying(unsigned long inChannel)
{
    if (inChannel < gChannels.size()) {
        if (gChannels[inChannel].mSample)
            return AIL_sample_status(gChannels[inChannel].mHandle) == SMP_PLAYING ? kSPIMChannelPlaying
                                                                                  : kSPIMChannelIdle;
        return kSPIMChannelIdle;
    }
    return kSPIMBadParameter;
}

// FUNCTION: MIXER 0x100015e5
// Loads a wave file (from disk or a memory file) into Miles memory and fills in the sample's format and length;
// kSPIMFailed when Miles cannot parse it
extern "C" long SPIMLoadWaveData(FSFileSpec* inSpec, SESample* ioSample)
{
    char thePath[256];
    long theResult;
    void* theData;
    unsigned long theSize;
    FSMemoryFile* theMemoryFile = dynamic_cast<FSMemoryFile*>(inSpec);
    theData = NULL;
    theSize = 0;
    gLoadLock->Enter();
    unsigned long theLatency = AIL_get_preference(DIG_DS_MIX_FRAGMENT_CNT) * gFragmentSize;
    SPIMSetLatency(500);
    if (theMemoryFile == NULL) {
        sprintf(thePath, "%s", (const char*)(inSpec->GetVolume() + inSpec->GetDirectory() + inSpec->GetFile()));
        theSize = AIL_file_size(thePath);
        theData = AIL_file_read(thePath, NULL);
        if (theData)
            theResult = kSPIMOK;
        else {
            AIL_last_error();
            theResult = kSPIMNativeError;
        }
    } else {
        theResult = theMemoryFile->GetLength(theSize);
        if (theResult == kSPIMOK) {
            theData = AIL_mem_alloc_lock(theSize);
            if (theData)
                memcpy(theData, theMemoryFile->GetData(), theSize);
            else
                theResult = kSPIMOutOfMemory;
        }
    }
    SPIMSetLatency(theLatency);
    if (theResult == kSPIMOK) {
        AIL_init_sample(gScratchSample);
        if (AIL_set_named_sample_file(gScratchSample, inSpec->GetFile(), theData, theSize, 0)) {
            unsigned long theGranularity = AIL_sample_granularity(gScratchSample);
            ioSample->SetFilename(inSpec->GetFile());
            ioSample->SetData(new SampleData(theData));
            ioSample->SetLengthInBytes(theSize);
            ioSample->SetSampleBits(theGranularity);
            ioSample->SetLengthInSamples(theSize / theGranularity);
            ioSample->SetSampleRate(AIL_sample_playback_rate(gScratchSample));
        } else {
            AIL_mem_free_lock(theData);
            theResult = kSPIMFailed;
        }
    }
    gLoadLock->Leave();
    return theResult;
}

// FUNCTION: MIXER 0x1000180f
// Lets Miles mix ahead (services the driver)
extern "C" long SPIMMixAhead()
{
    AIL_serve();
    return kSPIMOK;
}

// FUNCTION: MIXER 0x10001818
// Starts the channel's sample; kSPIMChannelBusy when already playing, kSPIMNoSample when the channel is empty
extern "C" long SPIMPlayChannel(unsigned long inChannel)
{
    if (inChannel < gChannels.size()) {
        if (gChannels[inChannel].mSample) {
            if (AIL_sample_status(gChannels[inChannel].mHandle) != SMP_PLAYING) {
                AIL_start_sample(gChannels[inChannel].mHandle);
                return kSPIMOK;
            }
            return kSPIMChannelBusy;
        }
        return kSPIMNoSample;
    }
    return kSPIMBadParameter;
}

// FUNCTION: MIXER 0x1000186e
// Does nothing; returns true
extern "C" bool SPIMProcessMessage()
{
    return true;
}

// FUNCTION: MIXER 0x10001871
// Puts a sample on an idle channel and applies its loop, pan, pitch and volume; a NULL sample stops the channel
extern "C" long SPIMSetChannelSample(unsigned long inChannel, SESample* inSample)
{
    // Fixed: the sample's data used to be read before the NULL test, so a NULL sample faulted
    SampleData* theWaveData = inSample ? static_cast<SampleData*>(inSample->GetData()) : NULL;
    long theResult = SPIMIsChannelBusyPlaying(inChannel);
    if (theResult != kSPIMChannelIdle)
        return theResult;
    if (inSample) {
        if (inSample->GetMixerChannel() == -1) {
            SPIMStopChannel(inChannel);
            gChannels[inChannel].mSample = inSample;
            inSample->SetMixerChannel(inChannel);
            AIL_init_sample(gChannels[inChannel].mHandle);
            AIL_set_named_sample_file(gChannels[inChannel].mHandle, inSample->GetFilename(), theWaveData->mData,
                                      inSample->GetLengthInBytes(), 0);
            SPIMSetSampleLooping(inSample, inSample->GetIsLooping());
            SPIMSetSamplePan(inSample, inSample->GetPan());
            SPIMSetSamplePitch(inSample, inSample->GetPitch());
            SPIMSetSampleVolume(inSample, inSample->GetVolume());
            theResult = kSPIMOK;
        } else if (inSample->GetMixerChannel() == inChannel)
            theResult = kSPIMOK;
        else
            theResult = kSPIMSampleOnOtherChannel;
    } else {
        SPIMStopChannel(inChannel);
        theResult = kSPIMOK;
    }
    return theResult;
}

// FUNCTION: MIXER 0x10001955
// Sets the DirectSound mix-ahead in ms (rounded to fragments, capped), or restores the startup value for -1
extern "C" long SPIMSetLatency(unsigned long inMilliseconds)
{
    if (inMilliseconds != -1) {
        unsigned long theCount = inMilliseconds / gFragmentSize;
        if (theCount > gFragmentCount)
            theCount = gFragmentCount;
        AIL_set_preference(DIG_DS_MIX_FRAGMENT_CNT, theCount);
    } else
        AIL_set_preference(DIG_DS_MIX_FRAGMENT_CNT, gMixFragmentCount);
    AIL_serve();
    return kSPIMOK;
}

// FUNCTION: MIXER 0x1000198c
// Sets the sample's looping flag and its channel's loop count (0 = forever)
extern "C" void SPIMSetSampleLooping(SESample* inSample, bool inLooping)
{
    inSample->SetTheIsLooping(inLooping);
    if (inSample->GetMixerChannel() != -1) {
        long theLoopCount;
        if (inLooping)
            theLoopCount = 0;
        else
            theLoopCount = 1;
        AIL_set_sample_loop_count(gChannels[inSample->GetMixerChannel()].mHandle, theLoopCount);
    }
}

// FUNCTION: MIXER 0x100019b9
// Moves the playback position of the sample's channel
extern "C" long SPIMSetSampleOffset(SESample* inSample, unsigned long inOffset)
{
    if (inSample->GetMixerChannel() != -1)
        AIL_set_sample_position(gChannels[inSample->GetMixerChannel()].mHandle, inOffset);
    return kSPIMOK;
}

// FUNCTION: MIXER 0x100019db
// Sets the sample's pan (-1..1) and its channel's Miles pan (0..127)
extern "C" long SPIMSetSamplePan(SESample* inSample, float inPan)
{
    inSample->SetThePan(inPan);
    if (inSample->GetMixerChannel() == -1)
        return kSPIMOK;
    long thePan = UMFloatToLong((inPan + 1.0f) * kMaximumVolume * 0.5f);
    thePan = min(127, max(thePan, 0));
    AIL_set_sample_pan(gChannels[inSample->GetMixerChannel()].mHandle, thePan);
    return kSPIMOK;
}

// FUNCTION: MIXER 0x10001a3f
// Sets the sample's pitch and its channel's playback rate (sample rate times pitch)
extern "C" long SPIMSetSamplePitch(SESample* inSample, float inPitch)
{
    inSample->SetThePitch(inPitch);
    if (inSample->GetMixerChannel() != -1)
        AIL_set_sample_playback_rate(gChannels[inSample->GetMixerChannel()].mHandle,
                                     UMFloatToLong(inSample->GetSampleRate() * inPitch));
    return kSPIMOK;
}

// FUNCTION: MIXER 0x10001a85
// Sets the sample's volume and its channel's Miles volume (0..127)
extern "C" long SPIMSetSampleVolume(SESample* inSample, float inVolume)
{
    inSample->SetTheVolume(inVolume);
    if (inSample->GetMixerChannel() != -1)
        AIL_set_sample_volume(gChannels[inSample->GetMixerChannel()].mHandle, UMFloatToLong(kMaximumVolume * inVolume));
    return kSPIMOK;
}

// FUNCTION: MIXER 0x10001ac2
// Sets the master volume (0..1); kSPIMBadParameter when out of range
extern "C" long SPIMSetVolume(float inVolume)
{
    if (inVolume < 0.0f || inVolume > 1.0f)
        return kSPIMBadParameter;
    AIL_set_digital_master_volume(gDriver, UMFloatToLong(kMaximumVolume * inVolume));
    gVolume = inVolume;
    return kSPIMOK;
}

// FUNCTION: MIXER 0x10001b15
// Stops every channel; returns the last channel's result (kSPIMChannelBusy when that one was empty)
extern "C" long SPIMStopAllChannels()
{
    long theResult = kSPIMOK;
    for (unsigned long i = 0; i < gChannels.size(); i++) {
        theResult = SPIMStopChannel(i);
        if (theResult == kSPIMNativeError) {
            ESGetNativeError();
            break;
        }
    }
    return theResult;
}

// FUNCTION: MIXER 0x10001b4f
// Stops the channel and detaches its sample; kSPIMChannelBusy when the channel is empty
extern "C" long SPIMStopChannel(unsigned long inChannel)
{
    if (inChannel < gChannels.size()) {
        SPIMChannel& theChannel = gChannels[inChannel];
        SESample* theSample = theChannel.mSample;
        if (theSample) {
            AIL_end_sample(theChannel.mHandle);
            theSample->SetMixerChannel(-1);
            gChannels[inChannel].mSample = NULL;
            return kSPIMOK;
        }
        return kSPIMChannelBusy;
    }
    return kSPIMBadParameter;
}
