// Low-Level Engine\Sound Engine\Mixer\Mixer.cpp
// The sound mixer plug-in's shim (' snd'). The mixing, the sample/channel logic and the SPIM entry points that only
// reach them are Rust (rust/crates/ee_mixer ffi.rs, which defines them under their SPIM names); this file keeps the
// miniaudio device and decoder, the entry points over C++ objects (samples' files, the name) and the plug-in's entry
// table the engine looks names up in. The samples are Rust's (rust/crates/ee_sound objects.rs, which also serves
// ee_mixer's ee_mixer_host_* sample access).
#include <windows.h>
#include "platform/PlatFile.h"
#include "platform/PlatSim.h"
#include <math.h>
#include <string>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "miniaudio/ee_miniaudio.h"
#include "lle/LowLevelEngine.h"
#include "rust/ee_mixer.h"

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
static ma_context gContext;
static ma_device gDevice;
static std::string gOutputDevice;

// ---- the device and the decoder (miniaudio)

extern "C" void SPIMSetOutputDevice(const char* inName)
{
    gOutputDevice = inName ? inName : "";
}
extern "C" unsigned long SPIMGetOutputDevices(char (*outNames)[128], unsigned long inCapacity)
{
    ma_context theContext;
    if (ma_context_init(NULL, 0, NULL, &theContext) != MA_SUCCESS)
        return 0;
    ma_device_info* theDevices = NULL;
    ma_uint32 theCount = 0;
    unsigned long theResult = 0;
    if (ma_context_get_devices(&theContext, &theDevices, &theCount, NULL, NULL) == MA_SUCCESS)
        for (ma_uint32 i = 0; i < theCount && theResult < inCapacity; i++) {
            snprintf(outNames[theResult], 128, "%s", theDevices[i].name);
            theResult++;
        }
    ma_context_uninit(&theContext);
    return theResult;
}
static void MixerCallback(ma_device* inDevice, void* outFrames, const void* inInput, ma_uint32 inCount)
{
    ee_mixer_mix(static_cast<float*>(outFrames), inCount);
}
extern "C" long SPIMDeactivateMixer();

static const UString* PIMGetName()
{
    return &gMixerName;
}
// Opens the playback device (stereo float at the device's own rate) and creates the channels
extern "C" long SPIMActivateMixer()
{
    if (ee_mixer_is_active())
        return kSPIMOK;
    // the first backend that opens and starts a playback device; the null backend (no output) always does
#if defined(_WIN32)
    ma_backend theBackends[] = { ma_backend_wasapi, ma_backend_dsound, ma_backend_winmm, ma_backend_null };
#elif defined(__APPLE__)
    ma_backend theBackends[] = { ma_backend_coreaudio, ma_backend_null };
#else
    ma_backend theBackends[] = { ma_backend_pulseaudio, ma_backend_alsa, ma_backend_null };
#endif
    ma_backend theNull[] = { ma_backend_null };
    ma_backend* theList = theBackends;
    unsigned long theCount = sizeof(theBackends) / sizeof(theBackends[0]);
    if (gPlatSimMode) {   // the headless simulation (platform/PlatSim.h): no output
        theList = theNull;
        theCount = 1;
    }
    ma_device_config theConfig = ma_device_config_init(ma_device_type_playback);
    theConfig.playback.format = ma_format_f32;
    theConfig.playback.channels = 2;
    theConfig.sampleRate = 0;                   // the device's own
    theConfig.dataCallback = MixerCallback;
    bool theOpen = false;
    for (unsigned long b = 0; b < theCount && !theOpen; b++) {
        if (ma_context_init(&theList[b], 1, NULL, &gContext) != MA_SUCCESS)
            continue;
        // port: the chosen output device (by name; the system's default when it is not there)
        ma_device_id theDeviceID;
        theConfig.playback.pDeviceID = NULL;
        if (!gOutputDevice.empty()) {
            ma_device_info* theDevices = NULL;
            ma_uint32 theDeviceCount = 0;
            if (ma_context_get_devices(&gContext, &theDevices, &theDeviceCount, NULL, NULL) == MA_SUCCESS)
                for (ma_uint32 d = 0; d < theDeviceCount; d++)
                    if (gOutputDevice == theDevices[d].name) {
                        theDeviceID = theDevices[d].id;
                        theConfig.playback.pDeviceID = &theDeviceID;
                    }
        }
        if (ma_device_init(&gContext, &theConfig, &gDevice) == MA_SUCCESS) {
            if (ma_device_start(&gDevice) == MA_SUCCESS)
                theOpen = true;
            else
                ma_device_uninit(&gDevice);
        }
        if (!theOpen)
            ma_context_uninit(&gContext);
    }
    if (!theOpen)
        return kSPIMNativeError;
    ee_mixer_activated(gDevice.sampleRate);
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
    ma_device_uninit(&gDevice);
    ma_context_uninit(&gContext);
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
// Decodes a whole file image through miniaudio; a NULL sample when it cannot be decoded
static const void* Decode(const void* inData, unsigned long inSize)
{
    ma_decoder_config theConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder theDecoder;
    if (ma_decoder_init_memory(inData, inSize, &theConfig, &theDecoder) != MA_SUCCESS)
        return NULL;
    unsigned long theChannels = theDecoder.outputChannels;
    unsigned long theRate = theDecoder.outputSampleRate;
    std::vector<float> theBlock(4096 * theChannels);
    std::vector<float> thePCM;
    for (;;) {
        ma_uint64 theRead = 0;
        if (ma_decoder_read_pcm_frames(&theDecoder, &theBlock[0], 4096, &theRead) != MA_SUCCESS && theRead == 0)
            break;
        thePCM.insert(thePCM.end(), theBlock.begin(), theBlock.begin() + (size_t)theRead * theChannels);
        if (theRead < 4096)
            break;
    }
    ma_decoder_uninit(&theDecoder);
    return ee_mixer_sample_create(static_cast<const uint8_t*>(inData), (uint32_t)inSize, thePCM.empty() ? NULL : &thePCM[0],
                                  thePCM.size(), (uint32_t)theChannels, (uint32_t)theRate);
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
    const void* theWaveData = Decode(theData, theSize);
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
