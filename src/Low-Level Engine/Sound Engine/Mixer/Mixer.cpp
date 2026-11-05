// Low-Level Engine\Sound Engine\Mixer\Mixer.cpp
// The sound mixer plug-in (' snd'; the original was "Miles Sound System Mixer.dll", built on Miles 6). The game's
// sample channels, the music and a movie's sound are mixed in software into one miniaudio playback device (WASAPI, DirectSound or waveOut, and a
// null device when the machine has no audio output). Samples are decoded whole when they are loaded (WAV, MP3,
// FLAC through miniaudio's decoders) and shared, reference-counted, by a sample and its duplicates. The SPIM entry
// points and their result codes are the original plug-in's.
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "miniaudio/ee_miniaudio.h"
#include "lle/LowLevelEngine.h"

// ---- Result codes returned by the SPIM entry points
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

const unsigned long kMaximumChannels = 32;     // channels the game may create (the Miles mixer usually had 16)

struct SPIMChannelInfo {                         // the engine passes one; never read
    unsigned long _unused[3];
};

// A loaded sample's decoded audio, shared by the sample and its duplicates
class SampleData : public UReferenceCounter {
public:
    SampleData() : mFrames(0), mChannels(0), mRate(0), mFrameBytes(0) {}
    virtual ~SampleData() {}

    std::vector<float> mPCM;                // interleaved, mChannels (1 or 2) per frame
    unsigned long mFrames;
    unsigned long mChannels;
    unsigned long mRate;
    unsigned long mFrameBytes;              // bytes per frame of the file's own format (byte offsets, granularity)
};

// One mixing channel: the sample bound to it and its playback state
struct MixerChannel {
    SESample* mSample;                      // NULL: free
    SampleData* mData;
    double mPosition;                       // in source frames
    double mStep;                           // source frames per output frame (rate * pitch / device rate)
    float mLeft;                            // volume and pan as per-side gains
    float mRight;
    bool mPlaying;
    bool mLooping;
};

static std::vector<MixerChannel> gChannels;
static UString gMixerName("Mixer");
static unsigned long gMixerType = ' snd';
static float gVolume = 1.0f;
static unsigned long gMaximumChannelCount;
static HWND gWindowHandle;
static bool gActive;
static ma_context gContext;
static ma_device gDevice;
// the music (the conductor's renderer) and a movie's sound: each adds interleaved stereo float frames at the device rate
static void (*gMusicSource)(void* inContext, float* outFrames, unsigned long inCount);
static void* gMusicContext;
static void (*gMovieSource)(void* inContext, float* outFrames, unsigned long inCount);
static void* gMovieContext;
static std::vector<float> gMusicBuffer;
static CRITICAL_SECTION gLock;              // the channels, between the game's threads and the device thread
static struct LockInit {
    LockInit() { InitializeCriticalSection(&gLock); }
} sLockInit;

struct Guard {
    Guard() { EnterCriticalSection(&gLock); }
    ~Guard() { LeaveCriticalSection(&gLock); }
};

extern "C" long SPIMActivateMixer();
extern "C" long SPIMCreateChannels(unsigned long inCount, SPIMChannelInfo* outInfo);
extern "C" long SPIMDeactivateMixer();
extern "C" long SPIMDestroyChannels();
extern "C" void SPIMSetSampleLooping(SESample* inSample, bool inLooping);
extern "C" long SPIMSetSamplePan(SESample* inSample, float inPan);
extern "C" long SPIMSetSamplePitch(SESample* inSample, float inPitch);
extern "C" long SPIMSetSampleVolume(SESample* inSample, float inVolume);
extern "C" long SPIMStopChannel(unsigned long inChannel);

static MixerChannel* ChannelOf(SESample* inSample)
{
    unsigned long theChannel = inSample->GetMixerChannel();
    return theChannel < gChannels.size() && gChannels[theChannel].mSample == inSample ? &gChannels[theChannel] : NULL;
}

// The per-side gains of a volume (0..1) and a pan (-1 left .. 1 right)
static void SetGains(MixerChannel& ioChannel, float inVolume, float inPan)
{
    if (inPan < -1.0f)
        inPan = -1.0f;
    if (inPan > 1.0f)
        inPan = 1.0f;
    ioChannel.mLeft = inVolume * (inPan > 0.0f ? 1.0f - inPan : 1.0f);
    ioChannel.mRight = inVolume * (inPan < 0.0f ? 1.0f + inPan : 1.0f);
}

