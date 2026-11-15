// C:\EEX\Empire Earth\Player\EEWUnitsOffMapInfo.h (RECONSTRUCTION; file name inferred; layout proven:
// EEWUnitsOffMapInfo sizeof 0x18 = its new-site size, ctor 0x161c55 exact in EEPlayer.cpp; member names provisional). A
// unit kept off the map (RTTI EEWUnitsOffMapInfo, no base; vtable 0x4429d0: the deleting dtor only; sizeof 0x18:
// EEPlayer news one at 0x162dc0). It holds a reference on the unit; a saved one keeps the unit id in
// mUnit until RepairPointers looks it up. Defined at the start of EEPlayer.cpp (0x161c55..0x161da5).
// Requires ee/EETileLocation.h, ee/EEUnit.h (RTSUnit).
#ifndef EE_EEWUNITSOFFMAPINFO_H
#define EE_EEWUNITSOFFMAPINFO_H

class FSFile;
class RTSUnit;

class EEWUnitsOffMapInfo {
public:
    EEWUnitsOffMapInfo();                   // 0x161c55
    virtual ~EEWUnitsOffMapInfo();          // +0x00 0x161c8b (??_G 0x161c6f)

    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x161c96
    void ReleaseUnit();                     // 0x161ce7
    long RepairPointers(unsigned long inFirst, unsigned long inSecond);   // 0x161cfc
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // 0x161d22
    void SetUnit(RTSUnit* inUnit);          // 0x161d84

    EETileLocation mLocation;               // +0x04 (saved)
    RTSUnit* mUnit;                         // +0x0c (a reference; its id while loading)
    unsigned long mTime;                    // +0x10 (saved)
    long mReason;                           // +0x14 (saved)
};

#endif
