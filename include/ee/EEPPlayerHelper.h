// C:\EEX\Empire Earth\Persistent\EEPPlayerHelper.h (RECONSTRUCTION; layout proven: EEPPlayerHelper sizeof 0x24 = its
// new-site size, ctor 0x14b339 exact in EEPPlayerHelper.cpp; member names are provisional). The saved players
// (RTTI EEPPlayerHelper > FSPersistent; vtable 0x442288, tag 5). EEPPlayerHelper.cpp is 0x14b22d..0x14b7a5
// (__FILE__ 0x4c17c0). sizeof 0x24 (CreateDefault's `new`). Names are provisional (the checksum key
// names mLocalPlayerIndex).
// Requires lle/LowLevelEngine.h, lle/FSRestorable.h, lle/FSPersistent.h, <vector>.
#ifndef EE_EEPPLAYERHELPER_H
#define EE_EEPPLAYERHELPER_H

class EEPlayer;
class EEWorld;
class EEGameState;

class EEPPlayerHelper : public FSPersistent {
public:
    EEPPlayerHelper();                                                          // 0x14b339
    EEPPlayerHelper(EEWorld& inWorld);                                          // 0x14b3a6
    // implicit virtual dtor: ??_G +0x10 0x14b380
    virtual void GetChecksum(ESChecksum& ioChecksum) const;                     // +0x00 0x14b446
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x14b4e1
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x14b6d0
    virtual FSPersistent* CreateDefault() const;                                // +0x14 0x14b42f
    long GetLocalPlayerIndex(long& outIndex);                                   // 0x14b4a3 (13 until loaded)
    long GetPlayers(std::vector<EEPlayer*>& outPlayers);                        // 0x14b4c0 (13 until loaded)
    // empty (ret 4), ICF-folded at 0x157269; the loader 0x14a98d calls it with its EEGameState (provisional name)
    void Fn157269(EEGameState* inState);
    EEPlayer* GetPlayer(unsigned long inIndex) const { return this->mPlayers[inIndex]; }  // (Save: the const operator[])

    // class static keys (guard byte 0x531b00, bits 1..8)
    static const UString sPlayerCount;
    static const UString sPlayerNum;
    static const UString sPlayerCode;
    static const UString sScenarioPlayerIndex;

    EEPlayer* GetPlayersAt(unsigned long inIndex) const { return this->mPlayers[inIndex]; }
    std::vector<EEPlayer*> mPlayers;        // +0x08
    unsigned long mLocalPlayerIndex;        // +0x18 (EEWorld +0xb84 mCurrentPlayer; Read/Write(unsigned long&))
    unsigned long mMaxPlayers;              // +0x1c (3; EEWorld +0xb8c; Read/Write(unsigned long&))
    bool mIsForLoading;                     // +0x20 (true; false for the world ctor: Load only reads when set)
    bool mLoaded;                           // +0x21 (Load sets it per player read)
};

#endif