static void SetStep(MixerChannel& ioChannel, float inPitch)
{
    ioChannel.mStep = ioChannel.mData && gDevice.sampleRate
                          ? (double)ioChannel.mData->mRate * inPitch / gDevice.sampleRate : 0.0;
}

// The device thread: mixes every playing channel (linear interpolation) into the stereo float output
static void MixerCallback(ma_device* inDevice, void* outFrames, const void* inInput, ma_uint32 inCount)
{
    float* theOut = static_cast<float*>(outFrames);
    memset(theOut, 0, inCount * 2 * sizeof(float));
    Guard theGuard;
    for (unsigned long c = 0; c < gChannels.size(); c++) {
        MixerChannel& theChannel = gChannels[c];
        const SampleData* theData = theChannel.mData;
        if (!theChannel.mPlaying || theData == NULL || theData->mFrames == 0 || theChannel.mStep <= 0.0)
            continue;
        const float* thePCM = &theData->mPCM[0];
        const unsigned long theFrames = theData->mFrames;
        const unsigned long theWidth = theData->mChannels;
        for (ma_uint32 i = 0; i < inCount; i++) {
            if (theChannel.mPosition >= theFrames) {
                if (!theChannel.mLooping) {
                    theChannel.mPlaying = false;
                    break;
                }
                theChannel.mPosition = fmod(theChannel.mPosition, (double)theFrames);
            }
            unsigned long theIndex = (unsigned long)theChannel.mPosition;
            float theFraction = (float)(theChannel.mPosition - theIndex);
            unsigned long theNext = theIndex + 1 < theFrames ? theIndex + 1 : (theChannel.mLooping ? 0 : theIndex);
            const float* a = thePCM + theIndex * theWidth;
            const float* b = thePCM + theNext * theWidth;
            float theLeft = a[0] + (b[0] - a[0]) * theFraction;
            float theRight = theWidth > 1 ? a[1] + (b[1] - a[1]) * theFraction : theLeft;
            theOut[2 * i] += theLeft * theChannel.mLeft;
            theOut[2 * i + 1] += theRight * theChannel.mRight;
            theChannel.mPosition += theChannel.mStep;
        }
    }
    // the effects take the master volume; the music has its own (the conductor's)
    bool theMusic = gMusicSource != NULL || gMovieSource != NULL;
    if (theMusic) {
        if (gMusicBuffer.size() < 2 * inCount)
            gMusicBuffer.resize(2 * inCount);
        memset(&gMusicBuffer[0], 0, 2 * inCount * sizeof(float));
        if (gMusicSource)
            gMusicSource(gMusicContext, &gMusicBuffer[0], inCount);
        if (gMovieSource)
            gMovieSource(gMovieContext, &gMusicBuffer[0], inCount);
    }
    for (ma_uint32 i = 0; i < 2 * inCount; i++) {
        float v = theOut[i] * gVolume + (theMusic ? gMusicBuffer[i] : 0.0f);
        theOut[i] = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
    }
}

// Bytes per frame of the file's own format (a WAV's block alignment; 2 bytes per channel otherwise)
static unsigned long FileFrameBytes(const unsigned char* inData, unsigned long inSize, unsigned long inChannels)
{
    if (inSize >= 12 && memcmp(inData, "RIFF", 4) == 0 && memcmp(inData + 8, "WAVE", 4) == 0) {
        for (unsigned long i = 12; i + 8 <= inSize;) {
            unsigned long theChunk = inData[i + 4] | inData[i + 5] << 8 | inData[i + 6] << 16 | (unsigned long)inData[i + 7] << 24;
            if (memcmp(inData + i, "fmt ", 4) == 0 && i + 22 <= inSize)
                return inData[i + 20] | inData[i + 21] << 8;
            i += 8 + theChunk + (theChunk & 1);
        }
    }
    return 2 * inChannels;
}

