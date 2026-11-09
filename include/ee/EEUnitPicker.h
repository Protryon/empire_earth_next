// EEUnitPicker.h (RECONSTRUCTION; file name inferred). Finds the unit under a screen point (RTTI EEUnitPicker;
// vtable 0x441c48, ctor 0x118cf8, dtor 0x118e35; EEUnitPicker.cpp 0x118aa2..0x11a1cc, __FILE__ 0x4be904). The game's
// instance is a global object at 0x528d78 (provisional name; its init chain 0x10ab0e passes 0x600). The render pass
// (EEROUnits) adds every drawn unit with its transformation (AddUnit); the pickers test the screen ray / rectangle
// against those models under mLock. Only what the decompiled units call. Requires lle/GE2DPoint.h.
// The data members need lle/GEViewport.h and the vector/plane headers: they are declared only where GEViewport.h came
// first (the members are not read inline anywhere else; same device as ee/EEMap.h's `#ifdef EE_EEUNIT_H`).
#ifndef EE_EEUNITPICKER_H
#define EE_EEUNITPICKER_H

class EEUnit;
class EEComplexUnit;
class EEPlayer;
class GERectangle;
class GETransformation;
class GE3DPlane;
class TSCriticalSection;

#ifdef LLE_GEVIEWPORT_H
// one drawn unit: its model where it was drawn and the unit (referenced) (0x50 bytes; `new[]` in the ctor runs
// GEPhysicalModel's inline ctor per element; its vector deleting dtor 0x118dd7 calls ~GEPhysicalModel) (provisional
// name)
struct EEUnitPickerEntry : public GEPhysicalModel {
    EEUnit* mUnit;                          // +0x4c
};

// a hit of PickUnit's ray (0x14 bytes; its vector's members are ICF-folded with vector<GEVideoMode>'s) (provisional
// name)
struct EEUnitPickerHit {
    float mDistance;                        // +0x00 along the ray
    unsigned long mPriority;                // +0x04 GetUnitPickPriority(unit, inArg)
    long mKind;                             // +0x08 GetUnitPickPriority(unit, false)
    unsigned long mIndex;                   // +0x0c into mEntries
    bool mExact;                            // +0x10 the exact (LOD 2) test hit
    bool mLarge;                            // +0x11 big on the screen (or kind 2)
};
#endif

class EEUnitPicker {                        // RTTI, vtable 0x441c48 (one slot)
public:
    EEUnitPicker(unsigned long inCapacity); // 0x118cf8
    virtual ~EEUnitPicker();                // +0x00 (??_G 0x118dbb; dtor 0x118e35)
    bool Contains(EEUnit* inUnit);          // 0x118e74 (provisional name)
    // 0x119540: the unit under inPoint (referenced; 0 if none) (provisional name)
    EEUnit* PickUnit(const GE2DPoint& inPoint, long inPreferResources);
    // 0x119ea2: the ids of inPlayer's units inside inRectangle (EEMSelectUnits' ctor; provisional name)
    void PickUnits(const GERectangle& inRectangle, EEPlayer* inPlayer, long inMode, std::vector<unsigned long>& outUnitIDs);
    // 0x119d63: the ids of the units inside inRectangle (the editor's lasso) (provisional name)
    void PickUnitsInRectangle(const GERectangle& inRectangle, long inArg2, std::vector<unsigned long>& outUnitIDs);
    void Clear();                           // 0x119f08: drops the entries and mCarriers (provisional name)
    void Reset();                           // 0x119f7c: Clear, the main viewport, the camera mode (provisional name)
    // 0x119ffb: one drawn unit (EEROUnits) (provisional name)
    void AddUnit(EEUnit* inUnit, const GETransformation& inTransformation, float inPixelSize);
#ifdef LLE_GEVIEWPORT_H
    // the picker's lock (the original addressed mLock as a global, gUnitPickerLock 0x529014)
    TSCriticalSection* GetLock() const { return this->mLock; }
#endif
private:
    // 0x118eaf: PickUnits' worker for one unit type (provisional name)
    void PickUnitsOfType(const GERectangle& inRectangle, EEPlayer* inPlayer, long inType,
                         std::vector<unsigned long>& outUnitIDs);
    void ReferenceCarriers(EEComplexUnit* inUnit);   // 0x119faa (provisional name)
#ifdef LLE_GEVIEWPORT_H
    std::vector<EEComplexUnit*> mCarriers;  // +0x04 (each referenced; Clear drops them)
    GEViewport mViewport;                   // +0x14 (Reset copies the main viewport 0x4e8f08)
    TSCriticalSection* mLock;               // +0x29c (named by the ctor's assert)
    EEUnitPickerEntry* mEntries;            // +0x2a0
    unsigned long mCapacity;                // +0x2a4
    unsigned long mCount;                   // +0x2a8
    long mShowHiddenUnits;                  // +0x2ac (Reset: gHiddenUnitsMode)
    bool mOrthographic;                     // +0x2b0 (Reset: gOrthographicView) (provisional name)
#endif
};

extern EEUnitPicker gUnitPicker;            // 0x528d78

#endif
