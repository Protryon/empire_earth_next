// Low-Level Engine\Sound Engine\Mixer\Mixer.cpp
// The sound mixer plug-in's shim (' snd'). The mixing, the sample/channel logic, the output device (cpal) and the
// decoder (symphonia) are Rust (rust/crates/ee_audio/src/mixer: ffi.rs defines the SPIM entry points that only reach
// them under their SPIM names, device.rs the device and the decoder); this file keeps the entry points over C++
// objects (samples' files, the name, the chosen output device) and the plug-in's entry table the engine looks names
// up in. The samples are Rust's (rust/crates/ee_audio/src/sound objects.rs, which also serves
// ee_mixer's ee_mixer_host_* sample access).
#include <windows.h>
#include "platform/PlatFile.h"
#include "platform/PlatSim.h"
#include <math.h>
#include <string>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "rust/audio/mixer.h"

// ---- Result codes returned by the SPIM entry points
enum {
    kSPIMOK = 0,
    kSPIMNativeError = 1,
    kSPIMOutOfMemory = 8,
    kSPIMFailed = 9,
};

struct SPIMChannelInfo {                         // the engine passes one; never read
    unsigned long _unused[3];
};

static UString gMixerName("Mixer");
static HWND gWindowHandle;
static std::string gOutputDevice;

// ---- the device (Rust: mixer/device.rs)

extern "C" void SPIMSetOutputDevice(const char* inName)
{
    gOutputDevice = inName ? inName : "";
}
extern "C" unsigned long SPIMGetOutputDevices(char (*outNames)[128], unsigned long inCapacity)
{
    return ee_mixer_output_devices(&outNames[0][0], 128, (uint32_t)inCapacity);
}
extern "C" long SPIMDeactivateMixer();

static const UString* PIMGetName()
{
    return &gMixerName;
}
// Opens the playback device (stereo float at the device's own rate: the chosen output, else the system's default,
// else the null device, which always opens) and creates the channels
extern "C" long SPIMActivateMixer()
{
    if (ee_mixer_is_active())
        return kSPIMOK;
    // the headless simulation (platform/PlatSim.h) has no output: the null device
    uint32_t theRate = ee_mixer_device_open(gOutputDevice.c_str(), gPlatSimMode != 0);
    ee_mixer_activated(theRate);
    return kSPIMOK;
}
// Returns whether the mixer works: activates and deactivates it once
static bool PIMIsValid()
{
    if (SPIMActivateMixer() != kSPIMOK)
        return false;
    SPIMDeactivateMixer();
    return true;
}
// Destroys the channels and closes the device
extern "C" long SPIMDeactivateMixer()
{
    if (!ee_mixer_is_active())
        return kSPIMOK;
    SPIMDestroyChannels();
    ee_mixer_device_close();
    ee_mixer_deactivated();
    return kSPIMOK;
}
// Creates a copy of the sample that shares (references) the same wave data
extern "C" long SPIMDuplicateSample(SESample* inSample, SESample** outSample)
{
    void* theWaveData = inSample->GetData();
    SESample* theSample = SESample::Duplicate(*inSample);
    if (theWaveData)   // the original sample's wave did not load: the copy is silent too
        ee_mixer_sample_reference(theWaveData);
    theSample->SetData(theWaveData);
    *outSample = theSample;
    return kSPIMOK;
}
extern "C" void SPIMInitializeMixer(HWND* inWindow)
{
    gWindowHandle = *inWindow;
}
// Loads a sound file (from disk or a memory file), decodes it and fills in the sample's format and length;
// kSPIMFailed when it cannot be decoded
extern "C" long SPIMLoadWaveData(FSFileSpec* inSpec, SESample* ioSample)
{
    std::vector<unsigned char> theFile;
    const void* theData = NULL;
    unsigned long theSize = 0;
    FSMemoryFile* theMemoryFile = dynamic_cast<FSMemoryFile*>(inSpec);
    if (theMemoryFile == NULL) {
        char thePath[MAX_PATH];
        sprintf(thePath, "%s", (const char*)(inSpec->GetVolume() + inSpec->GetDirectory() + inSpec->GetFile()));
        FILE* theStream = PlatFOpen(thePath, "rb");
        if (theStream == NULL)
            return kSPIMNativeError;
        fseek(theStream, 0, SEEK_END);
        long theLength = ftell(theStream);
        fseek(theStream, 0, SEEK_SET);
        if (theLength > 0) {
            theFile.resize(theLength);
            theSize = (unsigned long)fread(&theFile[0], 1, theLength, theStream);
        }
        fclose(theStream);
        if (theSize == 0)
            return kSPIMNativeError;
        theData = &theFile[0];
    } else {
        if (theMemoryFile->GetLength(theSize) != kSPIMOK)
            return kSPIMNativeError;
        theData = theMemoryFile->GetData();
    }
    const void* theWaveData = ee_mixer_decode(static_cast<const uint8_t*>(theData), (uint32_t)theSize);
    if (theWaveData == NULL)
        return kSPIMFailed;
    uint32_t theFrames, theRate, theFrameBytes;
    ee_mixer_sample_info(theWaveData, &theFrames, &theRate, &theFrameBytes);
    ioSample->SetFilename(inSpec->GetFile());
    ioSample->SetData(const_cast<void*>(theWaveData));
    ioSample->SetLengthInBytes(theSize);
    ioSample->SetSampleBits(theFrameBytes);
    ioSample->SetLengthInSamples(theFrames);
    ioSample->SetSampleRate(theRate);
    return kSPIMOK;
}

