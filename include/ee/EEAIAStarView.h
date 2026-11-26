// New (port): the world as the AI's tile A* searches see it (docs/RUST_PLAN.md R5b.1, PORT_LEARNINGS "AI Thread
// cluster, round 4"). Every read the tile goals and their modifiers make of the live world goes through one view, one
// query per fact of one tile, with a plain answer and no side effects; the searches' capture (ASRN) notes the answers
// of a view that is not quiet. The view is Rust (ee_sim::astar world_view.rs: the queries over the world views and
// the continent manager's record); this class is its record, the map and whether it is quiet (a modifier's view).
#ifndef EE_EEAIASTARVIEW_H
#define EE_EEAIASTARVIEW_H

#include "rust/sim/astar.h"   // EeAStarViewCallbacks, EeAStarWorldView

class EEMap;

class EEAIAStarView {
public:
    EEAIAStarView() { this->mView.map = 0; this->mView.quiet = false; }
    // a modifier's view: the given map, its queries not noted when inQuiet
    EEAIAStarView(EEMap* inMap, bool inQuiet) { this->mView.map = inMap; this->mView.quiet = inQuiet; }
    void SetMap(EEMap* inMap) { this->mView.map = inMap; }

private:
    EeAStarWorldView mView;
};

#endif