// Decodes a whole file image into a SampleData (NULL when miniaudio cannot decode it)
static SampleData* Decode(const void* inData, unsigned long inSize)
{
    ma_decoder_config theConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder theDecoder;
    if (ma_decoder_init_memory(inData, inSize, &theConfig, &theDecoder) != MA_SUCCESS)
        return NULL;
    SampleData* theSample = new SampleData;
    unsigned long theChannels = theDecoder.outputChannels;
    theSample->mChannels = theChannels > 2 ? 2 : theChannels;
    theSample->mRate = theDecoder.outputSampleRate;
    theSample->mFrameBytes = FileFrameBytes(static_cast<const unsigned char*>(inData), inSize, theChannels);
    std::vector<float> theBlock(4096 * theChannels);
    for (;;) {
        ma_uint64 theRead = 0;
        if (ma_decoder_read_pcm_frames(&theDecoder, &theBlock[0], 4096, &theRead) != MA_SUCCESS && theRead == 0)
            break;
        for (ma_uint64 f = 0; f < theRead; f++)
            for (unsigned long c = 0; c < theSample->mChannels; c++)
                theSample->mPCM.push_back(theBlock[(size_t)f * theChannels + c]);
        if (theRead < 4096)
            break;
    }
    ma_decoder_uninit(&theDecoder);
    theSample->mFrames = theSample->mChannels ? (unsigned long)(theSample->mPCM.size() / theSample->mChannels) : 0;
    if (theSample->mFrames == 0) {
        delete theSample;
        return NULL;
    }
    return theSample;
}

// Returns the plug-in's name
static const UString* PIMGetName()
{
    return &gMixerName;
}

// Returns the plug-in type (' snd')
static unsigned long PIMGetType()
{
    return gMixerType;
}

// Returns whether the mixer works: activates and deactivates it once
static bool PIMIsValid()
{
    if (SPIMActivateMixer() != kSPIMOK)
        return false;
    SPIMDeactivateMixer();
    return true;
}

