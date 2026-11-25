// C:\EEX\Empire Earth\RTS Map\EEMiniMap.h (RECONSTRUCTION; file name inferred). The mini map (RTTI
// EEMiniMap; cluster 0x11e266..0x1211c5). Only what the hot keys use.
#ifndef EE_EEMINIMAP_H
#define EE_EEMINIMAP_H

#include <list>
#include <vector>
#include "lle/UList.h"
#include "rust/base/math.h"
#include "rust/game/app.h"
#include "rust/game/gamecpp.h"
#include "rust/game/rts.h"

class GEScreen;
class RTSPlayer;
class GESurface;
class GE2DPoint;
class GETexture;
class TSCriticalSection;

// C:\EEX\RTS Engine\Map\RTSMiniMap.h (file name inferred). RTTI RTSMiniMap (vtable 0x441dc4); RTSMiniMap.cpp: ctor
// 0x1240b3 (its asserts name mTerrainTexture, mWorldTexture, mTerrainCriticalSection, mUnitCriticalSection,
// mBattleFlags). sizeof 0x15c. Other names are provisional.
// a cell of the 0x40 x 0x40 battle-flag grid (RTSMiniMap::Ping queues them; a 2-byte trivially copied struct:
// list::_Insert copies it as one word). Names are provisional.
struct RTSMiniMapCell {
    unsigned char mX;
    unsigned char mY;
};

class RTSMiniMap {
public:
    // 0x1240b3 (arguments inferred: the screen makes the surfaces)
    RTSMiniMap(RTSPlayer* inPlayer, GEScreen* inScreen) { ee_rts_minimap_construct(this, inPlayer, inScreen); }
    // +0x00 (??_G 0x124418)
    virtual ~RTSMiniMap() { ee_rts_minimap_destruct(this); }
    virtual void RedrawAll() = 0;           // +0x04
    // 0x124497: flags the cell under a world point (provisional name)
    void Ping(const GE2DPoint& inPoint) { ee_rts_minimap_ping(this, reinterpret_cast<const EeGe2DPoint*>(&inPoint)); }

    // the view trapezoid's corners (EEMiniMap::Refresh 0x51fc62 passes &mCorner* to GETransformation::Apply)
    GE2DPoint mCorner0;                     // +0x04 bottom left
    GE2DPoint mCorner1;                     // +0x0c bottom right
    GE2DPoint mCorner2;                     // +0x14 top left
    GE2DPoint mCorner3;                     // +0x1c top right
    GETransformation mMapToPixel;           // +0x024
    GETransformation mPixelToMap;           // +0x064
    GETransformation mMapToCell;            // +0x0a4
    GETransformation mCellToMap;            // +0x0e4
    RTSPlayer* mViewingPlayer;              // +0x124 the viewing player (EEMiniMap's ctor passes gGameState's player)
    GETexture* mTerrainTexture;             // +0x128
    GETexture* mWorldTexture;               // +0x12c
    GESurface* mUnitSurface;                // +0x130 (GEScreen::CreateSurface's out-parameter, then Clear)
    unsigned char* mBattleFlags;            // +0x134 (new[]; element type inferred)
    TSCriticalSection* mTerrainCriticalSection;   // +0x138
    TSCriticalSection* mUnitCriticalSection;      // +0x13c
    unsigned short* mPixels;                // +0x140 (untouched by the ctor; EEMiniMap: mUnitSurface's locked pixels)
    long mPixelsPerRow;                     // +0x144 (untouched by the ctor)
    std::list<RTSMiniMapCell> mBattleCells;  // +0x148 (Ping's cells: the node is 0xc, the value a word)
    long mBattleFlagsWidth;                 // +0x154 (= 0x40: the battle grid's row length)
    long mBattleFlagsHeight;                // +0x158 (= 0x40)
};

// RTTI EEMiniMap > RTSMiniMap (vtable 0x441d44; ctor 0x11e3d4, sizeof 0x12188).
class EEDoppelganger;
class EEUnit;
class EEMap;
class EETile;
class GEColor;
class GEViewport;

