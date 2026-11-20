// New (port): the calamity classes' Rust starts, stops and updates over their run's host (ee/EECalamityUpdate.h; ee_tech
// calamity_start.rs, calamity_update.rs; docs/RUST_PLAN.md R5b.2). The host's answers (the calls every class shares
// and each class's own world steps, what the twenty EECC* subclasses' units held, EECCEarthquake.cpp ..
// EECCVolcano.cpp 0xaebb7..0xb5b7d) are Rust too (ee_game gamecpp calamity.rs, over EEGameOps.cpp's cases; game core
// lane, round 7): this unit keeps the run's C++ objects.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"
#include "lle/GE3DPoint.h"

#include "ee/EEUnitKind.h"
#include "ee/EEEffect.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityUpdate.h"
#include "rust/game/gamecpp.h"

namespace {

// the host of one Rust run over the class and calamity (its sounds: the ones the class's run passes); the calamity's
// members in and out (EeCalamityState) around it
struct EECalamityRun {
    EECalamityUpdateHost mHost;
    EeCalamityHostFfi mFfi;
    EeCalamityState mState;

    EECalamityRun(EECalamityClass* inClass, EECalamity* inCalamity, EESoundObject* inSound, EESoundObject* inSecondarySound)
    {
        this->mHost.mClass = inClass;
        this->mHost.mCalamity = inCalamity;
        this->mHost.mSounds[0] = inSound;
        this->mHost.mSounds[1] = inSecondarySound;
        this->mHost.mPending = NULL;
        this->mFfi.ctx = &this->mHost;
        this->mFfi.call = &ee_gamecpp_calamity_call;
        this->mState = ee_gamecpp_calamity_state(inCalamity);
    }
    ~EECalamityRun() { ee_gamecpp_calamity_set_state(&this->mState, this->mHost.mCalamity); }
};

}   // namespace

bool EECalamityRunStart(EECalamityClass* inClass, EECalamity* inCalamity, const EeCalamityParams& inParams,
                        EESoundObject* inSound, EESoundObject* inSecondarySound)
{
    EECalamityRun theRun(inClass, inCalamity, inSound, inSecondarySound);
    return ee_calamity_start(&inParams, &theRun.mState, &theRun.mFfi);
}

void EECalamityRunStop(EECalamityClass* inClass, EECalamity* inCalamity, const EeCalamityParams& inParams,
                       EESoundObject* inSound, EESoundObject* inSecondarySound)
{
    EECalamityRun theRun(inClass, inCalamity, inSound, inSecondarySound);
    ee_calamity_stop(&inParams, &theRun.mState, &theRun.mFfi);
}

long EECalamityRunUpdate(EECalamityClass* inClass, EECalamity* inCalamity, float inElapsedTime, const EeCalamityParams& inParams,
                         EESoundObject* inSound, EESoundObject* inSecondarySound)
{
    EECalamityRun theRun(inClass, inCalamity, inSound, inSecondarySound);
    return ee_calamity_update(&inParams, &theRun.mState, inElapsedTime, &theRun.mFfi);
}
