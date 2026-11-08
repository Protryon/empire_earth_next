// C:\EEX\Empire Earth\Actions\EEABootClientFromCanceledGame.h (RECONSTRUCTION; file name inferred). The
// action a client posts when the host cancelled the game (RTTI EEABootClientFromCanceledGame > EEAction; vtable
// 0x43d0b4, sizeof 0x14): ctor inline (EEClient.cpp 0xb729e: new 0x14, +8/+0xc/+0x10 = 0, the count, the vptr), ??_G the
// shared EEAction one, Virtual4 0x16c0, Virtual8 the shared empty slot. Names are provisional. Requires ee/EEAction.h.
#ifndef EE_EEABOOTCLIENTFROMCANCELEDGAME_H
#define EE_EEABOOTCLIENTFROMCANCELEDGAME_H

class EEABootClientFromCanceledGame : public EEAction {
public:
    EEABootClientFromCanceledGame() : mStringID(0), mNoPop(false) {}
    // EEClient::ClientSlot4C 0xb7fa5 news one with 0x271b; HandleMessage 0xb79b1: both stored after the vptr, the string id first
    EEABootClientFromCanceledGame(long inStringID) { this->mStringID = inStringID; this->mNoPop = false; }
    // EEUserInterface 0x277231 expands one with both stored after the vptr (mNoPop first)
    EEABootClientFromCanceledGame(long inStringID, bool inNoPop) { this->mNoPop = inNoPop; this->mStringID = inStringID; }
    virtual void Virtual4();                // +0x04 (0x16c0)
    virtual void Virtual8();                // +0x08 (the shared empty slot 0x1720c6)

    long mStringID;                         // +0x0c the message's resource string (0: none)
    bool mNoPop;                            // +0x10 set: keep the current form
};

#endif
