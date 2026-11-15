// C:\EEX\Empire Earth\Player\EEPPlayerEvent.h (RECONSTRUCTION; layout proven: EEPPlayerEvent sizeof 0x24 = its new-site
// size, ctor 0x16e1a2 exact in EEPPlayerEvent.cpp; names are provisional). A timed player
// event at a map location (RTTI EEPPlayerEvent, no base; vtable 0x442b3c: the dtor, Load, the ICF-shared trivial
// RepairPointers 0x27cf38, Save; FSRestorable's slot shape without the class). While active it holds a reference on
// its tile of the terrain mesh and a mini-map mark for the players who share the line of sight. Defined by
// EEPPlayerEvent.cpp 0x16e1a2..0x16e5b9. Requires ee/EETileLocation.h.
#ifndef EE_EEPPLAYEREVENT_H
#define EE_EEPPLAYEREVENT_H

class EEPlayer;
class FSFile;

class EEPPlayerEvent {
public:
    EEPPlayerEvent(const EETileLocation& inSource, const EETileLocation& inLocation, long inType,
                   float inTimeRemaining, long inTextID);                                       // 0x16e1a2
    virtual ~EEPPlayerEvent();                                                                  // +0x00 0x16e212
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x16e326
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond) { return kErrorOK; }   // +0x08 (ICF 0x27cf38)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x16e487

    void Reset();                                           // 0x16e219
    void Deactivate(EEPlayer* inPlayer);                    // 0x16e23c
    static float GetDuration(long inType);                  // 0x16e2fd (5000 for type 2, else 15000)
    void Reactivate(EEPlayer* inPlayer);                    // 0x16e316
    void Activate(EEPlayer* inPlayer);                      // 0x16e3c3
    void Update(EEPlayer* inPlayer, float inElapsedTime);   // 0x16e53a

    EETileLocation mSource;                 // +0x04 (saved)
    EETileLocation mLocation;               // +0x0c the tile (saved)
    long mType;                             // +0x14 (1: reveals the location; saved)
    float mTimeRemaining;                   // +0x18 (saved)
    long mTextID;                           // +0x1c (= -1; saved)
    bool mActive;                           // +0x20 (saved)
};

#endif
