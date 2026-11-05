// The post-game summary's events (class prefix EEUIEPGSE; EEUIEPostGameSummaryEvents.cpp by __FILE__ 0x1e2a01; ProcessEvents 0x1e295f..0x1e5e2e)
// (RECONSTRUCTION from RTTI/vtables/new sites; header and file names inferred.)
// Classes in ProcessEvent address order (the .cpp's definition order); declaration order, inline ProcessEvents
// and dtors are for the unit to prove (a unit's ??_G COMDATs follow its first user in declaration order).
// Requires lle/UIEvent.h.
#ifndef EE_EEUIEPOSTGAMESUMMARYEVENTS_H
#define EE_EEUIEPOSTGAMESUMMARYEVENTS_H

// vtable 0x44b760; ??_G 0x273ca4; ProcessEvent 0x1e3305; sizeof 0xc; ctor inline; new at 0x273acb
class EEUIEPGSETabSummary : public UIEvent {
public:
    virtual ~EEUIEPGSETabSummary() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b74c; ??_G 0x273ccf; ProcessEvent 0x1e356c; sizeof 0xc; ctor inline; new at 0x273af0
class EEUIEPGSETabTimeLine : public UIEvent {
public:
    virtual ~EEUIEPGSETabTimeLine() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b738; ??_G 0x273cfa; ProcessEvent 0x1e3655; sizeof 0xc; ctor inline; new at 0x273b15
class EEUIEPGSETabMilitary : public UIEvent {
public:
    virtual ~EEUIEPGSETabMilitary() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b724; ??_G 0x273d25; ProcessEvent 0x1e372c; sizeof 0xc; ctor inline; new at 0x273b3a
class EEUIEPGSETabEconomy : public UIEvent {
public:
    virtual ~EEUIEPGSETabEconomy() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b710; ??_G 0x273d50; ProcessEvent 0x1e3803; sizeof 0xc; ctor inline; new at 0x273b5f
class EEUIEPGSETabEpoch : public UIEvent {
public:
    virtual ~EEUIEPGSETabEpoch() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b6fc; ??_G 0x273d7b; ProcessEvent 0x1e38da; sizeof 0xc; ctor inline; new at 0x273b84
class EEUIEPGSETabSociety : public UIEvent {
public:
    virtual ~EEUIEPGSETabSociety() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b6e8; ??_G 0x273da6; ProcessEvent 0x1e39b1; sizeof 0xc; ctor inline; new at 0x273ba9
class EEUIEPGSETabMisc : public UIEvent {
public:
    virtual ~EEUIEPGSETabMisc() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b6d4; ??_G 0x273dd1; ProcessEvent 0x1e3a88; sizeof 0xc; ctor inline; new at 0x273bcb
class EEUIEPGSEExit : public UIEvent {
public:
    virtual ~EEUIEPGSEExit() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b6c0; ??_G 0x273dfc; ProcessEvent 0x1e3c95; sizeof 0x10; ctor inline; new at 0x273be9
class EEUIEPGSEPlayerChecked : public UIEvent {
public:
    EEUIEPGSEPlayerChecked(long inControlID) : mControlID(inControlID) {}  // init list: the new sites store mU0C before the vptr (0x273bfd)
    virtual ~EEUIEPGSEPlayerChecked() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    long mControlID;            // +0x0c (provisional name; new sites store edi)
};

// vtable 0x44b6ac; ??_G 0x273e27; ProcessEvent 0x1e3d53; sizeof 0x10; ctor inline; new at 0x273c0a
class EEUIEPGSEPlayerUnChecked : public UIEvent {
public:
    EEUIEPGSEPlayerUnChecked(long inControlID) : mControlID(inControlID) {}  // init list: the new sites store mU0C before the vptr (0x273bfd)
    virtual ~EEUIEPGSEPlayerUnChecked() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    long mControlID;            // +0x0c (provisional name; new sites store edi)
};

// vtable 0x44b030; ??_G 0x271e30; ProcessEvent 0x1e3d6f; sizeof 0xc; ctor inline; new at 0x271b9f
class EEUIEPGSEOpen : public UIEvent {
public:
    virtual ~EEUIEPGSEOpen() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b01c; ??_G 0x271e5b; ProcessEvent 0x1e5711; sizeof 0xc; ctor inline; new at 0x271bc4
class EEUIEPGSEClose : public UIEvent {
public:
    virtual ~EEUIEPGSEClose() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b698; ??_G 0x273e52; ProcessEvent 0x1e57fa; sizeof 0xc; ctor inline; new at 0x273c2b
class EEUIEPGSENextGraph : public UIEvent {
public:
    virtual ~EEUIEPGSENextGraph() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// The two reports Open sends (NEProtocol::SendSingle) to the host and port read from a file whose name is an
// encrypted file static (struct and member names provisional): message 1000 with the
// game's figures (the file static at 0x5630f0), then one message 1001 per player (the vector at 0x5630e0, sizeof
// 0x2c: the implicit copy ctor 0x1e5cd5 and operator= 0x1e5df4 copy every member, +0x28 is not exported).
struct EEPGSGameReport {
    void ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize);   // 0x1e2a01
    unsigned long ExportSize();             // 0x1e2aa3 (20)

    long mStartTime;                        // +0x00 gWorld's statistics +0x24
    long mEndTime;                          // +0x04 gWorld's statistics +0x2c
    unsigned long mNumPlayers;              // +0x08 the number of players
    unsigned long mNumHumans;               // +0x0c the players with EEStatisticsRecord::mIsHuman
};

struct EEPGSPlayerReport {
    void ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize);   // 0x1e2aa7
    unsigned long ExportSize();             // 0x1e2ba5 (the name's bytes + 22)

    UWideString mName;                      // +0x00
    long mStartTime;                        // +0x14 gWorld's statistics +0x24
    long mUnitsLost;                        // +0x18 (0x1e4ed5: graph 4's maximum)
    long mUnitsKilled;                      // +0x1c (0x1e4ed5: graph 3's maximum)
    long mResult;                           // +0x20 EEStatisticsRecord::mResult
    bool mIsHuman;                          // +0x24 EEStatisticsRecord::mIsHuman
    long mU28;                              // +0x28
};

// Globals of other units the summary reads (provisional names; no evidence beyond these reads)
extern GEFont* gFontTahoma8;                // 0x531990 (the status bars' font; Empire Earth.cpp)
extern long& gGameResources;                 // 0x53123c (0x2ec52 compares it with 4)
// 0x1318e6 EEGetResourceLevel, the vector 0x4e8690 (EEStartingResources::sTable) and 0x1304cc
// EEGetNumberOfResourceLevels are declared in ee/EEStartingResources.h
#endif
