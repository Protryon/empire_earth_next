// C:\EEX\Empire Earth\...\EEAIRectangleArea.h (RECONSTRUCTION; file path inferred). An AI map area
// (RTTI EEAIRectangleArea > EEAIArea @0; vtable 0x43a540, 12 slots). Only the base is declared: EECPILocation::Update 0x80a57 reads the
// EEAIArea serial of what EEAIContinentManager's lookup returns. Requires ee/EEAIArea.h, lle/U2DRectangle.h.
#ifndef EE_EEAIRECTANGLEAREA_H
#define EE_EEAIRECTANGLEAREA_H

class EEAIRectangleArea : public EEAIArea {
public:
    // (EEAIRectangleArea.cpp 0x2c79f..0x2cc87): ctor 0x2c79f (init list, then the vptr), ??_G 0x2cc65
    // (own vptr, then ~EEAIArea: a user-declared empty dtor)
    EEAIRectangleArea();
    virtual ~EEAIRectangleArea() {}
    // +0x20 0x2c939: EEAIArea's random point; when its tile is off the map, a random one of the tiles on the map
    virtual U2DPoint<long> GetRandomPoint();
    // +0x2c 0x2c929 (ret 0xc: two ignored dword arguments): EEAIArea::GetRandomPoint() (provisional name)
    virtual U2DPoint<long> GetBaseRandomPoint(long inArg1, long inArg2);
    // 0x2c7df: counts the rectangle's tiles per continent kind (+0x7c/+0x80/+0x84); mW88 = the highest continent id
    // seen; returns it (provisional name)
    unsigned short UpdateContinents();

    // EECPCombatManager 0x47306c walks mRect's tiles (x mLeft..mRight, y mTop..mBottom) when the count for
    // its argument (+0x7c / +0x80 / +0x84) is positive, unless mW88 is the continent asked for. (provisional names)
    // New (port): the members behind accessors (R5b.1 blocker (b))
    const U2DRectangle<long>& GetRect() const { return this->mRect; }
    void SetRect(const U2DRectangle<long>& inRect) { this->mRect = inRect; }
    long GetScenarioAreaID() const { return this->mScenarioAreaID; }
    void SetScenarioAreaID(long inID) { this->mScenarioAreaID = inID; }
    long GetNumLandTiles() const { return this->mNumLandTiles; }
    long GetNumWaterTiles() const { return this->mNumWaterTiles; }
    long GetNumSpaceTiles() const { return this->mNumSpaceTiles; }
    void SetNumLandTiles(long inCount) { this->mNumLandTiles = inCount; }
    void SetNumWaterTiles(long inCount) { this->mNumWaterTiles = inCount; }
    void SetNumSpaceTiles(long inCount) { this->mNumSpaceTiles = inCount; }
    unsigned short GetContinentID() const { return this->mContinentID; }
    unsigned short GetID() const { return this->mID; }
    void SetID(unsigned short inID) { this->mID = inID; }
    // the members in a saved game (EEAIContinentSave.cpp's writer and reader), in this order
    template <class W> void WriteFields(W& ioWriter) const
    {
        ioWriter.Rect(this->mRect);
        ioWriter.I32(this->mScenarioAreaID);
        ioWriter.I32(this->mNumLandTiles);
        ioWriter.I32(this->mNumWaterTiles);
        ioWriter.I32(this->mNumSpaceTiles);
        ioWriter.U16(this->mContinentID);
        ioWriter.U16(this->mID);
    }
    template <class R> bool ReadFields(R& ioReader)
    {
        return ioReader.Rect(this->mRect) && ioReader.I32(this->mScenarioAreaID) && ioReader.I32(this->mNumLandTiles)
            && ioReader.I32(this->mNumWaterTiles) && ioReader.I32(this->mNumSpaceTiles) && ioReader.U16(this->mContinentID)
            && ioReader.U16(this->mID);
    }

private:
    U2DRectangle<long> mRect;               // +0x68
    long mScenarioAreaID;                   // +0x78 (not read there)
    long mNumLandTiles;                     // +0x7c
    long mNumWaterTiles;                    // +0x80
    long mNumSpaceTiles;                    // +0x84
    unsigned short mContinentID;            // +0x88 a continent id (EEAIContinent::mID)
    unsigned short mID;                     // +0x8a EEAIContinentManager::mRectangleAreas index (0x1f3a0)
};

#endif
