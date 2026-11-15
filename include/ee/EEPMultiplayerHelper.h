// C:\EEX\Empire Earth\Persistent\EEPMultiplayerHelper.h (RECONSTRUCTION; layout proven: EEPMultiplayerHelper sizeof 0x8
// = its new-site size, ctor 0x14ae4d exact in EEPMultiplayerHelper.cpp; member names are provisional). The saved
// multiplayer state (RTTI EEPMultiplayerHelper > FSPersistent; vtable 0x44225c, tag 8; no members: sizeof 8).
// EEPMultiplayerHelper.cpp is 0x14ac35..0x14b22d. Load/Save save the synchronous communications' tick counters
// (gCommunications, dynamic_cast to the server/client); provisional names (the keys).
// Requires lle/LowLevelEngine.h, lle/FSRestorable.h, lle/FSPersistent.h.
#ifndef EE_EEPMULTIPLAYERHELPER_H
#define EE_EEPMULTIPLAYERHELPER_H

class EEPMultiplayerHelper : public FSPersistent {
public:
    EEPMultiplayerHelper();                                                     // 0x14ae4d
    virtual ~EEPMultiplayerHelper();                                            // +0x10 0x14ae79 (??_G 0x14ae5d)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x14ae9e
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x14aff1
    virtual FSPersistent* CreateDefault() const;                                // +0x14 0x14ae87

    // class static keys (guard byte 0x531ab8, bits 1..0x80)
    static const UString sNemmMessageNumber;
    static const UString sNescCt;
    static const UString sNescPt;
    static const UString sNescommCt;
    static const UString sNescommLastTickRecvd;
    static const UString sSNessLastCt;      // "EEPMultiplayerHelper-sNessLastCt" (sic)
    static const UString sNessCt;
    static const UString sNessPt;
};

#endif
