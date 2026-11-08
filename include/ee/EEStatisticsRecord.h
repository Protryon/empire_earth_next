// EEStatisticsRecord.h (RECONSTRUCTION; file name inferred; class and member names are provisional).
// One player's record in the world's statistics history (EEStatisticsHistory::mRecords, sizeof 0x378; record 0 is
// gaia, the post-game summary walks 1..size-1). Layout from its members: the ctor 0x13b66c (EEPlayerStatistics ctor,
// two UWideString ctors, the colour 1,1,1, then 0x16dadc), the copy ctor 0x13d28a, operator= 0x13e451 (a 7-dword
// copy loop at +0x330) and the dtor 0x16db75 (no vptr stores: the statistics are a member, not a base). Uses from the
// post-game summary (0x1e2bf1..0x1e5711). Requires ee/EEPlayer.h (EEPlayerStatistics), ee/EEStatisticsHistory.h.
#ifndef EE_EESTATISTICSRECORD_H
#define EE_EESTATISTICSRECORD_H

class EEStatisticsRecord {
public:
    EEStatisticsRecord();                   // 0x13b66c
    // the copy ctor 0x13d28a and operator= 0x13e451 are implicit (member-wise)
    ~EEStatisticsRecord();                  // 0x16db75
    void Clear();                           // 0x16dadc (the ctor's last call; provisional name)

    EEPlayerStatistics mStatistics;         // +0x000
    const UWideString& GetName() const { return this->mName; }  // inline (EEUIEPGSEOpen 0x5e3d6f)
    const GEColor& GetColor() const { return this->mColor; }
    long GetResult() const { return this->mResult; }
    bool GetIsHuman() const { return this->mIsHuman; }
    UWideString mName;                      // +0x304 (the summary's name label)
    GEColor mColor;                         // +0x318 (the player's colour: bars and graph series)
    long mResult;                           // +0x324 (1 / 2: the victory / defeat banner; 0 ends the report loop)
    unsigned long mTeam;                    // +0x328 (shown when non-zero, label n + 0x5eb)
    unsigned long mEndTime;                 // +0x32c (milliseconds: the summary scales it by 0.001)
    unsigned long mResources[7];            // +0x330 (the summary adds [2..6] for the series 7..11)
    unsigned long mScore;                   // +0x34c
    unsigned long mEconomyScore;            // +0x350
    unsigned long mEpochScore;              // +0x354
    unsigned long mMilitaryScore;           // +0x358
    unsigned long mTechScore;               // +0x35c
    unsigned char mSlot;                    // +0x360 (compared with 0xfe)
    bool mIsLocal;                          // +0x361
    bool mIsHuman;                          // +0x362 (counted apart: numbered names for the others)
    UWideString mNameSuffix;                // +0x364
};

inline EEStatisticsSeries EEPlayerStatistics::GetSeries(unsigned long inSeries) { return this->mSeries[inSeries]; }

// inline (the post-game Open calls the COMDAT 0x1e5d0f out of line)
inline const EEStatisticsRecord& EEStatisticsHistory::GetCurrentRecord() const { return this->mRecords[this->mCurrentRecordIndex]; }

#endif
