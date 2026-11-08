// C:\EEX\Empire Earth\...\EEAIForest.h (RECONSTRUCTION; file path inferred). An AI map area
// (RTTI EEAIForest > EEAIArea @0; vtable 0x43a42c, 11 slots). Only the base is declared: EECPILocation::Update 0x80a57 reads the
// EEAIArea serial of what EEAIContinentManager's lookup returns. Requires ee/EEAIArea.h.
#ifndef EE_EEAIFOREST_H
#define EE_EEAIFOREST_H

class EEAIForest : public EEAIArea {
public:
    // (EECPResourceManager 0x90195 / 0x90af3): +0x80 its continent, +0x82 its id (the continent manager's mForests
    // index); +0x68..+0x80 not decoded
    // (EEAIAStarPathAreaGoal::CreateAreas 0x17510): ctor 0x29bcd (out of line), UpdateResourceKind
    // +0x6c/+0x70/+0x74 the resource tile counts by kind 1/3/2 (provisional names)
    EEAIForest();                           // 0x29bcd
    virtual ~EEAIForest() {}                // ??_G 0x29f35 (stores its own vptr: user-declared)
    bool RegrowTrees(const U2DPoint<long>& inTile, long inRadius, long inCount);  // 0x29c01
    void UpdateResourceKind();              // 0x29d69 (after the counts are set)
    // +0x28 0x29e17: EEAIArea's, then the removed tile's resource kind (EEAIHasResourceUnit) leaves its count
    virtual bool RemoveTile(const U2DPoint<long>& inPoint);
    long mResourceKind;                     // +0x68 (EEAmbientWorldAnalyzer 0x124e99 keeps it) (provisional name)
    long mNumResources1;                    // +0x6c
    long mNumResources2;                    // +0x70
    long mNumResources3;                    // +0x74
    long mL78;                              // +0x78 (the ctor zeroes it)
    unsigned long mNextRenewal;             // +0x7c (unsigned deadline, 0x22a3fd)
    unsigned short GetContinentID() const { return this->mContinentID; }
    unsigned short mContinentID;            // +0x80 (provisional name)
    unsigned short GetID() const { return this->mID; }
    unsigned short mID;                     // +0x82 (provisional name)
};

#endif