// Opens the playback device (stereo float at the device's own rate) and creates the channels
extern "C" long SPIMActivateMixer()
{
    if (gActive)
        return kSPIMOK;
    // the first backend that opens and starts a playback device; the null backend (no output) always does
    ma_backend theBackends[] = { ma_backend_wasapi, ma_backend_dsound, ma_backend_winmm, ma_backend_null };
    ma_device_config theConfig = ma_device_config_init(ma_device_type_playback);
    theConfig.playback.format = ma_format_f32;
    theConfig.playback.channels = 2;
    theConfig.sampleRate = 0;                   // the device's own
    theConfig.dataCallback = MixerCallback;
    bool theOpen = false;
    for (unsigned long b = 0; b < sizeof(theBackends) / sizeof(theBackends[0]) && !theOpen; b++) {
        if (ma_context_init(&theBackends[b], 1, NULL, &gContext) != MA_SUCCESS)
            continue;
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
    gMaximumChannelCount = kMaximumChannels;
    gActive = true;
    SPIMChannelInfo theInfo;
    SPIMCreateChannels(gMaximumChannelCount, &theInfo);
    return kSPIMOK;
}

// Creates up to the channel limit of empty channels; kSPIMChannelBusy if channels exist
extern "C" long SPIMCreateChannels(unsigned long inCount, SPIMChannelInfo* outInfo)
{
    Guard theGuard;
    if (gChannels.size() != 0)
        return kSPIMChannelBusy;
    if (inCount > gMaximumChannelCount)
        inCount = gMaximumChannelCount;
    MixerChannel theChannel = {};
    gChannels.assign(inCount, theChannel);
    return kSPIMOK;
}

// Destroys the channels and closes the device
extern "C" long SPIMDeactivateMixer()
{
    if (!gActive)
        return kSPIMOK;
    SPIMDestroyChannels();
    ma_device_uninit(&gDevice);
    ma_context_uninit(&gContext);
    gMaximumChannelCount = 0;
    gActive = false;
    return kSPIMOK;
}

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

// Detaches every channel's sample and empties the channel list
extern "C" long SPIMDestroyChannels()
{
    Guard theGuard;
    for (unsigned long i = 0; i < gChannels.size(); i++)
        if (gChannels[i].mSample)
            gChannels[i].mSample->SetMixerChannel(-1);
    gChannels.clear();
    return kSPIMOK;
}

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

extern "C" unsigned long SPIMGetChannelCount()
{
    Guard theGuard;
    return gChannels.size();
}

// The mixer runs on the device's own thread; there is no load figure to report
extern "C" float SPIMGetCPUUsage()
{
    return 0.0f;
}

// The original handed out Miles' DirectSound object (for DirectMusic and Bink); nothing uses DirectSound any more
extern "C" void* SPIMGetDirectSoundInterface()
{
    return NULL;
}

// The device's sample rate (0 while the mixer is not active): the music renders at it
extern "C" unsigned long SPIMGetOutputRate()
{
    return gActive ? gDevice.sampleRate : 0;
}

// Sets (or clears, NULL) the music source the device thread mixes in
extern "C" void SPIMSetMusicSource(void (*inSource)(void*, float*, unsigned long), void* inContext)
{
    Guard theGuard;
    gMusicSource = inSource;
    gMusicContext = inContext;
}

// Sets (or clears, NULL) the movie's sound source the device thread mixes in
extern "C" void SPIMSetMovieSource(void (*inSource)(void*, float*, unsigned long), void* inContext)
{
    Guard theGuard;
    gMovieSource = inSource;
    gMovieContext = inContext;
}

extern "C" long SPIMGetHighestTimerDelay()
{
    return 0;
}

extern "C" unsigned long SPIMGetMaximumChannelCount()
{
    return gMaximumChannelCount;
}

// Returns the playback position of the sample's channel in bytes of the file's format (0 when it has none)
extern "C" unsigned long SPIMGetSampleOffset(SESample* inSample)
{
    Guard theGuard;
    MixerChannel* theChannel = ChannelOf(inSample);
    return theChannel && theChannel->mData ? (unsigned long)theChannel->mPosition * theChannel->mData->mFrameBytes : 0;
}

// Returns the sample's total and remaining play time in ms; zeros and kSPIMNativeError when it has no channel
extern "C" long SPIMGetSamplePlayBackTime(SESample* inSample, long* outTotal, long* outRemaining)
{
    Guard theGuard;
    MixerChannel* theChannel = ChannelOf(inSample);
    if (theChannel && theChannel->mData && theChannel->mData->mRate) {
        double theRate = theChannel->mData->mRate;
        *outTotal = (long)(theChannel->mData->mFrames * 1000.0 / theRate);
        long thePosition = (long)(theChannel->mPosition * 1000.0 / theRate);
        *outRemaining = thePosition < *outTotal ? *outTotal - thePosition : 0;
        return kSPIMOK;
    }
    *outTotal = 0;
    *outRemaining = 0;
    return kSPIMNativeError;
}

extern "C" float SPIMGetVolume()
{
    return gVolume;
}

// Stores the game window (the DirectSound bridge's cooperative level)
extern "C" void SPIMInitializeMixer(HWND* inWindow)
{
    gWindowHandle = *inWindow;
}

// kSPIMChannelPlaying while the channel's sample plays, else kSPIMChannelIdle (kSPIMBadParameter: bad index)
extern "C" long SPIMIsChannelBusyPlaying(unsigned long inChannel)
{
    Guard theGuard;
    if (inChannel >= gChannels.size())
        return kSPIMBadParameter;
    return gChannels[inChannel].mSample && gChannels[inChannel].mPlaying ? kSPIMChannelPlaying : kSPIMChannelIdle;
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
        FILE* theStream = fopen(thePath, "rb");
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
    SampleData* theWaveData = Decode(theData, theSize);
    if (theWaveData == NULL)
        return kSPIMFailed;
    ioSample->SetFilename(inSpec->GetFile());
    ioSample->SetData(theWaveData);
    ioSample->SetLengthInBytes(theSize);
    ioSample->SetSampleBits(theWaveData->mFrameBytes);
    ioSample->SetLengthInSamples(theWaveData->mFrames);
    ioSample->SetSampleRate(theWaveData->mRate);
    return kSPIMOK;
}

// The device thread mixes ahead on its own
extern "C" long SPIMMixAhead()
{
    return kSPIMOK;
}

// Starts the channel's sample (from the start once it has played out); kSPIMChannelBusy when already playing,
// kSPIMNoSample when the channel is empty
extern "C" long SPIMPlayChannel(unsigned long inChannel)
{
    Guard theGuard;
    if (inChannel >= gChannels.size())
        return kSPIMBadParameter;
    MixerChannel& theChannel = gChannels[inChannel];
    if (!theChannel.mSample)
        return kSPIMNoSample;
    if (theChannel.mPlaying)
        return kSPIMChannelBusy;
    if (theChannel.mData && theChannel.mPosition >= theChannel.mData->mFrames)
        theChannel.mPosition = 0.0;
    theChannel.mPlaying = true;
    return kSPIMOK;
}

extern "C" bool SPIMProcessMessage()
{
    return true;
}

// Puts a sample on an idle channel and applies its loop, pan, pitch and volume; a NULL sample stops the channel
extern "C" long SPIMSetChannelSample(unsigned long inChannel, SESample* inSample)
{
    long theResult = SPIMIsChannelBusyPlaying(inChannel);
    if (theResult != kSPIMChannelIdle)
        return theResult;
    if (inSample == NULL) {
        SPIMStopChannel(inChannel);
        return kSPIMOK;
    }
    if (inSample->GetMixerChannel() == inChannel)
        return kSPIMOK;
    if (inSample->GetMixerChannel() != -1)
        return kSPIMSampleOnOtherChannel;
    SPIMStopChannel(inChannel);
    {
        Guard theGuard;
        MixerChannel& theChannel = gChannels[inChannel];
        theChannel.mSample = inSample;
        theChannel.mData = static_cast<SampleData*>(inSample->GetData());
        theChannel.mPosition = 0.0;
        theChannel.mPlaying = false;
        inSample->SetMixerChannel(inChannel);
    }
    SPIMSetSampleLooping(inSample, inSample->GetIsLooping());
    SPIMSetSamplePan(inSample, inSample->GetPan());
    SPIMSetSamplePitch(inSample, inSample->GetPitch());
    SPIMSetSampleVolume(inSample, inSample->GetVolume());
    return kSPIMOK;
}

// The device's own buffering decides the latency
extern "C" long SPIMSetLatency(unsigned long inMilliseconds)
{
    return kSPIMOK;
}

extern "C" void SPIMSetSampleLooping(SESample* inSample, bool inLooping)
{
    Guard theGuard;
    inSample->SetTheIsLooping(inLooping);
    if (MixerChannel* theChannel = ChannelOf(inSample))
        theChannel->mLooping = inLooping;
}

// Moves the playback position of the sample's channel (bytes of the file's format)
extern "C" long SPIMSetSampleOffset(SESample* inSample, unsigned long inOffset)
{
    Guard theGuard;
    MixerChannel* theChannel = ChannelOf(inSample);
    if (theChannel && theChannel->mData && theChannel->mData->mFrameBytes)
        theChannel->mPosition = (double)(inOffset / theChannel->mData->mFrameBytes);
    return kSPIMOK;
}

extern "C" long SPIMSetSamplePan(SESample* inSample, float inPan)
{
    Guard theGuard;
    inSample->SetThePan(inPan);
    if (MixerChannel* theChannel = ChannelOf(inSample))
        SetGains(*theChannel, inSample->GetVolume(), inPan);
    return kSPIMOK;
}

extern "C" long SPIMSetSamplePitch(SESample* inSample, float inPitch)
{
    Guard theGuard;
    inSample->SetThePitch(inPitch);
    if (MixerChannel* theChannel = ChannelOf(inSample))
        SetStep(*theChannel, inPitch);
    return kSPIMOK;
}

extern "C" long SPIMSetSampleVolume(SESample* inSample, float inVolume)
{
    Guard theGuard;
    inSample->SetTheVolume(inVolume);
    if (MixerChannel* theChannel = ChannelOf(inSample))
        SetGains(*theChannel, inVolume, inSample->GetPan());
    return kSPIMOK;
}

// Sets the master volume (0..1); kSPIMBadParameter when out of range
extern "C" long SPIMSetVolume(float inVolume)
{
    if (inVolume < 0.0f || inVolume > 1.0f)
        return kSPIMBadParameter;
    gVolume = inVolume;
    return kSPIMOK;
}

// Stops every channel; returns the last channel's result (kSPIMChannelBusy when that one was empty)
extern "C" long SPIMStopAllChannels()
{
    long theResult = kSPIMOK;
    unsigned long theCount = SPIMGetChannelCount();
    for (unsigned long i = 0; i < theCount; i++)
        theResult = SPIMStopChannel(i);
    return theResult;
}

// Stops the channel and detaches its sample; kSPIMChannelBusy when the channel is empty
extern "C" long SPIMStopChannel(unsigned long inChannel)
{
    Guard theGuard;
    if (inChannel >= gChannels.size())
        return kSPIMBadParameter;
    MixerChannel& theChannel = gChannels[inChannel];
    if (!theChannel.mSample)
        return kSPIMChannelBusy;
    theChannel.mSample->SetMixerChannel(-1);
    theChannel.mSample = NULL;
    theChannel.mData = NULL;
    theChannel.mPlaying = false;
    return kSPIMOK;
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
