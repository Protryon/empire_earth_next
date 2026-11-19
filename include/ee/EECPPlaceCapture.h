// New (port), TEST ONLY: PLCE, the computer player's placement and resource centres' capture record
// (tools/sim_capture.py; AI lane B, round 2). Every entry point of EECPPlacement, EECPResourceCenter and their free
// functions (the epoch classes, EECPCountOwn, EECPNextPlaceSearch) opens a scope; when the outermost one on the thread
// returns, one record holds the method id, the player id, the inputs (classes, tiles, sizes, flags, distances as f32
// bits) and the outputs (the result, out tiles, counts; for the state-changing calls the placement's town centre tile
// and site lists, or the centre's units and main unit, after the call). Calls made from inside another
// record nothing. Off (no EE_SIM_CAPTURE), a scope is a thread-local count. Whole in Rust (ee_sim::cp
// place_capture.rs) since the game's leaves round 2.
#ifndef EE_EECPPLACECAPTURE_H
#define EE_EECPPLACECAPTURE_H

#include <stdint.h>

#include "rust/sim/cp.h"

class EEPlayer;
class EECPPlacement;
class RTSUnit;

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

// The scope of one call: forwarders over ee_sim::cp place_capture.rs (the depths, the thread, the record). The
// player is the computer player (NULL: none); a unit an EEUnit or EEComplexUnit (its id; NULL: 0xfffffffe); the
// state reads the placement and the player through the world layout. Requires ee/EEComputerPlayer.h and ee/EEUnit.h
// where it is used (the casts to the bases).
class EECPPlaceScope {
public:
    // a PLCE record, or a CPEV record (inEvents: true)
    template <class P> EECPPlaceScope(unsigned int inMethod, const P* inPlayer, bool inEvents = false)
        : mEvents(inEvents), mScope(ee_cp_scope_begin(EECPPlaceScope::Player(inPlayer), inMethod, inEvents)) {}
    ~EECPPlaceScope() { ee_cp_scope_end(this->mScope, this->mEvents); }
    EECPPlaceScope(const EECPPlaceScope&) = delete;
    EECPPlaceScope& operator=(const EECPPlaceScope&) = delete;
    EECPPlaceScope& I(long inValue)
    {
        ee_cp_scope_word(this->mScope, (uint32_t)inValue);
        return *this;
    }
    template <class T> EECPPlaceScope& U(const T* inUnit)
    {
        ee_cp_scope_unit(this->mScope, (uint64_t)(uintptr_t)static_cast<const RTSUnit*>(inUnit));
        return *this;
    }
    template <class T> EECPPlaceScope& CU(const T* inUnit) { return this->U(inUnit); }
    // the placement's state: the town centre tile, the site lists (sizes and an FNV-1a of their tiles), the classes seen
    EECPPlaceScope& State(const EECPPlacement* inPlacement)
    {
        ee_cp_scope_state(this->mScope, (uint64_t)(uintptr_t)inPlacement);
        return *this;
    }
    // every resource centre of the player: their number and an FNV-1a of their states
    template <class P> EECPPlaceScope& Centers(const P* inPlayer)
    {
        ee_cp_scope_centers(this->mScope, EECPPlaceScope::Player(inPlayer));
        return *this;
    }

private:
    template <class P> static uint64_t Player(const P* inPlayer)
        { return (uint64_t)(uintptr_t)static_cast<const EEPlayer*>(inPlayer); }
    bool mEvents;
    EeCpPlaceScope* mScope;   // NULL: it does not record
};

// CPEV records the calls of the thread that runs the computer players' Update (noted at its first call)
inline void EECPPlaceCaptureThread() { ee_cp_capture_thread(); }

#endif
