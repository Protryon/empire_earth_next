// New (port): the calamity classes' starts, stops and updates in Rust (ee_tech calamity_start.rs, calamity_update.rs;
// docs/RUST_PLAN.md R5b.2). A class's Start, Stop and Update fill an EeCalamityParams and call EECalamityRunStart /
// RunStop / RunUpdate; the Rust run asks the world through EECalamityUpdateHost, answered in EECalamityUpdate.cpp:
// the calls every class shares and each class's own world steps (its effects, units, orders), keyed by its enum.
// Requires lle/LowLevelEngine.h.
#ifndef EE_EECALAMITYUPDATE_H
#define EE_EECALAMITYUPDATE_H

#include <vector>

#include "rust/sim/tech.h"
#include "lle/U2DPoint.h"

class EECalamity;
class EECalamityClass;
class EEComplexUnit;
class EESoundObject;
class EEUnit;

struct EECalamityUpdateHost {
    EECalamityClass* mClass;
    EECalamity* mCalamity;
    EESoundObject* mSounds[2];              // the class's sound and secondary sound (UpdateSound's)
    std::vector<EEUnit*> mUnits;            // the last EEGetUnitsNear's
    void* mPending;                         // a class step's object for its next step (the cloak's area effect)
    std::vector<U2DPoint<long> > mTiles;    // the steps' tile searches (ee_game gamecpp calamity.rs)
    std::vector<EEUnit*> mScratch;          // the steps' own unit searches
};

// runs the class's update in Rust; the calamity's members in and out (EeCalamityState) around it and every host call
// (the class's run over this host: gamecpp calamity_io.rs class_run, game core lane round 11)

#endif
