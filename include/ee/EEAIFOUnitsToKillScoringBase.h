// C:\EEX\Empire Earth\AI Thread\EEAIFOUnitsToKillScoringBase.h (RECONSTRUCTION; file name inferred). The base of
// the "which own units to delete" scorers that EEComputerPlayer 0x2e851 (ret 8)
// takes (RTTI EEAIFOUnitsToKillScoringBase, vtable 0x43a47c: ??_G 0x7b906, +0x04 0x2a193, +0x08 0x2a019, +0x0c 0x2a126,
// +0x10/+0x14 _purecall). Its ctor (0x7b7e4: -1, 0, 0, 0 before the vptr) and ??_G are out-of-line COMDATs after their
// first user in EECPDefenseCommander.cpp; the three scores (each returns a long) and the static 0x2a40f (cdecl) sit
// before EEComputerPlayer.cpp and are not decompiled yet. Names are provisional (slots by offset).
#ifndef EE_EEAIFOUNITSTOKILLSCORINGBASE_H
#define EE_EEAIFOUNITSTOKILLSCORINGBASE_H

class EEComplexUnit;
class EEComputerPlayer;

class EEAIFOUnitsToKillScoringBase {
public:
    EEAIFOUnitsToKillScoringBase() : mCount(-1), mCitizensToKill(0), mTransportsToKill(0), mMilitaryToKill(0) {}
    virtual ~EEAIFOUnitsToKillScoringBase() {}   // +0x00 (??_G 0x7b906)
    virtual long ScoreCitizen(EEComplexUnit* inUnit);  // +0x04 0x2a193
    virtual long ScoreMilitary(EEComplexUnit* inUnit);  // +0x08 0x2a019
    virtual long ScoreTransport(EEComplexUnit* inUnit);  // +0x0c 0x2a126
    virtual bool CanKill(EEComplexUnit* inUnit) = 0;     // +0x10 (whether inUnit may be deleted)
    virtual long GetKillScore(EEComplexUnit* inUnit) = 0;  // +0x14 (its score)
    // 0x2a40f (cdecl): a count for inPlayer through 0x2a348 (provisional name)
    static long GetCitizensToKill(EEComputerPlayer* inPlayer);
    // 0x2a348 (cdecl; also 0x487423): the citizens counted (EECPUGatherCitizenUnitsCount) against inPlayer's
    // Virtual7C(0) maximum: outRatio = counted / maximum, outCount = ceil(maximum * outRatio), returns outCount -
    // ceil(maximum * mCitizenPercentage) (provisional name)
    static long GetCitizenSurplus(EEComputerPlayer* inPlayer, float& outRatio, long& outCount);

    long mCount;                            // +0x04 (-1; the scorers' users store how many to delete)
    long mCitizensToKill;                   // +0x08
    long GetTransportsToKill() const { return this->mTransportsToKill; }
    long mTransportsToKill;                 // +0x0c
    long mMilitaryToKill;                   // +0x10
};

#endif
