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
    long GetCount() const { return this->size(); }
    T& GetAt(long inIndex) { return (*this)[inIndex]; }
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
    {
        unsigned long theCount = UMCeilUnsigned((float)inMap->mSize / inRegionSize);
        this->mRegionsX = theCount;
        this->mRegionsY = theCount;
        this->mRegionSize = inRegionSize;
        this->mRegionArea = inRegionSize * inRegionSize;
        this->mRegions = new RTSRegion<T>[theCount * theCount];
    }
    virtual ~RTSRegionTable() { delete[] this->mRegions; }  // +0x00

    // inline (expanded everywhere in EEInfluenceTable.cpp; out-of-line copy 0x10ae9 takes a U2DPoint<long>): 0 off the grid
    RTSRegion<T>* GetRegion(unsigned long inX, unsigned long inY)
        { return inX < this->mRegionsX && inY < this->mRegionsY ? &this->mRegions[inX * this->mRegionsY + inY] : NULL; }
    // never expanded (COMDAT 0x10ae9, in the AI A* range: its first user's)
    RTSRegion<T>* GetRegion(const U2DPoint<long>& inTile)
    {
        return (unsigned long)inTile.mX < this->mRegionsX && (unsigned long)inTile.mY < this->mRegionsY ?
            &this->mRegions[inTile.mX * this->mRegionsY + inTile.mY] : NULL;
    }
    // inline (COMDAT 0x6136d after its first user EECPAttackSubGroup::Virtual1C4 0x460e65; ret 8, hidden result, y
    // divided first): the region holding inTile (provisional name)
    U2DPoint<long> GetRegionPoint(const U2DPoint<long>& inTile)
        { return U2DPoint<long>(inTile.mX / this->mRegionSize, inTile.mY / this->mRegionSize); }
    // inline (COMDAT 0xe06f, EEAIArea.cpp's range; EEHerd.cpp's unit collector too): inTiles' edges in regions
    // (provisional name)
    U2DRectangle<long> GetRegions(const U2DRectangle<long>& inTiles)
    {
        U2DRectangle<long> theRegions;
        theRegions.mBottom = inTiles.mBottom / this->mRegionSize;
        theRegions.mLeft = inTiles.mLeft / this->mRegionSize;
        theRegions.mRight = inTiles.mRight / this->mRegionSize;
        theRegions.mTop = inTiles.mTop / this->mRegionSize;
        return theRegions;
    }
    // 0x6bf74 (ret 0xc; COMDAT in EECPBuildManager.cpp's range; defined below the class, never expanded): the values in
    // the regions under inTiles, sorted with inLess and without duplicates (EECPBuildManager 0x67e10, EECPCombatManager
    // Update 0x47219a; the third argument is the sort predicate: its byte is pushed by value to sort 0xe181)
    // (provisional name)
    void CollectValues(const U2DRectangle<long>& inTiles, std::vector<T>& outValues, const std::less<T>& inLess);
    // 0x6c76b (ret 8): every value in the regions under inTiles, last region and last value first (provisional name)
    void GetValues(const U2DRectangle<long>& inTiles, std::vector<T>& outValues);
    // 0x6d0b5 (ret 8): the regions under inTiles (the corners through U2DRectangle's GetTopLeft / GetBottomRight)
    // (provisional name)
    void GetRegions(const U2DRectangle<long>& inTiles, std::vector<RTSRegion<T>*>& outRegions);
    void Add(unsigned long inX, unsigned long inY, const T& inValue)
    {
        RTSRegion<T>* theRegion = this->GetRegion(inX, inY);
        if (theRegion != NULL)
            theRegion->push_back(inValue);
    }

    RTSRegion<T>* mRegions;                 // +0x04 (new[] of mRegionsX * mRegionsY vectors)
    long mRegionSize;                       // +0x08 tiles per region side (the callers divide tile coordinates by it)
    long mRegionArea;                       // +0x0c mRegionSize squared
    unsigned long mRegionsX;                // +0x10 (unsigned: `jae` in GetRegion)
    unsigned long mRegionsY;                // +0x14
};

// out of line (never expanded: COMDATs 0x6bf74 / 0x6c76b / 0x6d0b5 in EECPBuildManager.cpp's range)
template <class T>
void RTSRegionTable<T>::CollectValues(const U2DRectangle<long>& inTiles, std::vector<T>& outValues, const std::less<T>& inLess)
{
    this->GetValues(inTiles, outValues);
    std::sort(outValues.begin(), outValues.end(), inLess);
    std::vector<T>::iterator it = std::unique(outValues.begin(), outValues.end());
    if (it != outValues.end())
        outValues.erase(it, outValues.end());
}
template <class T>
void RTSRegionTable<T>::GetValues(const U2DRectangle<long>& inTiles, std::vector<T>& outValues)
{
    std::vector<RTSRegion<T>*> theRegions;
    outValues.clear();
    this->GetRegions(inTiles, theRegions);
    for (long i = theRegions.size() - 1; i >= 0; i--) {
        RTSRegion<T>* theRegion = theRegions[i];
        for (long j = theRegion->size() - 1; j >= 0; j--)
            outValues.push_back((*theRegion)[j]);
    }
}
template <class T>
void RTSRegionTable<T>::GetRegions(const U2DRectangle<long>& inTiles, std::vector<RTSRegion<T>*>& outRegions)
{
    long theMinX = inTiles.GetTopLeft().mX / this->mRegionSize;
    long theMinY = inTiles.GetTopLeft().mY / this->mRegionSize;
    long theMaxX = inTiles.GetBottomRight().mX / this->mRegionSize;
    long theMaxY = inTiles.GetBottomRight().mY / this->mRegionSize;
    if (theMinX <= 0)
        theMinX = 0;
    if (theMinY <= 0)
        theMinY = 0;
    long theCount = (theMaxY - theMinY) * (theMaxX - theMinX);
    if (theCount > 0)
        outRegions.reserve(theCount);
    for (long x = theMinX; x <= theMaxX; x++) {
        for (long y = theMinY; y <= theMaxY; y++) {
            RTSRegion<T>* theRegion = this->GetRegion(x, y);
            if (theRegion)
                outRegions.push_back(theRegion);
        }
    }
}

#endif
