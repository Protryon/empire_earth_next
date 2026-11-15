// EEStatisticsHistory.h (RECONSTRUCTION; file name inferred; provisional class name). Held by the world
// at +0xd8 (sizeof 0x38). Its functions sit in the Player group next to EEPlayerStatistics (0x16d453..0x16e1a2):
// Reset 0x16db99, the dtor 0x16dbc3; the ctor is inline (its refused copy 0x137e06 follows EEWorld's ctor, its first
// user). The records (sizeof 0x378) hold an EEPlayerStatistics at +0 (its dtor 0x16d498 is called by the record's
// 0x16db75) and UWideStrings at +0x304 and +0x364. Requires lle/LowLevelEngine.h (UWideString), <vector>.
#ifndef EE_EESTATISTICSHISTORY_H
#define EE_EESTATISTICSHISTORY_H

#include <vector>

class EEStatisticsRecord;                   // sizeof 0x378 (provisional name; layout above)

class EEStatisticsHistory {
public:
    // 0x137e06: the members' ctors, then Reset(). Probably inline in the real header (0x137e06 follows EEWorld's ctor,
    // its first user), which needs the record type complete; declared here until EEStatisticsRecord is decoded
    EEStatisticsHistory();
    ~EEStatisticsHistory();                 // 0x16dbc3
    // 0x16db99: clears the records and the name inline, zeroes +0x24..+0x37 (provisional name)
    void Reset();
    // inline (its COMDAT 0x1e5d0f sits in the post-game summary's range): &mRecords[mU28]; defined in
    // ee/EEStatisticsRecord.h, which completes the record type (provisional name)
    const EEStatisticsRecord& GetCurrentRecord() const;
    // inline const accessor: the summary indexes the records through the const operator[] (the first is loaded
    // into a register before the scaled index is added, 0x1e2c12; provisional name)
    const std::vector<EEStatisticsRecord>& GetRecords() const { return this->mRecords; }

    std::vector<EEStatisticsRecord> mRecords;   // +0x00
    UWideString mCampaignName;              // +0x10
    long mStartTime;                        // +0x24
    long mCurrentRecordIndex;               // +0x28
    long mEndTime;                          // +0x2c
    long mTileCount;                        // +0x30
    bool mTeamsLocked;                      // +0x34
    bool mVictoryEnabled;                   // +0x35
    bool mIsScenario;                       // +0x36
    bool mIsCampaign;                       // +0x37
};

#endif
