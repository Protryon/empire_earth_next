// EEPPatrolGroup (RECONSTRUCTION; file name inferred): the patrol points a group of units walks (RTTI EEPPatrolGroup >
// UReferenceCounter; vtable 0x442980; ctor 0x16066d; sizeof 0x20: `new 0x20` in EEMAddPatrolPoint and EEUGPatrol's
// ctor). Held by a patrolling EEUGPatrol (+0x20), by EEComplexUnit +0x1c4 and EEPlayer (0x16279f).
// Names are provisional. Requires lle/LowLevelEngine.h.
#ifndef EE_EEPPATROLGROUP_H
#define EE_EEPPATROLGROUP_H

#include <vector>

class EEComplexUnit;
class EEPlayer;
class FSFile;
class GE3DPoint;

class EEPPatrolGroup : public UReferenceCounter {
public:
    EEPPatrolGroup();                       // 0x16066d (+0x18 = a global counter's next value)
    virtual ~EEPPatrolGroup();              // ??_G 0x1606b0, dtor 0x1606cc
    static void ResetIDs();                 // 0x160cdc (the counter back to 0; called from 0x13883a)
    // (provisional names): the members the other units do not call
    void AppendPatrolPoint(const GE3DPoint& inPoint, unsigned long inPlayerID);  // 0x1607c8 (appends after back())
    unsigned long IndexOf(EEComplexUnit* inUnit);    // 0x1608db (inUnit's index; a unit not in the list: its nearest point's)
    void InsertPatrolPoint(const GE3DPoint& inPoint, unsigned long inPlayerID, EEComplexUnit* inAfter);  // 0x160a58 (a new point unit)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile);   // 0x160ade
    void RemoveUnit(EEComplexUnit* inUnit);  // 0x160b72 (drops inUnit; the last one releases the group from its player)
    long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion);   // 0x160bf2 (ids -> units)
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile);   // 0x160ce4
    void AddPoint(const GE2DPoint& inPoint, unsigned long inPlayerID);   // 0x160727
    // 0x1607f4: the patrol's unit nearest to (inTileX, inTileY) (EEMPatrol; provisional name)
    EEComplexUnit* GetNearest(long inTileX, long inTileY);
    void InsertPatrolPointAt(const GE2DPoint& inPoint, unsigned long inPlayerID, EEComplexUnit* inUnit);  // 0x1609e4
    void Select(EEPlayer* inPlayer, bool inFlag);    // 0x160d70
    // 0x160da9: the patrol points from inPoints (taken by value: the callers copy-construct a vector on the stack)
    // for player inPlayerID (EETown 0xabc27/0xac4b6/0xac573; provisional name)
    void SetPatrolPoints(std::vector<U2DPoint<long> > inPoints, unsigned long inPlayerID);
    // EEUGPatrol.cpp's callees (provisional names)
    EEComplexUnit* GetNext(EEComplexUnit* inUnit);   // 0x16085d (0: past the end)
    EEComplexUnit* GetPrevious(EEComplexUnit* inUnit);   // 0x16098a (0: before the start)
    EEComplexUnit* GetLast();               // 0x16363e (the last point's unit, 0 if none)
    // 0x228031: the number of points (defined in EEUGPatrol.cpp right after its first caller; never expanded)
    unsigned long GetNumPoints() { return this->mPatrolPoints.size(); }  // inline (out-of-line COMDAT 0x228031)

    std::vector<EEComplexUnit*> mPatrolPoints;  // +0x08 (the patrol points; GetLast 0x16363e returns back())
    unsigned long mID;                      // +0x18 (a global counter at 0x531d8c)
    bool mIsLoop;                           // +0x1c (= false)
    bool mIsTownPatrol;                     // +0x1d (= false)
};

#endif
