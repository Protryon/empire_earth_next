// New (port), TEST ONLY: PLCE, the computer player's placement and resource centres' capture record
// (tools/sim_capture.py; AI lane B, round 2). Every entry point of EECPPlacement, EECPResourceCenter and their free
// functions (the epoch classes, EECPCountOwn, EECPNextPlaceSearch) opens a scope; when the outermost one on the thread
// returns, one record holds the method id, the player id, the inputs (classes, tiles, sizes, flags, distances as f32
// bits) and the outputs (the result, out tiles, counts; for the state-changing calls the placement's town centre tile
// and site lists, or the centre's units and main unit, after the call). Calls made from inside another
// record nothing. Off (no EE_SIM_CAPTURE), a scope is a thread-local count and a cached test.
#ifndef EE_EECPPLACECAPTURE_H
#define EE_EECPPLACECAPTURE_H

#include <vector>

class EEComputerPlayer;
class EECPPlacement;
class EECPResourceCenter;
class EEUnit;
class EEComplexUnit;

enum {
    // EECPPlacement
    kPlceHasUnitNear = 1,
    kPlceHasAntiAirNear,
    kPlceHasTownCenterNear,
    kPlceHasGateNear,
    kPlceHasTowerNear,
    kPlceHasWallNear,
    kPlceCollectSiteTiles,
    kPlceHasEnemyNear,
    kPlceFindAirportSite,
    kPlceFindAntiAirSiteNear,
    kPlceFindAntiAirSiteAround,
    kPlceFindAntiAirSite,
    kPlceFindCenterSite,
    kPlceFindGranarySite,
    kPlceFindProductionSite,
    kPlceFindShoreSite,
    kPlceFindSpaceSite,
    kPlceFindTowerSite,
    kPlceFindWonderSite,
    kPlceFindSite,
    kPlceIsNearAirportEnd,
    kPlceHasBuilderFor,
    kPlceSetTownCenter,
    kPlceUnitAdded,
    kPlceReset,
    kPlceRestore,
    kPlceLoad,
    kPlceSave,
    kPlceCanBuildAt,
    // the free functions
    kPlceIsSiteClass = 40,
    kPlceAntiAirClass,
    kPlceFishingBoatClass,
    kPlceGateClass,
    kPlceTowerClass,
    kPlceWallClass,
    kPlceCountOwn,
    kPlceNextPlaceSearch,
    // EECPResourceCenter
    kPlceCenterAddUnit = 60,
    kPlceCenterContains,
    kPlceCenterAdoptDropSite,
    kPlceCenterFindBuildSite,
    kPlceCenterMinEffectRadius,
    kPlceCenterMoraleFill,
    kPlceCenterFindResourceUnit,
    kPlceCenterIsInAreaEffect,
    kPlceCenterLoad,
    kPlceCenterRestore,
    kPlceCenterIsWithinRadius,
    kPlceCenterSave,
    kPlceCenterSetUnit,
    kPlceCenterSetActive,
    kPlceCenterReleaseUnit,
    kPlceCenterGetCenter,
};

// CPEV (TEST ONLY; AI lane B, round 2): EEComputerPlayer's own calls, the same record shape with its own thread-local
// depth (a CPEV call's placement calls still record PLCE as before). Its scope appends the computer player's state when
// it ends: an FNV-1a of the record EeCpFields, the favourite history, the units to kill, the danger zones, every
// resource centre with its active flag (all set by then), the town centre tile.
enum {
    kCpevWorkOutPopulation = 1,
    kCpevRegroup,
    kCpevUpdateGroups,
    kCpevGetObjectsToGroup,
    kCpevGetObjectRegion,
    kCpevChooseFavoriteAir,
    kCpevChooseFavoriteLand,
    kCpevChooseFavoriteSea,
    kCpevChooseFavoriteSpace,
    kCpevEnterEpoch,
    kCpevChooseCivilization,
    kCpevPendingEpoch,
    kCpevAvoidsEnemies,
    kCpevCanProduce,
    kCpevRestoreManagers,
    kCpevDamageTaken,
    kCpevUnitPlaced,
    kCpevUnitTaken,
    kCpevUnitCompleted,
    kCpevUnitGoalDone,
    kCpevSetActive,
    kCpevEpochReached,
    kCpevUnitStranded,
    kCpevPayImprovement,
    kCpevUnitsSwapped,
};

// CPEV records the calls of the thread that runs the computer players' Update (noted at its first call)
void EECPPlaceCaptureThread();

class EECPPlaceScope {
public:
    // inPlayer: the computer player (NULL: none)
    EECPPlaceScope(unsigned int inMethod, const EEComputerPlayer* inPlayer);
    // a CPEV record (inEvents: true)
    EECPPlaceScope(unsigned int inMethod, const EEComputerPlayer* inPlayer, bool inEvents);
    ~EECPPlaceScope();
    bool On() const { return this->mOn; }
    EECPPlaceScope& I(long inValue)
    {
        if (this->mOn)
            this->mWords.push_back((unsigned int)inValue);
        return *this;
    }
    EECPPlaceScope& F(float inValue);
    EECPPlaceScope& U(const EEUnit* inUnit);            // its id (0xfffffffe: none)
    EECPPlaceScope& CU(const EEComplexUnit* inUnit);
    // the placement's state: the town centre tile, the site lists (sizes and an FNV-1a of their tiles), the classes seen
    EECPPlaceScope& State(const EECPPlacement* inPlacement);
    // a centre's state: its units' ids, its main unit's id
    EECPPlaceScope& State(const EECPResourceCenter* inCenter);
    // every resource centre of the player: their number and an FNV-1a of their states
    EECPPlaceScope& Centers(const EEComputerPlayer* inPlayer);
    // CPEV: the computer player's state (appended by the destructor)
    EECPPlaceScope& Player(const EEComputerPlayer* inPlayer);

private:
    void Begin(unsigned int inMethod, const EEComputerPlayer* inPlayer);
    const EEComputerPlayer* mSelf;
    bool mEvents;
    bool mOuter;
    bool mOn;
    unsigned int mMethod;
    unsigned int mPlayer;
    std::vector<unsigned int> mWords;
};

#endif
