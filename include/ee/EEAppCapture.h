// New (port), TEST ONLY: the app and world lane's records of the simulation capture (tools/sim_capture.py; RUST_PLAN
// R6 "app and world"). Reads the objects; writes nothing.
#ifndef EE_EEAPPCAPTURE_H
#define EE_EEAPPCAPTURE_H

class EECameraControl;
class EEPlayer;
class EEEMStorm;
class GETransformation;
class UWideString;

// CAMU: the camera's whole state (the poses, the view, the target, the followed units' ids, the move's numbers, the
// mode and flags, every key of the path) and the viewport's clipping distances, at each Update's start (inPhase 0,
// with its elapsed time) and end (1)
void EEAppCaptureCamera(const EECameraControl& inCamera, unsigned int inPhase, float inElapsedTime);

// MAPS: after a sight change of EEMap (AddSight inKind 0, RemoveSight 1): its arguments and an FNV-1a of what it
// changed on the tiles near the point (each tile's visibility words and the mesh tile's reference count)
void EEAppCaptureSight(unsigned int inKind, EEPlayer* inPlayer, float inX, float inY, float inRadius, bool inShared);

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
class EEAppMapScope {
public:
    // the tile rectangle x in [inLeft, inRight], y in [inBottom, inTop] of inMap (the hash reads one tile and point
    // more on each side); inLeft > inRight: the whole map
    EEAppMapScope(EEMap* inMap, unsigned int inKind, long inA, long inB, long inC, long inD, long inE, long inF,
                  long inLeft, long inRight, long inBottom, long inTop);
    ~EEAppMapScope();

private:
    EEMap* mMap;
    bool mOuter;
    unsigned int mKind;
    long mArgs[6];
    long mRect[4];
};

// MAPK: ComputeChecksum's window (the cursor before and after) and its sum; MAPP: GetTerrainPitch (inKind 0) or
// GetTerrainRoll (1) with the location and the result
void EEAppCaptureChecksum(unsigned long inCursorBefore, long inResult, unsigned long inCursorAfter);
void EEAppCaptureSlope(unsigned int inKind, const GETransformation& inLocation, float inResult);

// MMPX: the mini map's terrain pixels a RedrawPixels wrote (the rectangle and the packed pixels, read back)
void EEAppCaptureMiniMapPixels(long inLeft, long inRight, long inBottom, long inTop, const unsigned short* inBits,
                               unsigned long inPitch);
// MMSN: the mini map's cells (each cell's sight count and unit layer pixel, its building and unit dots in list
// order), from EE-SIM's tick hook every game minute
void EEAppCaptureMiniMapSnapshot();

// STRM: a storm's state after its creation (inPhase 0) and each Step (1, with its time)
void EEAppCaptureStorm(const EEEMStorm& inStorm, unsigned int inPhase, unsigned long inTime);

// EE-SIM --app-sweep (the capture's app profile): at five game minutes the cheat codes (CHET: each chat line, its
// parse and, when applied, CHEF: every player's state after it), at ten one storm of each kind
void EEAppCaptureCheatSweep();
void EEAppCaptureStormSweep();
// TEST ONLY: WSTA, EEWorld::RecordStatistics at the app profile's end (EEAppCaptureWorld.cpp)
void EEAppCaptureStatisticsSweep();
// CHLS: a line ListCheatCodes adds to the chat log
void EEAppCaptureCheatList(const UWideString& inLine);

#endif
