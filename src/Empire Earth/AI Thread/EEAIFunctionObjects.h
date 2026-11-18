// \eex\Empire Earth\AI Thread\EEAIFunctionObjects.h (RECONSTRUCTION, PARTIAL). The AI's
// small function objects whose members live in EEAIFunctionObjects.cpp (0x29fcc..0x2bb72) and that no other header
// declares yet (the original header records __FILE__ for an assert first emitted at 0x2ddfb: found through the
// \eex\Empire Earth\AI Thread\ include path: lives in src/Empire Earth/AI Thread/, included as
// <EEAIFunctionObjects.h> so __FILE__ is the drive-less /I form). Class names from RTTI; slot and
// member names PROVISIONAL.
// Requires ee/EECPUMGatherUnitsScreenBase.h.
#ifndef EE_EEAIFUNCTIONOBJECTS_H
#define EE_EEAIFUNCTIONOBJECTS_H

#include "ee/EEAIFOUnitScreeners.h"

class EEComplexUnit;
class EEComputerPlayer;
class EEUnitClass;

// RTTI EECPUGatherHeroesCount > EECPUMGatherUnitsScreenBase (vtable 0x43a678: ??_G 0x7895f, 0x2b503, 0x2b528,
// 0x2b549): the heroes (EEUnitClass +0x1d0 == 0x1b). Its ctor is not in this unit.
class EECPUGatherHeroesCount : public EECPUMGatherUnitsScreenBase {
public:
    // inline (EEComputerPlayer::GetHeroCount 0x2e248 builds it in its frame: the base's stores, the vptr, then
    // mPlayer)
    EECPUGatherHeroesCount(EEComputerPlayer* inPlayer, bool inByClass, bool inRequests)
        : EECPUMGatherUnitsScreenBase(inByClass, inRequests, false)
    {
        this->mPlayer = inPlayer;
    }
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);   // +0x04 0x2b503
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x2b528
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x2b549

    EEComputerPlayer* mPlayer;              // +0x10
};

// RTTI EEAIFOFavoriteUnitScreener > EEAIFOUnitSelectionScreener (vtable 0x43a660; ctor 0x2ddfb in EEComputerPlayer.cpp,
// ret 0x1c: the init list before the vptr, mEpoch clamped to 0..14 in the body). Needs ee/EECPUnitSelector.h.
// Member names PROVISIONAL.
class EEAIFOFavoriteUnitScreener : public EEAIFOUnitSelectionScreener {
public:
    EEAIFOFavoriteUnitScreener(EEComputerPlayer* inPlayer, bool inAllowAir, bool inAllowLand, bool inAllowSea, bool inAllowSpace, long inEpoch,
        long inEpochSpan);
    virtual bool Screen(EEUnitClass* inClass);   // +0x04 0x2a7d2 (EEAIFunctionObjects.cpp)

    EEComputerPlayer* mPlayer;              // +0x04
    long mEpoch;                            // +0x08
    long mEpochSpan;                        // +0x0c
    bool mAllowAir;                         // +0x10
    bool mAllowSea;                         // +0x11
    bool mAllowLand;                        // +0x12
    bool mAllowSpace;                       // +0x13
};

// The screeners' base slot +0x04 (COMDAT 0x2de43, first emitted by EEComputerPlayer.cpp): not to be called. The class
// is declared in ee/EECPUnitSelector.h; the ESVERIFY line is the original's (0x1d8 = 472).
inline bool EEAIFOUnitSelectionScreener::Screen(EEUnitClass* inClass)
{
    ESVERIFY_AT(false, 472);
    return false;
}

#endif
