// New (port), TEST ONLY: the app and world lane's records of the simulation capture (tools/sim_capture.py; RUST_PLAN
// R6 "app and world"). Reads the objects; writes nothing.
#ifndef EE_EEAPPCAPTURE_H
#define EE_EEAPPCAPTURE_H

class EECameraControl;
class EEPlayer;
class EEEMStorm;
class GETransformation;
class UWideString;

// (CAMU, the camera's whole state at each Update's start and end: ee_game gamecpp camera_host.rs)

// MAPS: after a sight change of EEMap (AddSight inKind 0, RemoveSight 1): its arguments and an FNV-1a of what it
// changed on the tiles near the point (each tile's visibility words and the mesh tile's reference count)
// The world's hooks the game calls are pointers that EEAppCaptureWorld.cpp sets before main, so no game unit names a
// symbol of that TEST ONLY unit (PORT_LEARNINGS "Cycle cuts")
inline void (*EEAppCaptureSight)(unsigned int inKind, EEPlayer* inPlayer, float inX, float inY, float inRadius,
                                 bool inShared) = nullptr;

// MAPC: one of EEMap's terrain edits (the kinds below) entered from outside the map; when the outermost one returns,
// its arguments and an FNV-1a of the map around its rectangle (the tiles' terrain types, the mesh's tiles, points
// and normals). Nested edits (UpdateCliffs' SetCliffColorSets) record nothing.
enum {
    kAppMapUpdateCliffs = 1,
    kAppMapRemoveFlatCliffs,
    kAppMapPaintTerrain,
    kAppMapPaintTerrainRect,
    kAppMapSetCliffColorSets,
    kAppMapUpdateWater,
    kAppMapCreateMap,
    kAppMapSetCliffTexture,
};
class EEMap;
// (the scope's hooks: Enter answers whether it is the outermost; Leave writes the record when outermost)
inline bool (*EEAppMapScopeEnter)() = nullptr;
inline void (*EEAppMapScopeLeave)(EEMap* inMap, bool inOuter, unsigned int inKind, const long* inArgs,
                                  const long* inRect) = nullptr;
class EEAppMapScope {
public:
    // the tile rectangle x in [inLeft, inRight], y in [inBottom, inTop] of inMap (the hash reads one tile and point
    // more on each side); inLeft > inRight: the whole map
    EEAppMapScope(EEMap* inMap, unsigned int inKind, long inA, long inB, long inC, long inD, long inE, long inF,
                  long inLeft, long inRight, long inBottom, long inTop)
        : mMap(inMap), mOuter(EEAppMapScopeEnter()), mKind(inKind), mArgs{inA, inB, inC, inD, inE, inF},
          mRect{inLeft, inRight, inBottom, inTop}
    {
    }
    ~EEAppMapScope() { EEAppMapScopeLeave(this->mMap, this->mOuter, this->mKind, this->mArgs, this->mRect); }

private:
    EEMap* mMap;
    bool mOuter;
    unsigned int mKind;
    long mArgs[6];
    long mRect[4];
};

// (MAPK, MAPP, MMPX, MMSN, CHLS: ee_game app world_capture.rs, called directly)

// (STRM, a storm's state after its creation and each step: ee_game gamecpp weather.rs)

// EE-SIM --app-sweep (the capture's app profile): at five game minutes the cheat codes (CHET: each chat line, its
// parse and, when applied, CHEF: every player's state after it), at ten one storm of each kind
inline void (*EEAppCaptureCheatSweep)() = nullptr;
inline void (*EEAppCaptureStormSweep)() = nullptr;
// TEST ONLY: WSTA, EEWorld::RecordStatistics at the app profile's end (EEAppCaptureWorld.cpp)
inline void (*EEAppCaptureStatisticsSweep)() = nullptr;

#endif
