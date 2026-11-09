// C:\EEX\Low-Level Engine\Sound Engine\SESMInstanceNode.h (RECONSTRUCTION). One playing (or paused) instance of a
// sound-manager sound, pooled in SESoundManager::sSoundsPool. Its out-of-line members are defined in
// SESoundManager.cpp (0x4f0d3..0x4f15c); the inline ones are emitted first by SEConductor.cpp (0x4c47e..0x4c500).
// Requires lle/LowLevelEngine.h.
#ifndef LLE_SESMINSTANCENODE_H
#define LLE_SESMINSTANCENODE_H

class SESample;

class LLE_API SESMInstanceNode : public UReferenceCounter {   // proven sizeof 0x34 (??_E: index * 0x34)
    friend class SESoundManager;
public:
    SESMInstanceNode();
    SESMInstanceNode(const SESMInstanceNode& inNode) {}
    virtual ~SESMInstanceNode();            // out of line in SESoundManager.cpp (ICF-folded, kept at 0x6e04c);
                                            // ??_E calls it (0x4c4a5)
    void Clear();
    void Dereference()
    {
        if (this->mReferenceCount > 1)
            UReferenceCounter::Dereference();
        else
            this->LastDereference();
    }
    void InitializeFromPool();
    void LastDereference();
    void Reference() { UReferenceCounter::Reference(); }   // exported, ICF-folded into UReferenceCounter's (0x436c)
private:
    void DeleteThis() { this->mIsFree = true; }
    SESMInstanceNode& operator=(const SESMInstanceNode& inNode) { return *this; }   // exported, ICF-folded (0xd5a1)

    unsigned long mCategory;                // +0x08 (-1; SESoundManager::GetCategory) provisional names from here on
    unsigned long mSampleID;                // +0x0c (-1; GetNumberOfInstances)
    unsigned long mMaxInstances;            // +0x10 (SESoundManager::Load: the sound's instance count)
    unsigned long mInstanceID;              // +0x14 (-1; GetInstanceID)
    unsigned long mInstanceIndex;           // +0x18 (-1; its slot in the sound's instance vector)
    unsigned long mChannel;                 // +0x1c (-1: not on a channel)
    SESample* mSample;                      // +0x20 (GetIsLooping)
    float mVolume;                          // +0x24
    float mPan;                             // +0x28
    float mPitch;                           // +0x2c
    bool mIsFree;                           // +0x30
};

#endif
