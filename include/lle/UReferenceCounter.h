// C:\EEX\Low-Level Engine\Utilities\UReferenceCounter.h (RECONSTRUCTION; UReferenceCounter.cpp 0x8104b..0x81d93).
// Split from lle/LowLevelEngine.h (which still includes it, same token order) so that an engine unit can see the
// class without the umbrella: File System.cpp is its first includer in link order. Requires LLE_API
// (lle/LowLevelEngineCore.h).
#ifndef LLE_UREFERENCECOUNTER_H
#define LLE_UREFERENCECOUNTER_H

// Every member below is inline except the reference-tracking ones (UReferenceCounter.cpp, 0x1008104b..); the inline
// block is emitted first by File System.cpp (0x1000436c..0x100043dd, declaration order; the dtor is ICF-folded at
// 0x1003bf2f).
class FSFileSpec;
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_EXE_HEADERS)
#include <map>      // sReferenceDebugInfo's maps (engine view); GetBucketMap's result (the game's view)
#endif
class LLE_API UReferenceCounter {
public:
    // inline: mixer SPIMDuplicateSample increments +4 in place
    void Reference() { this->mReferenceCount++; }
    // inline (body 0x4370): VC6 refuses to expand the `delete this` body wherever the caller does anything else, so
    // every engine and plug-in caller calls it out of line (probe: a lone call inlines)
    void Dereference() { if (--this->mReferenceCount == 0) delete this; }
    unsigned long GetReferenceCount() const { return this->mReferenceCount; }  // ICF-folded (0x4380)
    // reference tracking (UReferenceCounter.cpp): per-caller counts keyed by (line, file)
    static void DumpAllLeakedReferences(FSFileSpec& inFile);
    void Dereference(unsigned long inLine, const char* inFile);
    void Reference(unsigned long inLine, const char* inFile);
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_EXE_HEADERS)
    // Engine and game views (the exe calls __imp_?GetBucketMap@UReferenceCounter@@SAPAV?$map@... at 0x1f8df4, a
    // reference-leak dump that walks the returned map). Typedef provisional names: line -> counter -> file -> count.
    typedef std::map<const char*, unsigned long> FileCountMap;
    typedef std::map<UReferenceCounter*, FileCountMap*> CounterMap;
    typedef std::map<unsigned long, CounterMap*> LineMap;
    static CounterMap* GetBucketMap(unsigned long inLine);
#endif
#ifdef LOWLEVELENGINE_EXPORTS
    // engine view only: a static data member costs every includer a $E number; the plug-ins saw none
private:
    static LineMap sReferenceDebugInfo;     // 0x1012ec00
#endif
protected:
    // exported ??0UReferenceCounter@@IAE@XZ (0x4384): protected, the count assigned in the body (vptr stored first;
    // GEMaterial ctor 0x16b9b)
    UReferenceCounter() { this->mReferenceCount = 1; }
    UReferenceCounter(unsigned long inLine, const char* inFile) : mReferenceCount(0) { this->Reference(inLine, inFile); }
    // inline: the mixer's SampleData construction stores the count (1) with no call; the base vtable is
    // re-stored from the imported ??_7UReferenceCounter@@6B@ in ~SampleData. Protected (exported ??1...@@MAE@XZ).
    virtual ~UReferenceCounter() {}
    void SetReferenceCountToZeroWithoutDeleting() { this->mReferenceCount = 0; }
public:

    unsigned long mReferenceCount;            // proven +0x04
};

#endif
