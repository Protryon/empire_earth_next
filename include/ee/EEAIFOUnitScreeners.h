// The unit class screeners the computer player's favourite types are chosen by (EEComputerPlayer::ChooseFavorite*Type,
// GetScreenedTypesByLine). Extracted from ee/EECPUnitSelector.h when stage 5 deleted the unit selector with the rest
// of the C++ combat tree (docs/AI_RESEARCH.md §8.90).
#ifndef EE_EEAIFOUNITSCREENERS_H
#define EE_EEAIFOUNITSCREENERS_H

#include <vector>

class EEComputerPlayer;
class EEUnitClass;
template <class T> class U2DPoint;

// 0x2492c2 (cdecl; EECPWallManager.cpp): the tiles around (inX, inY) at inRadius appended to outTiles (ee/EECPIntelligence.h
// declared it until stage 5)
void EEGetSquareRingTiles(long inX, long inY, long inRadius, std::vector<U2DPoint<long> >& outTiles);

// RTTI EEAIFOUnitSelectionScreener (vtable slot +0x04 only; base of the screeners).
class EEAIFOUnitSelectionScreener {
public:
    virtual ~EEAIFOUnitSelectionScreener() {}
    virtual bool Screen(EEUnitClass* inClass);   // +0x04 (inline 0x2de43: ESVERIFY(false), in EEAIFunctionObjects.h)
};

// RTTI EEAIFOCurrentlyBuildableUnitScreener > EEAIFOUnitSelectionScreener (vtable 0x43a690: ??_G 0x783ca, +0x04
// 0x2ae09).
class EEAIFOCurrentlyBuildableUnitScreener : public EEAIFOUnitSelectionScreener {
public:
    EEAIFOCurrentlyBuildableUnitScreener(EEComputerPlayer* inPlayer) : mPlayer(inPlayer) {}
    virtual bool Screen(EEUnitClass* inClass);   // +0x04 0x2ae09

    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;              // +0x04
};

// RTTI EECPUSFavoriteUnitsScreen (vtable 0x43a4c0: ??_G 0x2a978); its four tests (ret 4, one per unit kind)
// (provisional names)
class EECPUSFavoriteUnitsScreen {
public:
    EECPUSFavoriteUnitsScreen() : mAllowAir(true), mAllowSea(true), mAllowLand(true), mAllowSpace(true) {}
    virtual ~EECPUSFavoriteUnitsScreen() {}   // +0x00 (??_G 0x2a978)
    bool IsFavoriteAir(EEUnitClass* inClass);
    bool IsFavoriteSea(EEUnitClass* inClass);
    bool IsFavoriteLand(EEUnitClass* inClass);
    bool IsFavoriteSpace(EEUnitClass* inClass);

    bool mAllowAir;                         // +0x04
    bool mAllowSea;                         // +0x05
    bool mAllowLand;                        // +0x06
    bool mAllowSpace;                       // +0x07
};

#endif
