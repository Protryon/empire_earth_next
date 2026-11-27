// RTSRegionTable.h (RECONSTRUCTION; file name inferred). template<T>
// RTSRegionTable: the map cut into square regions of inRegionSize tiles, a vector<T> per region (no base; sizeof 0x18).
// Instantiations: <std::pair<unsigned long, long> > vtable 0x43c424 (EEInfluenceTable's base; ctor 0xa5b91, dtor 0xa5b05,
// ??_G 0xa5b1a, the vector array's ??_E 0xa5b36: COMDATs closing EEInfluenceTable.cpp), <EEComplexUnit*> vtable
// 0x441d04 (ctor 0x11e05e, dtor 0x11e11a), <EEEffect*> vtable 0x441d30. Names are provisional.
// Requires lle/LowLevelEngineCore.h (UMCeilUnsigned), ee/EEMap.h (mSize), <vector>.
#ifndef EE_RTSREGIONTABLE_H
#define EE_RTSREGIONTABLE_H

// one region's values (thiscall Remove 0x239ee2: find + erase; COMDAT after its first user EEUnitClass::Virtual94)
template <class T>
class RTSRegion : public std::vector<T> {
public:
    // inline: the region loops read through them, so size and operator[] sit one level deeper and are refused where
    // the original calls them out of line (EEGetUnitsInRectangle 0xa509a) (provisional names)
    void Remove(const T& inValue)
    {
        for (iterator it = begin(); it != end(); ++it)
            if (*it == inValue) {
                erase(it);
                return;
            }
    }
};

template <class T>
class RTSRegionTable {
public:
    // regions per side = the ceiling of inMap->mSize / inRegionSize (fild/fidiv, the fld/fistp helper, fild qword)
    RTSRegionTable(EEMap* inMap, long inRegionSize)
        : RTSRegionTable(UMCeilUnsigned((float)inMap->mSize / inRegionSize), inRegionSize) {}
    RTSRegionTable(unsigned long inCount, long inRegionSize)   // inCount regions a side
        : mRegions(new RTSRegion<T>[inCount * inCount]), mRegionSize(inRegionSize), mRegionArea(inRegionSize * inRegionSize),
          mRegionsX(inCount), mRegionsY(inCount) {}
    virtual ~RTSRegionTable() { delete[] this->mRegions; }  // +0x00

    RTSRegion<T>* mRegions;                 // +0x04 (new[] of mRegionsX * mRegionsY vectors)
    long mRegionSize;                       // +0x08 tiles per region side (the callers divide tile coordinates by it)
    long mRegionArea;                       // +0x0c mRegionSize squared
    unsigned long mRegionsX;                // +0x10 (unsigned: `jae` in GetRegion)
    unsigned long mRegionsY;                // +0x14
};

#endif
