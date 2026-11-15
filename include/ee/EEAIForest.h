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
    // New (port): the members behind accessors (R5b.1 blocker (b))
    long GetResourceKind() const { return this->mResourceKind; }
    long GetNumResources1() const { return this->mNumResources1; }
    long GetNumResources2() const { return this->mNumResources2; }
    long GetNumResources3() const { return this->mNumResources3; }
    void SetNumResources1(long inCount) { this->mNumResources1 = inCount; }
    void SetNumResources2(long inCount) { this->mNumResources2 = inCount; }
    void SetNumResources3(long inCount) { this->mNumResources3 = inCount; }
    unsigned long GetNextRenewal() const { return this->mNextRenewal; }
    void SetNextRenewal(unsigned long inTime) { this->mNextRenewal = inTime; }
    unsigned short GetContinentID() const { return this->mContinentID; }
    void SetContinentID(unsigned short inID) { this->mContinentID = inID; }
    unsigned short GetID() const { return this->mID; }
    void SetID(unsigned short inID) { this->mID = inID; }
    // the members in a saved game (EEAIContinentSave.cpp's writer and reader), in this order
    template <class W> void WriteFields(W& ioWriter) const
    {
        ioWriter.I32(this->mResourceKind);
        ioWriter.I32(this->mNumResources1);
        ioWriter.I32(this->mNumResources2);
        ioWriter.I32(this->mNumResources3);
        ioWriter.I32(this->mL78);
        ioWriter.U32(this->mNextRenewal);
        ioWriter.U16(this->mContinentID);
        ioWriter.U16(this->mID);
    }
    template <class R> bool ReadFields(R& ioReader)
    {
        return ioReader.I32(this->mResourceKind) && ioReader.I32(this->mNumResources1)
            && ioReader.I32(this->mNumResources2) && ioReader.I32(this->mNumResources3) && ioReader.I32(this->mL78)
            && ioReader.U32(this->mNextRenewal) && ioReader.U16(this->mContinentID) && ioReader.U16(this->mID);
    }

private:
    long mResourceKind;                     // +0x68 (EEAmbientWorldAnalyzer 0x124e99 keeps it) (provisional name)
    long mNumResources1;                    // +0x6c
    long mNumResources2;                    // +0x70
    long mNumResources3;                    // +0x74
    long mL78;                              // +0x78 (the ctor zeroes it)
    unsigned long mNextRenewal;             // +0x7c (unsigned deadline, 0x22a3fd)
    unsigned short mContinentID;            // +0x80 (provisional name)
    unsigned short mID;                     // +0x82 (provisional name)
};

#endif