#include "lle/PIMModule.h"

// The plug-in's entry points (registered in PIMModule.cpp)
static const PIMEntry sEntries[] = {
    PIM_ENTRY(PIMGetName),
    {"PIMGetType", reinterpret_cast<void*>(&ee_mixer_pim_type)},
    PIM_ENTRY(PIMIsValid),
    PIM_ENTRY(SPIMActivateMixer),
    PIM_ENTRY(SPIMCreateChannels),
    PIM_ENTRY(SPIMDeactivateMixer),
    PIM_ENTRY(SPIMDeleteWaveData),
    PIM_ENTRY(SPIMDestroyChannels),
    PIM_ENTRY(SPIMDuplicateSample),
    PIM_ENTRY(SPIMGetCPUUsage),
    PIM_ENTRY(SPIMGetChannelCount),
    PIM_ENTRY(SPIMGetDirectSoundInterface),
    PIM_ENTRY(SPIMGetHighestTimerDelay),
    PIM_ENTRY(SPIMGetMaximumChannelCount),
    PIM_ENTRY(SPIMGetOutputRate),
    PIM_ENTRY(SPIMGetSampleOffset),
    PIM_ENTRY(SPIMGetSamplePlayBackTime),
    PIM_ENTRY(SPIMGetVolume),
    PIM_ENTRY(SPIMInitializeMixer),
    PIM_ENTRY(SPIMIsChannelBusyPlaying),
    PIM_ENTRY(SPIMLoadWaveData),
    PIM_ENTRY(SPIMMixAhead),
    PIM_ENTRY(SPIMPlayChannel),
    PIM_ENTRY(SPIMProcessMessage),
    PIM_ENTRY(SPIMSetChannelSample),
    PIM_ENTRY(SPIMSetLatency),
    PIM_ENTRY(SPIMSetMovieSource),
    PIM_ENTRY(SPIMSetMusicSource),
    PIM_ENTRY(SPIMSetSampleLooping),
    PIM_ENTRY(SPIMSetSampleOffset),
    PIM_ENTRY(SPIMSetSamplePan),
    PIM_ENTRY(SPIMSetSamplePitch),
    PIM_ENTRY(SPIMSetSampleVolume),
    PIM_ENTRY(SPIMSetVolume),
    PIM_ENTRY(SPIMStopAllChannels),
    PIM_ENTRY(SPIMStopChannel),
    { NULL, NULL },
};
extern const PIMPlugin gPIMPluginMixer = { "Mixer", sEntries };
