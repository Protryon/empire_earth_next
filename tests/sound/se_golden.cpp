// Golden driver for the Sound Engine's objects (LLE lane round 6): SESoundManager with its pooled instance nodes,
// SESample, SEMixer (the registry, UseMixer, SEInitializeMixers/SEShutdown), SEConductor and SEScore. Random
// sequences of public calls over fakes at the engine's own seams, the same in both builds:
//   - the Thread System at its C ABI (ee_ts_*): sections, events, threads (a thread never starts: the scavenger's
//     procedure is run by the driver with a scripted wait result list);
//   - the mixer plug-in: a fake module walk (the ee_soundcpp_* plug-in calls) whose SPIM entry points print each
//     call and answer from the driver's state (busy channels, playback times, failures);
//   - dmusic (Dm*): fake loader, segments and performance; the loader resolves through the conductor's resolver,
//     which reads real files from the music folder;
//   - FSPath::FindFile (a fake path: names starting with 'm' are not found).
// Each line is "<op> <args> :<the fake calls> = <result>". The relinked driver (-DSE_NEW, the new units and Rust)
// must print the same; rust/crates/ee_audio/src/sound/golden_objects.rs replays the lines over the Rust objects.
//
//   python -I build/lle6/drv.py orig|new        (compile with the game's flags, link with the units' objects)
//   build/lle6/seg_<orig|new>.exe <music folder> <sequences> [first seed] > golden.txt
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <map>

#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/SEMixer.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/SEScore.h"
#include <dmusic.h>
#include "rust/audio/sound.h"
#include "rust/base/ts.h"

long SEInitialize(const FSFileSpec& inDirectory, GEWindow* inWindow);
#ifdef SE_NEW
typedef void SEChannelDescription;   // the entry point's argument (no longer declared by the engine)
#endif

