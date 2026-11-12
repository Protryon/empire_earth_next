// C:\EEX\Empire Earth\RTS Map\EEMiniMap.h (RECONSTRUCTION; file name inferred). The mini map (RTTI
// EEMiniMap; cluster 0x11e266..0x1211c5). Only what the hot keys use.
#ifndef EE_EEMINIMAP_H
#define EE_EEMINIMAP_H

#include <list>
#include "lle/UList.h"

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
    RTSMiniMap(RTSPlayer* inPlayer, GEScreen* inScreen);   // 0x1240b3 (arguments inferred: the screen makes the surfaces)
    virtual ~RTSMiniMap();                  // +0x00 (??_G 0x124418)
    virtual void RedrawAll() = 0;           // +0x04
    void Ping(const GE2DPoint& inPoint);       // 0x124497: flags the cell under a world point (provisional name)

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
    virtual ~EEMiniMap();                   // +0x00 (??_G 0x11e4da)
    void RefreshView();                     // 0x11e7b1 (the editor's trigger screen after the view changes; provisional name)
    virtual void RedrawAll();               // +0x04 0x1200df
    virtual void RedrawRect(long, long, long, long);  // +0x08 0x120115 (EEMiniMap's own slot)
    void UpdateWorldTexture();              // 0x11e7c0 (EEUIMiniMap::CopyData calls it first; provisional name)
    void Refresh();                         // 0x11fc62 (after the camera jumps to a unit; provisional name)
    void Redraw();                          // 0x11f743 (EEMMakeWorldVisible; provisional name)
    void CountCellOff(long inX, long inY);  // 0x11e83a (EEPPlayerEvent::Deactivate; provisional name)
    void CountCellOn(long inX, long inY);   // 0x11e8bf (EEPPlayerEvent::Activate; provisional name)
    // 0x11e53b / 0x11e956: a doppelganger appears at / leaves tile (inX, inY) (EEDoppelgangerManager) (provisional
    // names)
    void AddDoppelganger(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIsWall);
    void RemoveDoppelganger(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIsWall);
    void MoveUnit(EEUnit* inUnit, long inX, long inY, long inArg4, long inArg5);   // 0x1203b9 (EEUnit::Fn229D73; -10000, -10000) (provisional name)
    // EEMiniMap.cpp's other members in address order (provisional names)
    void SetMap(EEMap* inMap);              // 0x11eb87: the four transformations from the map's size
    void TellTileVisibility(EETile* inTile, long inX, long inY);  // 0x11f0db (Redraw: a tile's visibility)
    void TellTile(EETile* inTile, long inX, long inY);   // 0x11f184 (Redraw: a tile's doppelgangers and units)
    void GetPixelColor(long inX, long inY, GEColor& outColor);  // 0x11f272 (the terrain colour of a mini-map pixel)
    void Recolor();                         // 0x11fa55 (the dots take the players' current colours)
    void RedrawCell(long inX, long inY);    // 0x11fb2a (one cell's pixel)
    void RedrawPixels(long inLeft, long inRight, long inBottom, long inTop);  // 0x1202f2 (the terrain texture's pixels)

    // +0x0015c / +0x0815c: UList<EEMiniMapDot>[0x1000] / UList<EEMiniMapUnitDot>[0x1000] in the original (the dots
    // per 0x40 x 0x40 cell; EEMiniMap's ctor/dtor loops; the lists' template statics renumber every includer's
    // `$E`)
    UList<MMPlayerUnit> mPlayerUnits[0x1000];  // +0x0015c
    UList<MMMapUnit> mMapUnits[0x1000]; // +0x0815c
    unsigned short mU1015C[0x1000];         // +0x1015c per-cell visibility count (0: unexplored; the ctor zeroes it last)
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

extern EEMiniMap* gMiniMap;                 // 0x564888 (provisional name)

#endif