class EEPlayer;
struct EeAppMiniMap;   // rust/ee_app.h
class EEMiniMap : public RTSMiniMap {
public:
// A doppelganger's dot in one mini-map cell (EEMiniMap::mPlayerUnits's lists: UListNode 0x10). The
// compare (inline: UList::Find's out-of-line COMDAT 0x1207ff expands it) checks the player and the colour only.
// Names are provisional.
struct MMPlayerUnit {
    bool operator==(const MMPlayerUnit& inOther) const { return this->mPlayer == inOther.mPlayer && this->mColor == inOther.mColor; }

    unsigned short mPlayer;                 // +0x00 (EEPlayer::mPlayerID)
    unsigned short mColor;                  // +0x02 (GEPackColor(2, ...))
    long mFlag;                             // +0x04 (1: a building class that hides the units under it)
};

// A unit's dot in one mini-map cell (EEMiniMap::mMapUnits's lists: UListNode 0x18). The compare is
// out of line (COMDAT 0x1206c2: the player, the colour and the tile; not mFlag). Names are provisional.
struct MMMapUnit {
    bool operator==(const MMMapUnit& inOther) const;

    unsigned short mPlayer;                 // +0x00
    unsigned short mColor;                  // +0x02
    long mFlag;                             // +0x04
    long mX;                                // +0x08 the unit's tile
    long mY;                                // +0x0c
};

