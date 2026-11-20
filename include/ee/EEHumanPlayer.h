// C:\EEX\Empire Earth\AI Thread\EEHumanPlayer.h (RECONSTRUCTION; file name inferred). A human player (RTTI
// EEHumanPlayer > EEPlayer > RTSPlayer; vtable 0x43c330; sizeof 0xb94: EEPPlayerHelper's CreatePlayer 0x14b629 news
// 0xb94 bytes; EEHumanPlayer.cpp 0xa543d..0xa5589). Both ctors build the UWideString at
// +0xb80, assign it "" and store dynamic_cast<EEHumanPlayer*>(this) in EEPlayer's mHumanPlayer (+0x9a8). GetName/SetName
// (+0xdc/+0xe0) are inline (emitted with the vtable after the first ctor, before the ??_G); Load/RepairPointers/Save
// forward to EEPlayer's. Names are provisional. Requires ee/EEPlayer.h.
#ifndef EE_EEHUMANPLAYER_H
#define EE_EEHUMANPLAYER_H

class EEHumanPlayer : public EEPlayer {
public:
    // an empty name, its human-player pointer itself (EEHumanPlayer.cpp went in the game's leaves round 2; its Load,
    // RepairPointers and Save overrides, +0x18/+0x20/+0x24, were EEPlayer's own and went with it)
    EEHumanPlayer() { this->mHumanName = ""; this->mHumanPlayer = this; }
    // 0xa54fe (called from 0x1306f2): a named human player with a player id
    EEHumanPlayer(const UWideString& inName, unsigned long inPlayerID) : EEPlayer(inName, inPlayerID)
        { this->mHumanName = ""; this->mHumanPlayer = this; }
    virtual const UWideString& GetName() { return this->mHumanName; }    // +0xdc 0xa54b4
    virtual void SetName(const UWideString& inName) { this->mHumanName = inName; }  // +0xe0 0xa54bb
    virtual bool IsHuman() { return true; }     // +0x74 (??_7EEHumanPlayer slot 29 = 0x418aa9 `mov al,1; ret`, ICF; EEPlayer/EEComputerPlayer: 0x4ac9f3 false)
    virtual ~EEHumanPlayer() {}             // +0x00 (user-declared: the ??_G 0xa54ce stores the vptr first)

    UWideString mHumanName;                 // +0xb80 (the ctors assign it "" (0x4e5b98))
};

#endif
