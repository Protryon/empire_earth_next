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

// ------------------------------------------------------------------------- round 11, lane C: the run's host

int64_t EECore11OpE(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))

// a class's run (gamecpp calamity_io.rs class_run): its host made and deleted around it (EEContagionManager.cpp's
// EECore11OpC5 default); p the class, a0 the calamity, a1 and a2 the sounds
int64_t EECore11OpC6(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::C11CalHostNew: return P64((new EECalamityUpdateHost{(EECalamityClass*)p, (EECalamity*)a[0], {(EESoundObject*)a[1], (EESoundObject*)a[2]}, {}, NULL, {}, {}}));
    case O::C11CalHostDelete: delete (EECalamityUpdateHost*)p;
        break;
    default: return EECore11OpE(inOp, p, a, f);
    }
    return 0;
}
