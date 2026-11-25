// ee/EETechTreeTechID.h (RECONSTRUCTION): the tech tree ids, moved out of ee/EEPlayer.h (which includes it) so that
// units without ee/EEPlayer.h can name them. Each comment gives the evidence for its name; (provisional name) marks an
// inferred one.
#ifndef EE_EETECHTREETECHID_H
#define EE_EETECHTREETECHID_H

#include <stdint.h>
#include <vector>

// The tech ids and the prerequisite bits: Rust's enums (ee_sim tech_tree_ids.rs), emitted back as plain C++ enums with
// their evidence comments.
#include "rust/sim/tech_tree_ids.h"

// The epochs (epochs.yaml; the dbtechtree file's epoch blocks): their count is set when gTechTree loads (15, the
// original's, until then). An epoch's tech id is its position 0..count-1; node ids are 1000 and up (kTechFirstNodeID),
// and a node's epoch is the block it was loaded in (EETechTreeEpochOf), not id / 1000 - 1 as in the original, so an
// epoch inserted in the middle keeps every other tech's id.
// The two tables are Rust's (ee_sim tech tech_ids.rs; declared here so the many includers need not take
// rust/sim/tech.h).
extern "C" int32_t ee_tech_epoch_count(void);
extern "C" int32_t ee_tech_epoch_of(int32_t id);
extern "C" int32_t ee_tech_epoch_title_id(int32_t epoch);
inline long EEEpochCount() { return ee_tech_epoch_count(); }
inline bool EEIsEpochID(long inID) { return inID >= 0 && inID < EEEpochCount(); }
const long kTechFirstNodeID = 1000;
// the epoch of a tech id (an epoch's own id; a node's block); -1: none
inline long EETechTreeEpochOf(long inID) { return ee_tech_epoch_of((int32_t)inID); }
// the epoch's title string ("Bronze Age"; epochs.yaml mTitleID); for EEEpochCount(), the game options' "Random Epoch"
inline long EEEpochTitleID(long inEpoch) { return ee_tech_epoch_title_id((int32_t)inEpoch); }
// one element per epoch (sized when it is made: after the database is loaded)
template <class T> class EEEpochArray : public std::vector<T> {
public:
    EEEpochArray() : std::vector<T>(EEEpochCount()) {}
};

#endif // EE_EETECHTREETECHID_H