    EEMiniMap(GEScreen* inScreen, EEMap* inMap, EEPlayer* inPlayer);   // 0x11e3d4 (arguments inferred)
    // Destroys the mini map (the base class frees its surfaces)
    // +0x00 (??_G 0x11e4da)
    virtual ~EEMiniMap() { ee_app_minimap_free(this->mRust); }
    // 0x11e7b1 (the editor's trigger screen after the view changes; provisional name)
    void RefreshView() { if (this->mL12174) this->Refresh(); }
    // Recomputes every terrain pixel of the mini map and flags the terrain texture changed
    // +0x04 0x1200df
    virtual void RedrawAll() { ee_gamecpp_minimap_redraw_all(this); }
    // Recomputes the terrain pixels covering a map rectangle and flags the terrain texture changed (the pixel
    // rectangle: ee_app minimap.rs)
    // +0x08 0x120115 (EEMiniMap's own slot)
    virtual void RedrawRect(long inLeft, long inRight, long inBottom, long inTop) { ee_gamecpp_minimap_redraw_rect(this, inLeft, inRight, inBottom, inTop); }
    // Copies the 64 x 64 unit layer into the world texture and relocks it for drawing
    // 0x11e7c0 (EEUIMiniMap::CopyData calls it first; provisional name)
    void UpdateWorldTexture() { ee_gamecpp_minimap_update_world_texture(this); }
    // Recomputes the camera's view trapezoid on the mini map (camera tilted 30 degrees, at least height 5),
    // shortening its sides to the width of its near edge (at least 32)
    // (the trapezoid: ee_gfx minimap.rs)
    // 0x11fc62 (after the camera jumps to a unit; provisional name)
    void Refresh() { ee_gamecpp_minimap_refresh(this); }
    // Clears the unit layer and all dots, then re-reads every tile's sight and contents diagonal by diagonal (the walk:
    // 0x11f743 (EEMMakeWorldVisible; provisional name)
    void Redraw() { ee_gamecpp_minimap_redraw(this); }
    // Counts one sight source off the mini map cell holding a tile and redraws the cell
    // 0x11e83a (EEPPlayerEvent::Deactivate; provisional name)
    void CountCellOff(long inX, long inY) { ee_gamecpp_minimap_count_cell(this, inX, inY, false); }
    // Counts one sight source onto the mini map cell holding a tile (an unexplored cell starts at 2), redraws it
    // 0x11e8bf (EEPPlayerEvent::Activate; provisional name)
    void CountCellOn(long inX, long inY) { ee_gamecpp_minimap_count_cell(this, inX, inY, true); }
    // 0x11e53b / 0x11e956: a doppelganger appears at / leaves tile (inX, inY) (EEDoppelgangerManager) (provisional
    // names)
    // Adds a building's remembered image as a dot in its mini map cell; a wall also adds its next three segments
    void AddDoppelganger(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIsWall) { ee_gamecpp_minimap_doppelganger(this, inDoppelganger, inX, inY, inIsWall, true); }
    // Removes a building image's dot from its mini map cell (and a wall's next three segments)
    void RemoveDoppelganger(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIsWall) { ee_gamecpp_minimap_doppelganger(this, inDoppelganger, inX, inY, inIsWall, false); }
    // Moves a unit's dot between mini map cells (or just updates its position within a cell); -10000 means
    // not on the map. Classes without a mini map colour and other players' foundations get no dot
    // 0x1203b9 (EEUnit::Fn229D73; -10000, -10000) (provisional name)
    void MoveUnit(EEUnit* inUnit, long inOldX, long inOldY, long inX, long inY) { ee_gamecpp_minimap_move_unit(this, inUnit, inOldX, inOldY, inX, inY); }
    // EEMiniMap.cpp's other members in address order (provisional names)
    // Attaches a map: builds the map/pixel and map/cell transformations (rotated 45 degrees, centred) and the
    // map size of one pixel, then redraws the mini map
    // (the transformations: ee_gfx minimap.rs; its translation adds are written `-x + m03` / `m13 - y`, as here)
    // 0x11eb87: the four transformations from the map's size
    void SetMap(EEMap* inMap) { ee_gamecpp_minimap_set_map(this, inMap); }
    // Recolours every player's dots after a colour change (units in player colour, buildings in shade)
    // 0x11fa55 (the dots take the players' current colours)
    void Recolor() { ee_gamecpp_minimap_recolor(this); }
    // New (port): locks the unit layer and hands its pixels to the Rust half
    // New (port): mUnitSurface locked, its pixels to the Rust half
    void LockUnitLayer() { ee_gamecpp_minimap_lock_unit_layer(this); }
    // Recomputes the terrain colour of each pixel in a texture rectangle (the lit average of the map vertices under it:
    // ee_app minimap.rs)
    // 0x1202f2 (the terrain texture's pixels)
    void RedrawPixels(long inLeft, long inRight, long inBottom, long inTop) { ee_gamecpp_minimap_redraw_pixels(this, inLeft, inRight, inBottom, inTop); }

    // port: the original's dots per 0x40 x 0x40 cell (UList<MMPlayerUnit>[0x1000] at +0x0015c, UList<MMMapUnit>
    // [0x1000] at +0x0815c) and the cells' sight counts (+0x1015c) are the Rust half's (ee_app minimap.rs; the dots
    // are its EeAppMiniMapDot and EeAppMiniMapUnitDot records)
    EeAppMiniMap* mRust;
    GE2DPoint mU1215C;                      // +0x1215c
    GE2DPoint mU12164;                      // +0x12164
    GE2DPoint mU1216C;                      // +0x1216c
    EEMap* mL12174;                         // +0x12174 (SetMap's map; RefreshView refreshes only with one)
    long mL12178;                           // +0x12178
    long mL1217C;                           // +0x1217c
    long mL12180;                           // +0x12180
    bool mB12184;                           // +0x12184
    bool mB12185;                           // +0x12185
};

inline bool EEMiniMap::MMMapUnit::operator==(const MMMapUnit& inOther) const
{
    return this->mPlayer == inOther.mPlayer && this->mColor == inOther.mColor && this->mX == inOther.mX && this->mY == inOther.mY;
}

typedef EEMiniMap::MMPlayerUnit EEMiniMapDot;
typedef EEMiniMap::MMMapUnit EEMiniMapUnitDot;

extern EEMiniMap* gMiniMap;

#endif
