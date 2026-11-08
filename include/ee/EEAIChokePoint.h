// C:\EEX\Empire Earth\...\EEAIChokePoint.h (RECONSTRUCTION; file path inferred). An AI map area
// (RTTI EEAIChokePoint > EEAIArea @0; vtable 0x439d3c, 11 slots). Only the base is declared: EECPILocation::Update 0x80a57 reads the
// EEAIArea serial of what EEAIContinentManager's lookup returns. Requires ee/EEAIArea.h.
#ifndef EE_EEAICHOKEPOINT_H
#define EE_EEAICHOKEPOINT_H

class EEAIChokePoint : public EEAIArea {
public:
    // inline ctor (COMDAT 0x1109a, first user EEAIAStarChokePointGoal::CreateChokePoint)
    // dtor 0x1c740 and AddAreaSerials 0x1c75a (EEAIChokePoint.cpp)
    EEAIChokePoint() : mIndex(0), mContinentID(0) { this->mSerial = 0; }
    virtual ~EEAIChokePoint();
    void AddAreaSerials(const std::vector<unsigned long>& inSerials);   // 0x1c75a: appended to mAreaSerials (provisional name)
    // (EEAIContinentManager::AddChokePoint 0x1f1b8): +0x78 its mChokePoints index (also
    // EEAIContinent::AddChokePoint's argument), +0x7c its continent (the mContinents index); +0x68..+0x78 not decoded
    // (the dtor 0x1c740 destroys a vector<long> at +0x68)
    unsigned long GetAreaSerialsAt(unsigned long inIndex) const { return this->mAreaSerials[inIndex]; }
    std::vector<unsigned long> mAreaSerials;       // +0x68 the serials of the areas it joins (EEAIContinentManager 0x24b31) (provisional name)
    long mIndex;                            // +0x78 (provisional name)
    unsigned short mContinentID;            // +0x7c (provisional name)
};

// EEAIChokePoint.cpp's terrain-pass arrays (.bss 0x4e5c44..0x4e5c54; one entry per tile, x * size + y;
// EEAIAStarChokePointGoal::AcceptTile 0x10fcc reads them) (provisional names)
extern long* gEEAIChokeTileKinds;           // 0x4e5c44 (0x36: a tile with a resource or an area of its own)
extern unsigned long* gEEAIChokeTileSerials;        // 0x4e5c48 (the serial of the area the tile belongs to)
extern unsigned char* gEEAIChokeTileFlags;  // 0x4e5c4c (set: a tile between two areas)
extern long gEEAIChokeWidth;                // 0x4e5c50
extern long gEEAIChokeHeight;               // 0x4e5c54

#endif
