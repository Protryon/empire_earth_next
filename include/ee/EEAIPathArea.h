// C:\EEX\Empire Earth\...\EEAIPathArea.h (RECONSTRUCTION, STUB; file path inferred). A path area
// of the AI's continent map (RTTI EEAIPathArea > EEAIArea @0; ctor 0x18a03, vtable 0x439fac; its id +0x7a =
// EEAIContinentManager::NewPathAreaID()). EEAIPathSector::GetPathAreaAt 0x2bd4d calls the EEAIArea inline Contains
// (0xc6fd) on it; members only where a decompiled body reads them (gaps elsewhere). Requires ee/EEAIArea.h.
#ifndef EE_EEAIPATHAREA_H
#define EE_EEAIPATHAREA_H

class EEAIPathArea : public EEAIArea {
public:
    // inline ctor (COMDAT 0x18a03, first user EEAIAStarPathAreaGoal::CreateAreas; ??_G 0x18a58)
    EEAIPathArea() : mUnitID(-1)
    {
        this->mContinentID = 0;
        this->mID = 0;
        this->mKind = 0;
        this->mSurfaceRegionID = 0;
        this->mWaterBodyID = 0;
        this->mLandMassID = 0;
        this->mSpaceRegionID = 0;
    }
    // a user dtor: ??_G 0x418a58 stores this vtable before destroying mV68
    virtual ~EEAIPathArea() {}

    std::vector<unsigned short> mSectorIDs;  // +0x68 (the ctor's _Buy(0) of a 2-byte vector) (provisional name)
    unsigned short GetContinentID() const { return this->mContinentID; }
    unsigned short mContinentID;            // +0x78 (0x208cc: GetContinentByID) (provisional name)
    unsigned short mID;                     // +0x7a (EEAIContinentManager::NewPathAreaID())
    long GetKind() const { return this->mKind; }
    long mKind;                             // +0x7c (the tile kind of EEAIGetTileKind: 0x204ea tests 2, 4, 7..0xc, 0xf) (provisional name)
    long mUnitID;                           // +0x80 (-1; CreateAreas stores the goal's owner id on kind 0xc) (provisional name)
    // +0x84..+0x8a: ids by kind (EEAIContinentManager::GetContinent 0x22b86: kind 4, 1, 2, 3;
    // EEAIContinent::GetShoreInRegions reads +0x86 and +0x8a) (provisional names)
    unsigned short GetSurfaceRegionID() const { return this->mSurfaceRegionID; }
    unsigned short mSurfaceRegionID;
    unsigned short GetWaterBodyID() const { return this->mWaterBodyID; }
    unsigned short mWaterBodyID;
    unsigned short GetLandMassID() const { return this->mLandMassID; }
    unsigned short mLandMassID;
    unsigned short GetSpaceRegionID() const { return this->mSpaceRegionID; }
    unsigned short mSpaceRegionID;

    void AddSectorID(unsigned short inSectorID);  // 0x2baf4 (EEAIContinentManager 0x20b48: the sub-sector's path sector id) (provisional name)
};

#endif
