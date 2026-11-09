// Low-Level Engine\Sound Engine\Mixer\Mixer.cpp
// The sound mixer plug-in's shim (' snd'). The mixing and the sample/channel logic are Rust
// (rust/crates/ee_mixer); this file keeps the miniaudio device and decoder, the SESample access Rust needs, and the
// SPIM entry points the engine looks up by name.
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
#include "rust/ee_rust.h"

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
static unsigned long gMixerType = ' snd';
static HWND gWindowHandle;
static ma_context gContext;
static ma_device gDevice;
static std::string gOutputDevice;

// ---- what Rust needs from the C++ SESample

extern "C" uint32_t ee_mixer_host_mixer_channel(void* inSample)
{
    return (uint32_t)static_cast<SESample*>(inSample)->GetMixerChannel();
}
extern "C" void ee_mixer_host_set_mixer_channel(void* inSample, uint32_t inChannel)
{
    static_cast<SESample*>(inSample)->SetMixerChannel(inChannel == 0xffffffffu ? (unsigned long)-1 : inChannel);
}
extern "C" const void* ee_mixer_host_data(void* inSample)
{
    return static_cast<SESample*>(inSample)->GetData();
}
extern "C" void ee_mixer_host_set_data(void* inSample, const void* inData)
{
    static_cast<SESample*>(inSample)->SetData(const_cast<void*>(inData));
}
extern "C" bool ee_mixer_host_is_looping(void* inSample)
{
    return static_cast<SESample*>(inSample)->GetIsLooping();
}
extern "C" float ee_mixer_host_pan(void* inSample)
{
    return static_cast<SESample*>(inSample)->GetPan();
}
extern "C" float ee_mixer_host_pitch(void* inSample)
{
    return static_cast<SESample*>(inSample)->GetPitch();
}
extern "C" float ee_mixer_host_volume(void* inSample)
{
    return static_cast<SESample*>(inSample)->GetVolume();
}
extern "C" void ee_mixer_host_set_is_looping(void* inSample, bool inLooping)
{
    static_cast<SESample*>(inSample)->SetTheIsLooping(inLooping);
}
extern "C" void ee_mixer_host_set_pan(void* inSample, float inPan)
{
    static_cast<SESample*>(inSample)->SetThePan(inPan);
}
extern "C" void ee_mixer_host_set_pitch(void* inSample, float inPitch)
{
    static_cast<SESample*>(inSample)->SetThePitch(inPitch);
}
extern "C" void ee_mixer_host_set_volume(void* inSample, float inVolume)
{
    static_cast<SESample*>(inSample)->SetTheVolume(inVolume);
}

// ---- the device and the decoder (miniaudio)

extern "C" void SPIMSetMasterVolume(float inVolume)
{
    ee_mixer_set_master_volume(inVolume);
}

extern "C" void SPIMSetMuted(bool inMuted)
{
    ee_mixer_set_muted(inMuted);
}

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

extern "C" long SPIMCreateChannels(unsigned long inCount, SPIMChannelInfo* outInfo);
extern "C" long SPIMDeactivateMixer();

static const UString* PIMGetName()
{
    return &gMixerName;
}

