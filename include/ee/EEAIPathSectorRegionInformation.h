// C:\EEX\Empire Earth\...\EEAIPathSectorRegionInformation.h (RECONSTRUCTION; file path inferred).
// The path-sector grid geometry of the continent manager (RTTI EEAIPathSectorRegionInformation, no bases; vtable
// 0x43a218, one slot ??_G 0x263ef; sizeof 0x14: EEAIContinentManager +0xdc..+0xf0). Ctor 0x2bb72 out of line (stores
// the vptr, then Reset()); the dtor is a user-declared empty inline (??_G 0x263ef stores the own vptr and calls no
// ??1; EEAIContinentManager's dtor 0x1f11f expands it to the vptr store). Member names are provisional.
// Requires ee/EEMap.h only for callers of Init (forward-declared here).
#ifndef EE_EEAIPATHSECTORREGIONINFORMATION_H
#define EE_EEAIPATHSECTORREGIONINFORMATION_H

class EEMap;
template <class T> class U2DPoint;

class EEAIPathSectorRegionInformation {
public:
    EEAIPathSectorRegionInformation();      // 0x2bb72
    virtual ~EEAIPathSectorRegionInformation() {}   // +0x00 (??_G 0x263ef)
    // 0x2bb84: every member 0 (the ctor and EEAIContinentManager 0x1f624 call it)
    void Reset();
    // 0x2bb93 (ret 8): mSectorSize = inSectorSize, mSectorArea = its square, mSectorsX = mSectorsY = the ceiling of
    // inMap->mSize / inSectorSize (fild/fidiv/fistp; the manager's 0x20b48 passes 10/12/14/15 by map size; the
    // ceiling in Rust: ee_astar sectors.rs sectors_per_side)
    void Init(EEMap* inMap, long inSectorSize);
    // GetSectorEnd 0x263cb (the last tile of a sector) is part of ee_astar sectors.rs sector_rect

    long mSectorSize;                       // +0x04 tiles per sector side (0x22bf8 divides the tile x, y by it)
    long mSectorArea;                       // +0x08 mSectorSize * mSectorSize
    long mSectorsX;                         // +0x0c sectors per side (0x22bf8: index = (x / size) * mSectorsX + y / size)
    long mSectorsY;                         // +0x10 (= mSectorsX; 0x20b48 resizes the sector vector to X * Y + 1)
};

#endif
