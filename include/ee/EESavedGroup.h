// EESavedGroup.h (RECONSTRUCTION; file name inferred). A hot-key group (RTTI EESavedGroup > EEGroup >
// UReferenceCounter): EEPlayer holds 20 of them (mGroups, typed EEGroup*); it tells the user interface (thread
// message 0x435 with mIndex) when its first unit changes. port (the groups lane, round 1): a Rust object as every
// group (ee/EEPlayer.h; ee_units group_obj.rs). Requires ee/EEPlayer.h.
#ifndef EE_EESAVEDGROUP_H
#define EE_EESAVEDGROUP_H

#include "ee/EEPlayer.h"

class EESavedGroup : public EEGroup {
public:
    static EESavedGroup* New(EEPlayer* inPlayer, long inIndex)
        { return static_cast<EESavedGroup*>(static_cast<EEGroup*>(ee_units_group_new(0, inPlayer, inIndex))); }
    // takes the selection's units (sorted first) and becomes its saved group
    void Assign(EESelectedGroup* inGroup) { ee_units_group_with(this, 3, inGroup); }
};

#endif