static unsigned long PIMGetType()
{
    return gMixerType;
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

extern "C" long SPIMCreateChannels(unsigned long inCount, SPIMChannelInfo* outInfo)
{
    return ee_mixer_create_channels((uint32_t)inCount);
}

extern "C" long SPIMDestroyChannels()
{
    return ee_mixer_destroy_channels();
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

extern "C" void SPIMDeleteWaveData(SESample* inSample)
{
    ee_mixer_delete_wave_data(inSample);
}

// Creates a copy of the sample that shares (references) the same wave data
extern "C" long SPIMDuplicateSample(SESample* inSample, SESample** outSample)
{
    void* theWaveData = inSample->GetData();
    SESample* theSample = new SESample(*inSample);
    if (theWaveData)   // the original sample's wave did not load: the copy is silent too
        ee_mixer_sample_reference(theWaveData);
    theSample->SetData(theWaveData);
    *outSample = theSample;
    return kSPIMOK;
}

extern "C" unsigned long SPIMGetChannelCount()
{
    return ee_mixer_channel_count();
}

extern "C" float SPIMGetCPUUsage()
{
    return 0.0f;
}

extern "C" void* SPIMGetDirectSoundInterface()
{
    return NULL;
}

extern "C" unsigned long SPIMGetOutputRate()
{
    return ee_mixer_output_rate();
}

extern "C" void SPIMSetMusicSource(void (*inSource)(void*, float*, unsigned long), void* inContext)
{
    ee_mixer_set_music_source(inSource, inContext);
}

extern "C" void SPIMSetMovieSource(void (*inSource)(void*, float*, unsigned long), void* inContext)
{
    ee_mixer_set_movie_source(inSource, inContext);
}

extern "C" long SPIMGetHighestTimerDelay()
{
    return 0;
}

extern "C" unsigned long SPIMGetMaximumChannelCount()
{
    return ee_mixer_maximum_channel_count();
}

extern "C" unsigned long SPIMGetSampleOffset(SESample* inSample)
{
    return ee_mixer_sample_offset(inSample);
}

extern "C" long SPIMGetSamplePlayBackTime(SESample* inSample, long* outTotal, long* outRemaining)
{
    int32_t theTotal = 0, theRemaining = 0;
    long theResult = ee_mixer_sample_play_back_time(inSample, &theTotal, &theRemaining);
    *outTotal = theTotal;
    *outRemaining = theRemaining;
    return theResult;
}

extern "C" float SPIMGetVolume()
{
    return ee_mixer_volume();
}

extern "C" void SPIMInitializeMixer(HWND* inWindow)
{
    gWindowHandle = *inWindow;
}

extern "C" long SPIMIsChannelBusyPlaying(unsigned long inChannel)
{
    return ee_mixer_is_channel_busy_playing((uint32_t)inChannel);
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

extern "C" long SPIMMixAhead()
{
    return kSPIMOK;
}

extern "C" long SPIMPlayChannel(unsigned long inChannel)
{
    return ee_mixer_play_channel((uint32_t)inChannel);
}

extern "C" bool SPIMProcessMessage()
{
    return true;
}

extern "C" long SPIMSetChannelSample(unsigned long inChannel, SESample* inSample)
{
    return ee_mixer_set_channel_sample((uint32_t)inChannel, inSample);
}

extern "C" long SPIMSetLatency(unsigned long inMilliseconds)
{
    return kSPIMOK;
}

extern "C" void SPIMSetSampleLooping(SESample* inSample, bool inLooping)
{
    ee_mixer_set_sample_looping(inSample, inLooping);
}

extern "C" long SPIMSetSampleOffset(SESample* inSample, unsigned long inOffset)
{
    return ee_mixer_set_sample_offset(inSample, (uint32_t)inOffset);
}

extern "C" long SPIMSetSamplePan(SESample* inSample, float inPan)
{
    return ee_mixer_set_sample_pan(inSample, inPan);
}

extern "C" long SPIMSetSamplePitch(SESample* inSample, float inPitch)
{
    return ee_mixer_set_sample_pitch(inSample, inPitch);
}

extern "C" long SPIMSetSampleVolume(SESample* inSample, float inVolume)
{
    return ee_mixer_set_sample_volume(inSample, inVolume);
}

extern "C" long SPIMSetVolume(float inVolume)
{
    return ee_mixer_set_volume(inVolume);
}

extern "C" long SPIMStopAllChannels()
{
    return ee_mixer_stop_all_channels();
}

extern "C" long SPIMStopChannel(unsigned long inChannel)
{
    return ee_mixer_stop_channel((uint32_t)inChannel);
}

#include "lle/PIMModule.h"

// The plug-in's entry points (registered in PIMModule.cpp)
static const PIMEntry sEntries[] = {
    PIM_ENTRY(PIMGetName),
    PIM_ENTRY(PIMGetType),
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
