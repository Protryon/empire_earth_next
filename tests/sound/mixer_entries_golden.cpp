// Golden driver for the mixer plug-in's entry table (LLE lane round 7: the SPIM entry points that only reach the Rust
// mixer moved to rust/crates/ee_mixer ffi.rs under their own names). Looks every name up in gPIMPluginMixer, as
// SEMixer does, and calls the entries that need no sample on the null backend (gPlatSimMode), in random orders:
// activation, channels, their states, volumes, the tuning entry points, deactivation.
//
//   python -I build/lle7/drvmix.py orig|new;   build/lle7/mix_<orig|new>.exe <sequences> > golden.txt
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/PIMModule.h"
#include "platform/PlatSim.h"

extern const PIMPlugin gPIMPluginMixer;
extern "C" void SPIMSetMasterVolume(float inVolume);

static uint64_t gRand;
static uint32_t R(uint32_t n)
{
    gRand = gRand * 6364136223846793005ull + 1442695040888963407ull;
    return n ? (uint32_t)(gRand >> 33) % n : 0;
}

static void* Entry(const char* inName)
{
    for (const PIMEntry* e = gPIMPluginMixer.mEntries; e->mName != NULL; e++)
        if (strcmp(e->mName, inName) == 0)
            return e->mProc;
    return NULL;
}

template <class F> static F E(const char* inName)
{
    return reinterpret_cast<F>(Entry(inName));
}

static uint32_t Bits(float inValue)
{
    uint32_t b;
    memcpy(&b, &inValue, 4);
    return b;
}

int main(int argc, char** argv)
{
    int theSequences = argc > 1 ? atoi(argv[1]) : 200;
    gPlatSimMode = 1;
    gRand = 5;
    printf("plugin %s\n", gPIMPluginMixer.mFile);
    int n = 0;
    for (const PIMEntry* e = gPIMPluginMixer.mEntries; e->mName != NULL; e++, n++)
        printf("entry %s %d\n", e->mName, e->mProc != NULL);
    printf("entries %d\n", n);
    printf("name %s type %lx valid %d\n", (const char*)*(*E<const UString* (*)()>("PIMGetName"))(),
           (*E<unsigned long (*)()>("PIMGetType"))(), (int)(*E<bool (*)()>("PIMIsValid"))());
    for (int s = 0; s < theSequences; s++) {
        printf("seq %d\n", s);
        for (int op = 0, ops = 5 + R(40); op < ops; op++) {
            unsigned long c = R(40);
            float v = (float)((int)R(9) - 2) / 4.0f;
            switch (R(18)) {
            case 0: printf("activate %ld\n", (*E<long (*)()>("SPIMActivateMixer"))()); break;
            case 1: printf("deactivate %ld\n", (*E<long (*)()>("SPIMDeactivateMixer"))()); break;
            case 2: printf("create %lu %ld\n", c, (*E<long (*)(unsigned long, void*)>("SPIMCreateChannels"))(c, NULL)); break;
            case 3: printf("destroy %ld\n", (*E<long (*)()>("SPIMDestroyChannels"))()); break;
            case 4: printf("count %lu max %lu\n", (*E<unsigned long (*)()>("SPIMGetChannelCount"))(),
                           (*E<unsigned long (*)()>("SPIMGetMaximumChannelCount"))()); break;
            case 5: printf("rate %lu\n", (*E<unsigned long (*)()>("SPIMGetOutputRate"))()); break;
            case 6: printf("busy %lu %ld\n", c, (*E<long (*)(unsigned long)>("SPIMIsChannelBusyPlaying"))(c)); break;
            case 7: printf("play %lu %ld\n", c, (*E<long (*)(unsigned long)>("SPIMPlayChannel"))(c)); break;
            case 8: printf("stop %lu %ld\n", c, (*E<long (*)(unsigned long)>("SPIMStopChannel"))(c)); break;
            case 9: printf("stopall %ld\n", (*E<long (*)()>("SPIMStopAllChannels"))()); break;
            case 10: printf("setvol %08x %ld\n", Bits(v), (*E<long (*)(float)>("SPIMSetVolume"))(v)); break;
            case 11: printf("vol %08x\n", Bits((*E<float (*)()>("SPIMGetVolume"))())); break;
            case 12: printf("cpu %08x ds %d timer %ld\n", Bits((*E<float (*)()>("SPIMGetCPUUsage"))()),
                            (*E<void* (*)()>("SPIMGetDirectSoundInterface"))() != NULL,
                            (*E<long (*)()>("SPIMGetHighestTimerDelay"))()); break;
            case 13: printf("ahead %ld msg %d latency %ld\n", (*E<long (*)()>("SPIMMixAhead"))(),
                            (int)(*E<bool (*)()>("SPIMProcessMessage"))(),
                            (*E<long (*)(unsigned long)>("SPIMSetLatency"))(c)); break;
            case 14: printf("chsample %lu %ld\n", c,
                            (*E<long (*)(unsigned long, void*)>("SPIMSetChannelSample"))(c, NULL)); break;
            case 15: {
                HWND theWindow = NULL;
                (*E<void (*)(HWND*)>("SPIMInitializeMixer"))(&theWindow);
                printf("init\n");
                break;
            }
            case 16:
                (*E<void (*)(void (*)(void*, float*, unsigned long), void*)>("SPIMSetMusicSource"))(NULL, NULL);
                (*E<void (*)(void (*)(void*, float*, unsigned long), void*)>("SPIMSetMovieSource"))(NULL, NULL);
                printf("sources\n");
                break;
            default:
                SPIMSetMasterVolume(v);
                printf("master %08x vol %08x\n", Bits(v), Bits((*E<float (*)()>("SPIMGetVolume"))()));
                break;
            }
        }
        printf("end %ld\n", (*E<long (*)()>("SPIMDeactivateMixer"))());
    }
    return 0;
}
