// C:\EEX\Empire Earth\AI Thread\EECPExplorationManager.h (RECONSTRUCTION; file name inferred). A computer player's
// exploration manager (RTTI EECPExplorationManager, no base; vtable 0x43c1b0, 6 slots; ctor 0x7ed38(inPlayer) from
// EEComputerPlayer 0x2fcc2/0x3cfb9, kept at EEComputerPlayer +0xbcc; EECPExplorationManager.cpp 0x7ed38..0x8013d) and
// the screener it hands EEComputerPlayer::GetScreenedTypesByLine. Member and slot names are provisional.
// Requires <vector>, ee/EECPUnitSelector.h (the screeners).
#ifndef EE_EECPEXPLORATIONMANAGER_H
#define EE_EECPEXPLORATIONMANAGER_H

#include <vector>

class EEComplexUnit;
class EEComputerPlayer;
class EECPPersonnelRequest;
class EEUnitClass;
class FSFile;
struct EEPUnitSwap;

// RTTI EEAIFOAcceptableComputerPlayerExplorerUnitScreener > EEAIFOCurrentlyBuildableUnitScreener (vtable 0x43c1cc:
// ??_G 0x783ca shared by ICF, +0x04 0x2afdf); a 0xc-byte temporary of EECPExplorationManager::RequestExplorer.
class EEAIFOAcceptableComputerPlayerExplorerUnitScreener : public EEAIFOCurrentlyBuildableUnitScreener {
public:
    EEAIFOAcceptableComputerPlayerExplorerUnitScreener(EEComputerPlayer* inPlayer)
        : EEAIFOCurrentlyBuildableUnitScreener(inPlayer)
    {
    }
    // declared empty: 0x7ee60 stores this class's vptr before the base dtor 0x7f148
    virtual ~EEAIFOAcceptableComputerPlayerExplorerUnitScreener() {}
    virtual bool Screen(EEUnitClass* inClass);   // +0x04 0x2afdf
};

class EECPExplorationManager {
public:
    EECPExplorationManager(EEComputerPlayer* inPlayer);   // 0x7ed38 (ret 4)
    virtual ~EECPExplorationManager();      // +0x00 (??_G 0x7ed7a, dtor 0x7ed96: releases the requests)
    virtual bool IsExplorer(unsigned long inUnitID);   // +0x04 0x7f14f
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x08 0x7f1a4
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // +0x0c 0x7f352
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x10 0x7f452
    virtual void Update(unsigned long inTime);   // +0x14 0x7f590

    void RequestExplorer(long inKind);      // 0x7edf7 (ret 4): kinds 0 land, 1 (+0x2d0 bit 4), 2 (bit 1), 3 (bit 8)
    void UnitsSwapped(std::vector<EEPUnitSwap>& inSwaps);   // 0x7f26d (ret 4; EEComputerPlayer 0x3ced7)
    void UnitRemoved(EEComplexUnit* inUnit, bool inSwapped);   // 0x7f4f4 (ret 8; EEComputerPlayer 0x3ef91)

    // vptr                                 // +0x00
    unsigned long GetUnitIDsAt(unsigned long inIndex) const { return this->mUnitIDs[inIndex]; }
    std::vector<unsigned long> mUnitIDs;    // +0x04 the explorers (saved through FSSaveVector, key "AI")
    EECPPersonnelRequest* GetRequestsAt(unsigned long inIndex) const { return this->mRequests[inIndex]; }
    std::vector<EECPPersonnelRequest*> mRequests;   // +0x14 (referenced; saved as their ids, Restore resolves them)
    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;              // +0x24
    unsigned long GetNextTime() const { return this->mNextTime; }
    unsigned long mNextTime;                // +0x28 (Update: inTime + 30000)
    bool mB2C;                              // +0x2c (saved)
    bool GetEnabled() const { return this->mEnabled; }
    bool mEnabled;                          // +0x2d (saved; Update does nothing while clear)
};

// Explorer kinds requested by EECPExplorationManager::RequestExplorer.
enum EECPExplorerKind {
    kExplorerLand = 0,  // EECPExplorationManager::Update requests it when theLandCount < theLandWanted (mNumLandExplorers, 28008); RequestExplorer case 0 takes classes with no air/sea/space flag (mDomains & 0xd == 0) (provisional name)
    kExplorerSea = 1,  // Update: theSeaCount < theSeaWanted (28009); case 1: mDomains & 4, largest sea continent (provisional name)
    kExplorerAir = 2,  // Update: theAirCount < theAirWanted (28010); case 2: mDomains & 1 (provisional name)
    kExplorerSpace = 3,  // Update: theSpaceCount < theSpaceWanted; case 3: mDomains & 8, GetLargestContinent(4) (provisional name)
};

#endif
