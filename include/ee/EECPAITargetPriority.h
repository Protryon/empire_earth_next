// Actual8B target-priority predicate from original8fd7; nominal name provisional.
// Moved unchanged from TargetScreenBase for its genuine Inferno user. Requires
// complete EEComplexUnit/EECPASTargetUnit and EETileDistanceLess declarations.
#ifndef EE_EECPAITARGETPRIORITY_H
#define EE_EECPAITARGETPRIORITY_H

// Built without /Og: its out-of-line COMDAT takes the optimisation state at this definition (r37_load: a pragma
// around the include flips 0x408fd7, one around the sort users or at the end of the file does not)
#pragma optimize("g", off)
struct EECPAITargetPriority {
    EECPAITargetPriority(const EETileCoordinate& inCenter) { this->mCenter = inCenter; }
    EECPAITargetPriority(const EECPAITargetPriority& inOther) : mCenter(inOther.mCenter) {}
    ~EECPAITargetPriority() {}
    bool operator()(EECPASTargetUnit* inFirst, EECPASTargetUnit* inSecond) const
    {
        bool theResult = false;
        if (inFirst->GetNumNearbyTargets() < inSecond->GetNumNearbyTargets()) {
        } else if (inFirst->GetNumNearbyTargets() == inSecond->GetNumNearbyTargets()) {
            long theFirst = inFirst->mUnit->GetTile().DistanceSquared(this->mCenter);
            long theSecond = inSecond->mUnit->GetTile().DistanceSquared(this->mCenter);
            theResult = theFirst > theSecond;
        } else
            theResult = true;
        return theResult;
    }
    EETileCoordinate mCenter;
};
#pragma optimize("g", on)

#endif