// ---- output
static std::string gLine;
static void T(const char* inFormat, ...)
{
    char theBuffer[512];
    va_list theArgs;
    va_start(theArgs, inFormat);
    vsnprintf(theBuffer, sizeof(theBuffer), inFormat, theArgs);
    va_end(theArgs);
    gLine += ' ';
    gLine += theBuffer;
}
static std::string gOut;
static void Flush()
{
    fwrite(gOut.data(), 1, gOut.size(), stdout);
    gOut.clear();
}
static void Begin(const char* inFormat, ...)
{
    char theBuffer[512];
    va_list theArgs;
    va_start(theArgs, inFormat);
    vsnprintf(theBuffer, sizeof(theBuffer), inFormat, theArgs);
    va_end(theArgs);
    gLine = theBuffer;
    gLine += " :";
}
static void End(const char* inFormat, ...)
{
    char theBuffer[512];
    va_list theArgs;
    va_start(theArgs, inFormat);
    vsnprintf(theBuffer, sizeof(theBuffer), inFormat, theArgs);
    va_end(theArgs);
    gOut += gLine;
    gOut += " = ";
    gOut += theBuffer;
    gOut += '\n';
    static bool sEach = getenv("SEG_FLUSH") != NULL;
    if (sEach || gOut.size() > 1 << 20)
        Flush();
}
static unsigned Bits(float inValue)
{
    unsigned theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

// ---- the random source (xorshift64*)
static uint64_t gRandom;
static uint32_t R(uint32_t inBound)
{
    gRandom ^= gRandom >> 12;
    gRandom ^= gRandom << 25;
    gRandom ^= gRandom >> 27;
    return (uint32_t)((gRandom * 2685821657736338717ull) >> 32) % inBound;
}
static float RF()
{
    static const float kValues[] = {0.0f, 1.0f, 0.5f, 0.25f, -0.5f, -1.0f, 0.005f, 0.01f, 0.75f, 2.0f, 0.3f, 1.5f};
    return kValues[R(12)];
}

// ---- the Thread System's C ABI
struct FakeSection { int mID; int mDepth; };
struct FakeEvent { int mID; bool mManual; bool mSet; };
struct FakeThread { int mID; unsigned long (*mFunction)(void*); void* mParameter; };
static int gNextTS;
static std::vector<long> gWaitScript;   // ee_ts_event_wait_any's next results (empty: 0, the exit event)
static FakeThread* gLastThread;

extern "C" {
EeTsSection* ee_ts_section_new()
{
    FakeSection* theSection = new FakeSection{++gNextTS, 0};
    T("cs+%d", theSection->mID);
    return reinterpret_cast<EeTsSection*>(theSection);
}
void ee_ts_section_free(EeTsSection* inSection)
{
    if (!inSection)
        return;
    FakeSection* theSection = reinterpret_cast<FakeSection*>(inSection);
    T("cs-%d/%d", theSection->mID, theSection->mDepth);
    delete theSection;
}
void ee_ts_section_enter(const EeTsSection* inSection)
{
    FakeSection* theSection = (FakeSection*)(inSection);
    theSection->mDepth++;
    T("E%d", theSection->mID);
}
void ee_ts_section_leave(const EeTsSection* inSection)
{
    FakeSection* theSection = (FakeSection*)(inSection);
    theSection->mDepth--;
    T("L%d", theSection->mID);
}
bool ee_ts_section_try_enter(const EeTsSection* inSection) { return true; }
long ee_ts_event_new(EeTsEvent** outEvent, bool inManual)
{
    FakeEvent* theEvent = new FakeEvent{++gNextTS, inManual, false};
    T("ev+%d%s", theEvent->mID, inManual ? "m" : "");
    *outEvent = reinterpret_cast<EeTsEvent*>(theEvent);
    return 0;
}
void ee_ts_event_free(EeTsEvent* inEvent)
{
    if (!inEvent)
        return;
    T("ev-%d", reinterpret_cast<FakeEvent*>(inEvent)->mID);
    delete reinterpret_cast<FakeEvent*>(inEvent);
}
long ee_ts_event_set(const EeTsEvent* inEvent)
{
    T("set%d", ((FakeEvent*)inEvent)->mID);
    ((FakeEvent*)inEvent)->mSet = true;
    return 0;
}
long ee_ts_event_clear(const EeTsEvent* inEvent)
{
    T("clr%d", ((FakeEvent*)inEvent)->mID);
    ((FakeEvent*)inEvent)->mSet = false;
    return 0;
}
long ee_ts_event_wait(const EeTsEvent* inEvent, uint32_t inTimeout)
{
    T("wait%d/%u", ((FakeEvent*)inEvent)->mID, inTimeout);
    return 0;
}
bool ee_ts_event_is_manual(const EeTsEvent* inEvent) { return ((FakeEvent*)inEvent)->mManual; }
void* ee_ts_event_handle(const EeTsEvent* inEvent) { return (void*)inEvent; }
int32_t ee_ts_event_wait_any(const EeTsEvent* const* inEvents, uint32_t inCount, uint32_t inTimeout)
{
    long theResult = 0;
    if (!gWaitScript.empty()) {
        theResult = gWaitScript.front();
        gWaitScript.erase(gWaitScript.begin());
    }
    T("any%d/%u/%u>%ld", ((FakeEvent*)inEvents[0])->mID, inCount, inTimeout, theResult);
    return (int32_t)theResult;
}
long ee_ts_thread_new(EeTsThread** outThread, EeTsThreadFunction inFunction, void* inParameter, uint32_t inStack,
                      bool inSuspended)
{
    FakeThread* theThread = new FakeThread{++gNextTS, (unsigned long (*)(void*))inFunction, inParameter};
    T("th+%d/%u/%d", theThread->mID, inStack, inSuspended);
    gLastThread = theThread;
    *outThread = reinterpret_cast<EeTsThread*>(theThread);
    return 0;
}
long ee_ts_thread_free(EeTsThread* inThread)
{
    if (!inThread)
        return 0;
    T("th-%d", reinterpret_cast<FakeThread*>(inThread)->mID);
    delete reinterpret_cast<FakeThread*>(inThread);
    return 0;
}
void ee_ts_thread_set_name(const EeTsThread* inThread, const char* inName)
{
    T("name%d=%s", ((FakeThread*)inThread)->mID, inName);
}
}

// ---- the engine's other leaves
UString gStartupVolume("C:");
UString gStartupDirectory("\\");
static bool gFailShutdownRegistration;
long URegisterShutdown(void (*inFunction)(bool))
{
    T("shutdown-registered");
    return gFailShutdownRegistration ? kErrorFailed : kErrorOK;
}
void ESVerify(const char* inFile, unsigned long inLine, const char* inText)
{
    T("VERIFY %lu", inLine);
}
static unsigned long gPrecision = 0;
void UMSetFloatingPointPrecision(unsigned long inPrecision)
{
    T("fpp=%lx", inPrecision);
    gPrecision = inPrecision;
}
unsigned long UMGetFloatingPointPrecision() { return gPrecision; }
unsigned long UMGetFloatingPointRoundingMode() { return 0; }
void UMSetFloatingPointRoundingMode(unsigned long) {}
extern "C" void PlatSleep(uint32_t inMilliseconds) { T("sleep%u", inMilliseconds); }
extern "C" uint32_t PlatGetLastError() { return 0; }

// ---- the sound path: a fake FindFile
FSPath::FSPath() {}
FSPath::~FSPath() { T("path-deleted"); }
static int gFinds;
long FSPath::FindFile(const UString& inName, FSFileSpec*& outFile)
{
    const char* theName = inName;
    T("find:%s", theName);
    gFinds++;
    if (theName[0] == 'm')
        return kErrorNotFound;
    outFile = new FSFileSpec(UString("C:\\snd\\") + inName);
    return kErrorOK;
}
class Path : public FSPath {
public:
    unsigned long Count() const { return this->mReferenceCount; }
};

// ---- the fake mixer plug-in
static std::map<const SESample*, int> gSamples;     // live samples the plug-in saw -> label
static int gNextSample;
static unsigned long gChannels;                     // SPIMGetChannelCount
static unsigned long gMaxChannels = 32;
static long gBusy[64];                              // SPIMIsChannelBusyPlaying per channel
static int gFailNext;                               // the next sample setter / channel call fails with this
static int gPlayTime;                               // added to every sample's playback times
static float gVolume = 1.0f;
static unsigned long gOutputRate = 22050;
static void (*gMusicSource)(void*, float*, unsigned long);
static void* gMusicContext;
static void (*gMovieSource)(void*, float*, unsigned long);
static bool gActive;
static int gLoadFail;                               // 1: SPIMLoadWaveData fails for names starting with 'b'

static int Label(const SESample* inSample)
{
    std::map<const SESample*, int>::iterator it = gSamples.find(inSample);
    return it == gSamples.end() ? -1 : it->second;
}
static long Fail()
{
    long theResult = gFailNext;
    gFailNext = 0;
    return theResult;
}

static long FakeActivateMixer() { T("activate"); gActive = true; return Fail(); }
static long FakeDeactivateMixer() { T("deactivate"); gActive = false; return Fail(); }
static long FakeCreateChannels(unsigned long inCount, SEChannelDescription* outChannels)
{
    // never fails: a manager with no channel plays from an empty instance list (the original's undefined
    // behaviour, a panic in Rust), which the game never reaches
    T("create%lu", inCount);
    gChannels = inCount;
    return 0;
}
static long FakeDestroyChannels() { T("destroy"); gChannels = 0; return 0; }
static void FakeDeleteWaveData(SESample* inSample)
{
    T("delwave:S%d", Label(inSample));
    gSamples.erase(inSample);
}
static long FakeDuplicateSample(const SESample* inSample, SESample** outSample)
{
#ifdef SE_NEW
    SESample* theSample = SESample::Duplicate(*inSample);
#else
    SESample* theSample = new SESample(*inSample);
#endif
    theSample->SetData(inSample->GetData());
    gSamples[theSample] = ++gNextSample;
    T("dup:S%d>S%d", Label(inSample), gNextSample);
    *outSample = theSample;
    return 0;
}
static unsigned long FakeGetChannelCount() { T("chcount"); return gChannels; }
static float FakeGetCPUUsage() { T("cpu"); return 0.25f; }
static IDirectSound* FakeGetDirectSoundInterface() { T("ds"); return NULL; }
static unsigned long FakeGetHighestTimerDelay() { T("delay"); return 7; }
static unsigned long FakeGetMaximumChannelCount() { T("maxch"); return gMaxChannels; }
static long FakeGetSamplePlayBackTime(SESample* inSample, long* outTotal, long* outRemaining)
{
    int theLabel = Label(inSample);
    *outTotal = 1000 + theLabel * 10 + gPlayTime;
    *outRemaining = theLabel * 7 + gPlayTime;
    T("ptime:S%d", theLabel);
    return 0;
}
static unsigned long FakeGetSampleOffset(SESample* inSample) { T("offset:S%d", Label(inSample)); return 3; }
static float FakeGetVolume() { T("getvol"); return gVolume; }
static void FakeInitializeMixer(GEWindow* inWindow) { T("init%d", inWindow != NULL); }
static long FakeIsChannelBusyPlaying(unsigned long inChannel)
{
    T("busy%lu", inChannel);
    return inChannel < 64 ? gBusy[inChannel] : 0;
}
static long FakeLoadWaveData(FSFileSpec* inSpec, SESample* ioSample)
{
    const char* theName = inSpec->GetFile();
    gSamples[ioSample] = ++gNextSample;
    T("loadwave:%s>S%d", theName, gNextSample);
    if (gLoadFail && theName[0] == 'b')
        return 9;
    size_t theLength = strlen(theName);
    ioSample->SetFilename(inSpec->GetFile());
    ioSample->SetData((void*)(uintptr_t)(0x1000 + gNextSample));
    ioSample->SetLengthInBytes(theLength * 1000);
    ioSample->SetSampleBits(8);
    ioSample->SetLengthInSamples(theLength * 500 + (theName[1] == '2' ? 0 : 17));
    ioSample->SetSampleRate(theName[1] == '3' ? 0 : 22050 + theLength);
    return 0;
}
static long FakeMixAhead() { T("mixahead"); return 4; }
static long FakePlayChannel(unsigned long inChannel) { T("playch%lu", inChannel); return Fail(); }
static bool FakeProcessMessage(unsigned long inMessage, void* inP1, void* inP2) { T("msg%lu", inMessage); return true; }
static long FakeSetChannelSample(unsigned long inChannel, SESample* inSample)
{
    T("setch%lu:S%d", inChannel, Label(inSample));
    long theResult = Fail();
    if (theResult == 0)
        inSample->SetMixerChannel(inChannel);
    return theResult;
}
static long FakeSetLatency(unsigned long inMilliseconds) { T("latency%lu", inMilliseconds); return 0; }
static void FakeSetSampleLooping(SESample* inSample, bool inLooping)
{
    T("loop:S%d=%d", Label(inSample), inLooping);
    inSample->SetTheIsLooping(inLooping);
}
static long FakeSetSampleOffset(SESample* inSample, unsigned long inOffset)
{
    T("setoff:S%d=%lu", Label(inSample), inOffset);
    return 0;
}
static long FakeSetSamplePan(SESample* inSample, float inPan)
{
    T("pan:S%d=%08x", Label(inSample), Bits(inPan));
    long theResult = Fail();
    inSample->SetThePan(inPan);
    return theResult;
}
static long FakeSetSamplePitch(SESample* inSample, float inPitch)
{
    T("pitch:S%d=%08x", Label(inSample), Bits(inPitch));
    long theResult = Fail();
    inSample->SetThePitch(inPitch);
    return theResult;
}
static long FakeSetSampleVolume(SESample* inSample, float inVolume)
{
    T("vol:S%d=%08x", Label(inSample), Bits(inVolume));
    long theResult = Fail();
    inSample->SetTheVolume(inVolume);
    return theResult;
}
static long FakeSetVolume(float inVolume) { T("mastervol=%08x", Bits(inVolume)); gVolume = inVolume; return 0; }
static long FakeStopAllChannels() { T("stopall"); return 0; }
static long FakeStopChannel(unsigned long inChannel) { T("stopch%lu", inChannel); return 0; }
static unsigned long FakeGetOutputRate() { T("rate"); return gOutputRate; }
static void FakeSetMusicSource(void (*inSource)(void*, float*, unsigned long), void* inContext)
{
    T("musicsrc%d", inSource != NULL);
    gMusicSource = inSource;
    gMusicContext = inContext;
}
static void FakeSetMovieSource(void (*inSource)(void*, float*, unsigned long), void* inContext)
{
    T("moviesrc%d", inSource != NULL);
    gMovieSource = inSource;
}
static UString gName("FakeMixer");
static const UString& FakeGetName() { T("getname"); return gName; }
static unsigned long FakeGetType() { return ' snd'; }
static bool gModuleValid[4];
static int gModuleIndex;
static bool FakeIsValid() { T("isvalid"); return gModuleValid[gModuleIndex]; }

#define E(n, f) { n, (void*)&f }
static const struct {
    const char* mName;
    void* mProc;
} sEntries[] = {
    E("SPIMActivateMixer", FakeActivateMixer), E("SPIMCreateChannels", FakeCreateChannels),
    E("SPIMDeactivateMixer", FakeDeactivateMixer), E("SPIMDeleteWaveData", FakeDeleteWaveData),
    E("SPIMDestroyChannels", FakeDestroyChannels), E("SPIMDuplicateSample", FakeDuplicateSample),
    E("SPIMGetChannelCount", FakeGetChannelCount), E("SPIMGetCPUUsage", FakeGetCPUUsage),
    E("SPIMGetDirectSoundInterface", FakeGetDirectSoundInterface),
    E("SPIMGetHighestTimerDelay", FakeGetHighestTimerDelay),
    E("SPIMGetMaximumChannelCount", FakeGetMaximumChannelCount),
    E("SPIMGetSamplePlayBackTime", FakeGetSamplePlayBackTime), E("SPIMGetSampleOffset", FakeGetSampleOffset),
    E("SPIMGetVolume", FakeGetVolume), E("SPIMInitializeMixer", FakeInitializeMixer),
    E("SPIMIsChannelBusyPlaying", FakeIsChannelBusyPlaying), E("SPIMLoadWaveData", FakeLoadWaveData),
    E("SPIMMixAhead", FakeMixAhead), E("SPIMPlayChannel", FakePlayChannel),
    E("SPIMProcessMessage", FakeProcessMessage), E("SPIMSetChannelSample", FakeSetChannelSample),
    E("SPIMSetLatency", FakeSetLatency), E("SPIMSetSampleLooping", FakeSetSampleLooping),
    E("SPIMSetSampleOffset", FakeSetSampleOffset), E("SPIMSetSamplePan", FakeSetSamplePan),
    E("SPIMSetSamplePitch", FakeSetSamplePitch), E("SPIMSetSampleVolume", FakeSetSampleVolume),
    E("SPIMSetVolume", FakeSetVolume), E("SPIMStopAllChannels", FakeStopAllChannels),
    E("SPIMStopChannel", FakeStopChannel), E("SPIMGetOutputRate", FakeGetOutputRate),
    E("SPIMSetMusicSource", FakeSetMusicSource), E("SPIMSetMovieSource", FakeSetMovieSource),
};

// The plug-in walk the Rust mixers make (the mixer plug-in's ee_soundcpp_* in the game, Mixer.cpp) over a scripted
// module list; the trace is the one the PIMModule fake printed before the registry went (LLE round 8)
static int gModuleCount;
static unsigned long gNextModule;
static int32_t FakeModule(void* inStart, bool inFirst)
{
    if (inFirst)
        gNextModule = 0;
    T("module%lu/%d", gNextModule, gModuleCount);
    if ((int)gNextModule >= gModuleCount)
        return kErrorNotFound;
    gModuleIndex = (int)gNextModule++;
    static_cast<SEMixerStart*>(inStart)->mModule = (void*)sEntries;
    return kErrorOK;
}
const UString& SEMixerPlugInName() { return FakeGetName(); }
bool SEMixerPlugInIsValid() { return FakeIsValid(); }
extern "C" {
int32_t ee_soundcpp_first_module(void* inStart) { return FakeModule(inStart, true); }
int32_t ee_soundcpp_next_module(void* inStart) { return FakeModule(inStart, false); }
bool ee_soundcpp_module_is_valid(void* inModule) { return FakeIsValid(); }
void ee_soundcpp_module_unload(void* inModule) { T("unload"); }
void* ee_soundcpp_module_entry(void* inModule, const char* inName)
{
    for (const auto& theEntry : sEntries)
        if (strcmp(theEntry.mName, inName) == 0)
            return theEntry.mProc;
    T("noentry:%s", inName);
    return NULL;
}
}

// ---- dmusic
struct DmLoader { int mID; DmLoaderResolverCallback* mResolve; void* mContext; };
struct DmPerformance { int mID; };
struct DmSegment { int mID; uint32_t mRepeats; int mRefs; };
static int gNextDm;
static bool gFailLoader, gFailPerformance, gFailPlay;

extern "C" {
DmResult DmLoader_create(DmLoader** outLoader, DmLoaderOptions inOptions)
{
    T("dmloader%u%s", (unsigned)inOptions, gFailLoader ? "!" : "");
    if (gFailLoader)
        return DmResult_INVALID_ARGUMENT;
    *outLoader = new DmLoader{++gNextDm, NULL, NULL};
    return DmResult_SUCCESS;
}
void DmLoader_release(DmLoader* inLoader)
{
    T("dmloader-%d", inLoader ? inLoader->mID : 0);
    delete inLoader;
}
DmResult DmLoader_addResolver(DmLoader* inLoader, DmLoaderResolverCallback* inResolve, void* inContext)
{
    T("resolver");
    inLoader->mResolve = inResolve;
    inLoader->mContext = inContext;
    return DmResult_SUCCESS;
}
DmResult DmLoader_getSegment(DmLoader* inLoader, char const* inName, DmSegment** outSegment)
{
    size_t theLength = 99;
    void* theData = inLoader->mResolve(inLoader->mContext, inName, &theLength);
    std::string theText = theData ? std::string((const char*)theData, theLength) : "-";
    for (size_t i = 0; i < theText.size(); i++)
        if (theText[i] == ' ' || theText[i] == '\r' || theText[i] == '\n')
            theText[i] = '_';
    T("getseg:%s=%zu:%s", inName, theLength, theText.c_str());
    free(theData);
    if (!theData || theText[0] == 'F')
        return DmResult_NOT_FOUND;
    *outSegment = new DmSegment{++gNextDm, 0, 1};
    return DmResult_SUCCESS;
}
void DmSegment_release(DmSegment* inSegment)
{
    T("seg-%d", inSegment->mID);
    delete inSegment;
}
uint32_t DmSegment_getRepeats(DmSegment const* inSegment)
{
    T("getrep%d", inSegment->mID);
    return inSegment->mRepeats;
}
void DmSegment_setRepeats(DmSegment* inSegment, uint32_t inRepeats)
{
    T("setrep%d=%u", inSegment->mID, inRepeats);
    inSegment->mRepeats = inRepeats;
}
DmResult DmPerformance_create(DmPerformance** outPerformance, uint32_t inRate)
{
    T("dmperf%u%s", inRate, gFailPerformance ? "!" : "");
    if (gFailPerformance)
        return DmResult_INVALID_ARGUMENT;
    *outPerformance = new DmPerformance{++gNextDm};
    return DmResult_SUCCESS;
}
void DmPerformance_release(DmPerformance* inPerformance)
{
    T("dmperf-%d", inPerformance ? inPerformance->mID : 0);
    delete inPerformance;
}
DmResult DmPerformance_playSegment(DmPerformance* inPerformance, DmSegment* inSegment, DmTiming inTiming)
{
    T("playseg%d/%d%s", inSegment ? inSegment->mID : 0, (int)inTiming, gFailPlay ? "!" : "");
    return gFailPlay ? DmResult_INVALID_STATE : DmResult_SUCCESS;
}
DmResult DmPerformance_playTransition(DmPerformance* inPerformance, DmSegment* inSegment, DmEmbellishmentType inType,
                                      DmTiming inTiming)
{
    T("trans%d/%d/%d%s", inSegment ? inSegment->mID : 0, (int)inType, (int)inTiming, gFailPlay ? "!" : "");
    return gFailPlay ? DmResult_INVALID_STATE : DmResult_SUCCESS;
}
void DmPerformance_setVolume(DmPerformance* inPerformance, float inVolume) { T("dmvol=%08x", Bits(inVolume)); }
void DmPerformance_setMasterGrooveLevel(DmPerformance* inPerformance, int8_t inLevel) { T("groove=%d", (int)inLevel); }
DmResult DmPerformance_renderPcm(DmPerformance* inPerformance, void* outBuffer, size_t inCount, DmRenderOptions inOptions)
{
    T("render%zu/%d", inCount, (int)inOptions);
    for (size_t i = 0; i < inCount; i++)
        ((float*)outBuffer)[i] = 0.5f;
    return DmResult_SUCCESS;
}
}

// ---- the sequences
static std::string gMusicFolder;                 // ends with a separator
static SESoundManager* gManager;
static unsigned long gCats;
static void DeleteManager()
{
#ifdef SE_NEW
    SESoundManager::Destroy(gManager);
#else
    delete gManager;
#endif
}                      // the manager's category count
static SESMInstanceNode* gNodes[8];
static std::vector<long> gInstanceIDs;           // ids seen
static std::vector<long> gScoreIDs;
static Path* gPaths[2];

static unsigned long PickID()
{
    if (!gInstanceIDs.empty() && R(5) != 0)
        return (unsigned long)gInstanceIDs[R((uint32_t)gInstanceIDs.size())];
    static const long kOthers[] = {-1, 0, 1, 2, 5, 1000};
    return (unsigned long)kOthers[R(6)];
}
static unsigned long PickSample() { return R(10) == 0 ? (unsigned long)-1 : R(3) == 0 ? R(9) : R(3); }
static long PickScore()
{
    if (!gScoreIDs.empty() && R(4) != 0)
        return gScoreIDs[R((uint32_t)gScoreIDs.size())];
    return (long)R(6) - 1;
}
static int gPick;                                // PickMixer's index (-1: none)
static SEMixer* PickMixer()
{
    unsigned long theCount = SEMixer::GetCount();
    gPick = -1;
    if (theCount == 0 || R(8) == 0)
        return NULL;
    SEMixer* theMixer = NULL;
    gPick = (int)R(theCount);
    SEMixer::GetMixer(gPick, theMixer);
    return theMixer;
}
static void Seen(unsigned long inID)
{
    if (inID != (unsigned long)-1)
        gInstanceIDs.push_back((long)inID);
}

static const char* kSounds[] = {"s1.wav", "s2.wav", "s3.wav", "b4.wav", "s5.wav", "m6.wav", "s7.wav", "a8.wav"};
static const char* kScores[] = {"a.sgt", "b.sgt", "c.sgt", "f.sgt", "z.sgt", "a.sgt"};

static void DumpSamples()
{
    for (std::map<const SESample*, int>::iterator it = gSamples.begin(); it != gSamples.end(); ++it) {
        // printed in label order below
    }
    std::map<int, const SESample*> theOrder;
    for (std::map<const SESample*, int>::iterator it = gSamples.begin(); it != gSamples.end(); ++it)
        theOrder[it->second] = it->first;
    for (std::map<int, const SESample*>::iterator it = theOrder.begin(); it != theOrder.end(); ++it) {
        SESample* s = const_cast<SESample*>(it->second);
        const char* theFile = s->GetFilename();
        T("S%d[%s,%lx,%08x,%08x,%08x,%ld,%lu,%lu,%lu,%lu,%d]", it->first, theFile[0] ? theFile : "-",
          (unsigned long)(uintptr_t)s->GetData(), Bits(s->GetVolume()), Bits(s->GetPan()), Bits(s->GetPitch()),
          (long)s->GetMixerChannel(), s->GetLengthInBytes(), s->GetLengthInSamples(), s->GetSampleBits(),
          s->GetSampleRate(), s->GetIsLooping());
    }
}

static void ConductorOp(SEConductor& c)
{
    switch (R(26)) {
    case 0: case 1: case 2: {
        const char* theName = kScores[R(6)];
        long theID = -7;
        Begin("m%d cload %s", gPick, theName);
        long r = c.LoadScore(UWideString(theName), theID);
        if (theID != -7)
            gScoreIDs.push_back(theID);
        End("%ld %ld", r, theID);
        break;
    }
    case 3: case 4: { long id = PickScore(); Begin("m%d cplay %ld", gPick, id); End("%ld", c.Play(id)); break; }
    case 5: { long id = PickScore(); Begin("m%d cqueue %ld", gPick, id); End("%ld", c.Queue(id)); break; }
    case 6: Begin("m%d cstop", gPick); End("%ld", c.Stop()); break;
    case 7: case 8: {
        long id = PickScore();
        unsigned long cmd = R(7), flags = R(3);
        Begin("m%d ctrans %ld %lu %lu", gPick, id, cmd, flags);
        End("%ld", c.TransitionTo(id, cmd, flags));
        break;
    }
    case 9: { long id = PickScore(); Begin("m%d cunload %ld", gPick, id); End("%ld", c.UnloadScore(id)); break; }
    case 10: case 11: { unsigned long s = R(3); Begin("m%d cstate %lu", gPick, s); End("%ld", c.SetConductorState(s)); break; }
    case 12: {
        unsigned long n = R(5) == 0 ? 300 : R(120);
        bool imm = R(2);
        Begin("m%d cint %lu %d", gPick, n, imm);
        End("%ld", c.SetIntensity(n, imm));
        break;
    }
    case 13: { float v = RF(); Begin("m%d cvol %08x", gPick, Bits(v)); End("%ld", c.SetVolume(v)); break; }
    case 14: {
        static const char* kFolders[] = {"", "x\\", "@"};
        int k = R(3);
        std::string theFolder = k == 2 ? gMusicFolder : kFolders[k];
        Begin("m%d cfolder %d", gPick, k);
        long r = c.SetMusicFolder(UWideString(theFolder.c_str()));
        UWideString theNow = c.GetMusicFolder();
        const char* f = theNow;
        End("%ld %s", r, k == 2 ? "@" : (f[0] ? f : "-"));
        break;
    }
    case 15: { unsigned long r = R(3); Begin("m%d creverb %lu", gPick, r); End("%ld %lu", c.SetReverb(r), c.GetReverbState()); break; }
    case 16: Begin("m%d cstyle", gPick); End("%ld", c.SetReverbStyle(0.5f, 0.1f, 1.0f, 2.0f)); break;
    case 17: {
        unsigned long n = R(3) == 0 ? 1001 : R(20);
        Begin("m%d cgroups %lu", gPick, n);
        long r = c.SetChannelGroupCount(n);
        End("%ld %lu", r, c.GetChannelGroupCount());
        break;
    }
    case 18: case 19: {
        long id = PickScore();
        Begin("m%d cscore %ld", gPick, id);
        SEScore* s = c.GetScore(id);
        if (!s) {
            End("null");
            break;
        }
        long theID = c.GetScoreID(s);
        unsigned long rep = s->GetRepeatCount();
        unsigned long n = R(4) * 1000;
        s->SetRepeatCount(n);
        End("%ld %d %lu %lu %lu", theID, s->GetIsSecondaryScore(), rep, s->GetRepeatCount(), s->GetSecondaryScoreCount());
        break;
    }
    case 20: Begin("m%d cscoreid0", gPick); End("%ld", c.GetScoreID(NULL)); break;
    case 21: Begin("m%d cplaying", gPick); End("%ld", c.GetCurrentlyPlayingScore()); break;
    case 22: Begin("m%d cget", gPick); End("%lu %lu %08x %lu %lu", c.GetConductorState(), c.GetIntensity(), Bits(c.GetVolume()),
                                 c.GetReverbState(), c.GetChannelGroupCount()); break;
    case 23: {
        Begin("m%d crender", gPick);
        if (gMusicSource) {
            float theFrames[16];
            gMusicSource(gMusicContext, theFrames, 4);
            End("%08x", Bits(theFrames[0]));
        } else
            End("none");
        break;
    }
    case 24: { gFailPlay = R(2); gFailPerformance = R(4) == 0; gFailLoader = R(6) == 0; Begin("m%d cfail %d %d %d", gPick, gFailPlay, gFailPerformance, gFailLoader); End("-"); break; }
    case 25: { gOutputRate = R(3) == 0 ? 0 : 44100; Begin("m%d crate %lu", gPick, gOutputRate); End("-"); break; }
    }
}

static void ManagerOp()
{
    SESoundManager* m = gManager;
    switch (R(46)) {
    case 0: case 1: case 2: case 3: {
        const char* theName = kSounds[R(8)];
        unsigned long cat = R(gCats + 1), max = R(6) == 0 ? 0 : 1 + R(4);
        Begin("load %s %lu %lu", theName, cat, max);
        End("%ld", (long)m->Load(UString(theName), cat, max));
        break;
    }
    case 4: case 5: case 6: case 7: {
        unsigned long sid = PickSample();
        float v = RF(), p = RF(), pi = RF();
        bool loop = R(3) == 0;
        Begin("play %ld %08x %08x %08x %d", (long)sid, Bits(v), Bits(p), Bits(pi), loop);
        unsigned long id = m->Play(sid, v, p, pi, loop, NULL);
        Seen(id);
        End("%ld", (long)id);
        break;
    }
    case 8: case 9: {
        unsigned long sid = PickSample();
        float v = RF();
        bool loop = R(2) == 0;
        int k = R(8);
        Begin("playn %d %ld %08x %d", k, (long)sid, Bits(v), loop);
        SESMInstanceNode* n = m->PlayAndReturnReferencedNode(sid, v, 0.0f, 1.0f, loop, (char*)"dbg");
        if (n) {
            if (gNodes[k])
                gNodes[k]->Dereference();
            gNodes[k] = n;
            Seen(m->GetInstanceID(n));
        }
        End("%d %ld", n != NULL, n ? (long)m->GetInstanceID(n) : -9);
        break;
    }
    case 10: {
        int k = R(8);
        Begin("deref %d", k);
        if (gNodes[k]) {
            gNodes[k]->Dereference();
            gNodes[k] = NULL;
        }
        End("-");
        break;
    }
    case 11: {
        int k = R(8), j = R(8);
        Begin("refcopy %d %d", k, j);
        if (gNodes[k] && k != j) {
            gNodes[k]->Reference();
            if (gNodes[j])
                gNodes[j]->Dereference();
            gNodes[j] = gNodes[k];
        }
        End("-");
        break;
    }
    case 12: {
        unsigned long id = PickID();
        int k = R(8);
        Begin("getref %ld %d", (long)id, k);
        SESMInstanceNode* n = m->GetAndReferenceSoundInstance(id);
        if (n) {
            if (gNodes[k])
                gNodes[k]->Dereference();
            gNodes[k] = n;
        }
        End("%d", n != NULL);
        break;
    }
    case 13: case 14: { unsigned long id = PickID(); bool f = R(2); Begin("stop %ld %d", (long)id, f); End("%ld", m->Stop(id, f)); break; }
    case 15: { int k = R(8); bool f = R(2); Begin("stopn %d %d", k, f); End("%ld", m->Stop(gNodes[k], f)); break; }
    case 16: {
        unsigned long id = PickID();
        Begin("get %ld", (long)id);
        End("%d %d %08x %08x %08x %ld %lu %ld", m->GetIsLooping(id), m->GetIsPlaying(id), Bits(m->GetPan(id)),
            Bits(m->GetPitch(id)), Bits(m->GetVolume(id)), (long)m->GetSampleID(id), m->GetSampleRate(id),
            m->GetTimeLeftInMilli(id));
        break;
    }
    case 17: {
        int k = R(8);
        SESMInstanceNode* n = gNodes[k];
        Begin("getn %d", k);
        End("%ld %d %d %08x %08x %ld", (long)m->GetInstanceID(n), m->GetIsLooping(n), m->GetIsPlaying(n),
            Bits(m->GetPan(n)), Bits(m->GetPitch(n)), (long)m->GetSampleID(n));
        break;
    }
    case 18: { unsigned long s = PickSample(); Begin("total %ld", (long)s); End("%ld", m->GetTotalPlayTimeInMilli(s)); break; }
    case 19: {
        unsigned long id = PickID();
        float v = RF();
        int w = R(4);
        float p2 = RF(), p3 = RF();
        Begin("set%d %ld %08x %08x %08x", w, (long)id, Bits(v), Bits(p2), Bits(p3));
        long r = w == 0 ? m->SetVolume(id, v) : w == 1 ? m->SetPan(id, v) : w == 2 ? m->SetPitch(id, v)
                                                       : m->SetSoundParameters(id, v, p2, p3);
        End("%ld", r);
        break;
    }
    case 20: case 21: {
        int k = R(8);
        float v = RF();
        int w = R(4);
        float p2 = RF(), p3 = RF();
        Begin("setn%d %d %08x %08x %08x", w, k, Bits(v), Bits(p2), Bits(p3));
        SESMInstanceNode* n = gNodes[k];
        long r = w == 0 ? m->SetVolume(n, v) : w == 1 ? m->SetPan(n, v) : w == 2 ? m->SetPitch(n, v)
                                                      : m->SetSoundParameters(n, v, p2, p3);
        End("%ld", r);
        break;
    }
    case 22: {
        unsigned long s = PickSample(), id = PickID();
        Begin("oneshot %ld %ld", (long)s, (long)id);
        End("%ld", m->RegisterOneShotSound(s, id));
        break;
    }
    case 23: Begin("stopos"); End("%ld", m->StopAllOneShotSounds()); break;
    case 24: { unsigned long s = PickSample(); Begin("unload %ld", (long)s); End("%ld", m->Unload(s)); break; }
    case 25: { unsigned long n = 1 + R(9); Begin("maxch %lu", n); End("%ld %lu", m->SetMaxChannels(n), m->GetAvailableChannels()); break; }
    case 26: {
        unsigned long cat = R(6);
        static const float kPercent[] = {0.0f, 0.5f, 1.0f, 0.25f, 0.1f};
        float p = kPercent[R(5)];
        Begin("pct %lu %08x", cat, Bits(p));
        End("%lu", m->SetCategoryPercentage(cat, p));
        break;
    }
    case 27: { unsigned long cat = R(6), p = R(5); Begin("prio %lu %lu", cat, p); End("%ld", m->SetCategoryPriority(cat, p)); break; }
    case 28: {
        int k = R(6) == 0 ? 2 : (int)R(2);
        Begin("setpath %d", k);
        m->SetPath(k == 2 ? NULL : gPaths[k]);
        End("%lu %lu %d", gPaths[0]->Count(), gPaths[1]->Count(), m->GetPath() == (k == 2 ? NULL : gPaths[k]));
        break;
    }
    case 29: { float v = RF(); Begin("master %08x", Bits(v)); long r = m->SetMasterVolume(v); End("%ld %08x", r, Bits(m->GetMasterVolume())); break; }
    case 30: Begin("misc"); End("%ld %ld %lu %lu %lu %d", m->SetLatency(R(100)), m->MixAhead(), m->GetMaxChannels(),
                                 m->GetHighestTimerDelay(), m->GetAvailableChannels(), m->GetCriticalSection() != NULL); break;
    case 31: { unsigned long id = PickID(); Begin("rate %ld", (long)id); End("%lu", m->GetSampleRate(id)); break; }
    case 32: case 33: case 34: {
        // one scavenger run: k passes, then the exit event
        int k = R(3);
        bool mute = R(4) == 0;
        m->SetIsMuted(mute);
        gWaitScript.clear();
        for (int i = 0; i < k; i++)
            gWaitScript.push_back(R(6) == 0 ? (long)kTSWaitFailed : (long)kTSWaitTimeout);
        gWaitScript.push_back(0);
        std::string theScript;
        for (size_t i = 0; i < gWaitScript.size(); i++)
            theScript += gWaitScript[i] == 0 ? '0' : gWaitScript[i] == (long)kTSWaitTimeout ? 't' : 'f';
        Begin("scav %d %s", mute, theScript.c_str());
        unsigned long r = gLastThread->mFunction(gLastThread->mParameter);
        End("%lu %d", r, m->GetIsMuted());
        break;
    }
    case 35: case 36: {
        int n = R(3);
        std::string theTable;
        for (int i = 0; i < 64; i++) {
            gBusy[i] = R(n + 2) == 0 ? 0 : (long)R(3) + 4;
            theTable += (char)('0' + gBusy[i]);
        }
        Begin("busy %s", theTable.c_str());
        End("-");
        break;
    }
    case 37: { gPlayTime = (int)R(3) * 500 - 200; Begin("ptime %d", gPlayTime); End("-"); break; }
    case 38: case 39: { gFailNext = R(3) == 0 ? 0 : (int)R(12) + 1; Begin("failnext %d", gFailNext); End("-"); break; }
    case 40: Begin("dump"); DumpSamples(); End("%lu %d", m->GetAvailableChannels(), gFinds); break;
    case 41: { gLoadFail = R(2); Begin("loadfail %d", gLoadFail); End("-"); break; }
    case 42: { unsigned long id = PickID(); Begin("vol %ld", (long)id); End("%08x", Bits(m->GetVolume(id))); break; }
    case 43: { int k = R(8); Begin("idn %d", k); End("%ld %ld", (long)m->GetInstanceID(gNodes[k]), (long)m->GetSampleID(gNodes[k])); break; }
    case 44: { gMaxChannels = R(4) == 0 ? 4 : 32; Begin("maxmix %lu", gMaxChannels); End("-"); break; }
    case 45: { unsigned long id = PickID(); Begin("left %ld", (long)id); End("%ld", m->GetTimeLeftInMilli(id)); break; }
    }
}

static void MixerOp()
{
    switch (R(12)) {
    case 0: case 1: {
        // samples take the active mixer when they are made and use it when deleted: never none under a manager
        SEMixer* theMixer = PickMixer();
        if (gManager && !theMixer)
            break;
        Begin("use %d", gPick);
        long r = SEMixer::UseMixer(theMixer);
        End("%ld %d %d", r, SEMixer::GetActiveMixer() == theMixer, SEMixer::GetActiveMixer() != NULL);
        break;
    }
    case 2: {
        unsigned long k = R(4);
        SEMixer* theMixer = (SEMixer*)1;
        Begin("getmixer %lu", k);
        long r = SEMixer::GetMixer(k, theMixer);
        End("%ld %d %lu", r, theMixer == (SEMixer*)1 ? 2 : theMixer != NULL, SEMixer::GetCount());
        break;
    }
    case 3: {
        SEMixer* m = PickMixer();
        if (!m)
            break;
        Begin("mixer %d", gPick);
        float u = m->GetCPUUsage();
        IDirectSound* ds = m->GetDirectSoundInterface();
        const char* n = m->GetName();
        bool v = m->IsValid();
        bool p = m->ProcessMessage(5, NULL, NULL);
        unsigned long rate = m->GetOutputRate();
        End("%08x %d %s %d %d %lu", Bits(u), ds != NULL, n, v, p, rate);
        break;
    }
    case 4: {
        SEMixer* m = PickMixer();
        if (!m)
            break;
        Begin("moviesrc %d", gPick);
        m->SetMovieSource(NULL, NULL);
        End("-");
        break;
    }
    default: {
        SEMixer* m = PickMixer();
        if (m)
            ConductorOp(m->GetConductor());
        break;
    }
    }
}

static void Sequence(uint64_t inSeed)
{
    gRandom = inSeed * 0x9E3779B97F4A7C15ull + 1;
    for (int i = 0; i < 20; i++)
        R(2);
    gOut += "seq " + std::to_string(inSeed) + "\n";
    gModuleCount = R(4);
    for (int i = 0; i < 4; i++)
        gModuleValid[i] = R(4) != 0;
    gFailPlay = gFailPerformance = gFailLoader = false;
    gFailNext = 0;
    gOutputRate = 22050;
    gChannels = 4 + R(2) * 4;
    gLoadFail = 1;
    gPlayTime = 0;
    memset(gBusy, 0, sizeof(gBusy));
    gInstanceIDs.clear();
    gScoreIDs.clear();
    FSFileSpec theDirectory;
    Begin("init %lu %d %d%d%d%d", gChannels, gModuleCount, gModuleValid[0], gModuleValid[1], gModuleValid[2], gModuleValid[3]);
    long r = inSeed % 5 == 0 ? SEInitialize(theDirectory, (GEWindow*)&theDirectory)
                             : SEInitializeMixers(theDirectory, inSeed % 2 ? (GEWindow*)&theDirectory : NULL);
    End("%ld %lu", r, SEMixer::GetCount());
    if (SEMixer::GetCount() > 0 && R(3) != 0) {
        SEMixer* theMixer = NULL;
        SEMixer::GetMixer(0, theMixer);
        Begin("use0");
        End("%ld", SEMixer::UseMixer(theMixer));
    }
    if (R(4) != 0) {
        SEMixer* theMixer = NULL;
        if (SEMixer::GetCount() > 0)
            SEMixer::GetMixer(0, theMixer);
        if (theMixer) {
            Begin("cfolder0");
            End("%ld", theMixer->GetConductor().SetMusicFolder(UWideString(gMusicFolder.c_str())));
        }
    }
    int theOps = 20 + R(140);
    for (int i = 0; i < theOps; i++) {
        unsigned w = R(10);
        if (w < 6 && gManager)
            ManagerOp();
        else if (w < 8)
            MixerOp();
        else if (w == 8) {
            if (gManager) {
                if (R(12) == 0) {
                    Begin("delmgr");
                    DeleteManager();
                    gManager = NULL;
                    End("-");
                }
            } else {
                SEMixer* theMixer = SEMixer::GetActiveMixer();
                if (theMixer) {
                    unsigned long cats = 1 + R(4), chans = R(3) == 0 && gChannels ? 0 : 1 + R(8);
                    int k = R(6) == 0 ? 2 : (int)R(2);
                    Begin("newmgr %lu %lu %d", cats, chans, k);
                    gCats = cats;
#ifdef SE_NEW
                    gManager = SESoundManager::Create(k == 2 ? NULL : gPaths[k], theMixer, cats, chans);
#else
                    gManager = new SESoundManager(k == 2 ? NULL : gPaths[k], theMixer, cats, chans);
#endif
                    End("%lu %lu %lu", gManager->GetAvailableChannels(), gPaths[0]->Count(), gPaths[1]->Count());
                }
            }
        } else {
            if (R(8) == 0) {
                unsigned long n = 1 + R(6);
                Begin("pool %lu", n);
                SESoundManager::InitializeSoundsPool(n, 2, 1);
                End("-");
            }
        }
    }
    if (gManager) {
        Begin("delmgr");
        DeleteManager();
        gManager = NULL;
        End("-");
    }
    Begin("nodes");
    for (int i = 0; i < 8; i++)
        if (gNodes[i]) {
            gNodes[i]->Dereference();
            gNodes[i] = NULL;
        }
    End("-");
    Begin("shutdown");
#ifdef SE_NEW
    ee_sound_shutdown(false);   // SEShutdown (LLE lane round 7: registered by ee_sound_initialize)
#else
    SEShutdown(false);
#endif
    End("%lu %d", SEMixer::GetCount(), SEMixer::GetActiveMixer() != NULL);
    if (R(4) == 0) {
        Begin("cleanup");
        SESoundManager::CleanupMemoryPoolAndDestroyCriticalSections();
        SESoundManager::CleanupMemoryPoolAndDestroyCriticalSections();
        End("-");
    }
    Begin("samples");
    DumpSamples();
    End("%zu", gSamples.size());
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "se_golden <music folder with a separator> <sequences> [first seed]\n");
        return 2;
    }
    gMusicFolder = argv[1];
    int theCount = atoi(argv[2]);
    uint64_t theFirst = argc > 3 ? strtoull(argv[3], NULL, 10) : 0;
    gPaths[0] = new Path;
    gPaths[1] = new Path;
    for (int i = 0; i < theCount; i++)
        Sequence(theFirst + i);
    Flush();
    return 0;
}
