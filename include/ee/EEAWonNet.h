// C:\EEX\Empire Earth\Actions\EEAWonNet.h (RECONSTRUCTION; file name inferred). The WON lobby's
// UI-thread action (RTTI EEAWonNet > EEAction; vtable 0x43914c): EEWonNet.cpp posts one per lobby callback and Virtual4
// feeds the WON forms (room list 0x1f, game room 0x20). Members from the three ctors 0x530f/0x5418/0x5523 and the dtor
// 0x5611 (five strings +0x18..+0x68). sizeof >= 0x94. Names are provisional.
// Requires lle/UIListBox.h, lle/UITextLabel.h.
#ifndef EE_EEAWONNET_H
#define EE_EEAWONNET_H

#include "ee/EEAction.h"

class EEAWonNet : public EEAction {
public:
    // 0x530f (EEWonNet 0xcfdab..: type 4, the game room's player list)
    EEAWonNet(long inType, UIListBox<UWideString>* inList, const UWideString& inText, bool inHasPassword);
    // 0x5418 (EEWonNet 0xcfb27..: type 3, one game line)
    EEAWonNet(long inType, const UWideString& inText, long inHumans, long inPlayers, bool inHasPassword, bool inIsPrivate, bool inIsInProgress);
    // 0x5523 (EEWonNet 0xd05ca/0xd0c6b: types 6/7, a chat line)
    EEAWonNet(long inType, UITextLabel* inChat, const UWideString& inPrefix, const UWideString& inText, bool inIsFromMe);
    virtual ~EEAWonNet();                   // 0x5611 (??_G 0x52f3)
    virtual void Virtual4();                // 0x564a
    virtual void Virtual8();                // +0x08 (the shared empty slot 0x1720c6)

    UIListBox<UWideString>* mList;          // +0x0c (type 4)
    UITextLabel* mChat;                     // +0x10 (types 6/7)
    long mType;                             // +0x14 (Virtual4's switch: 2 rooms, 3 games, 4 list, 6/7 chat, 8 room labels)
    UWideString mPrefix;                    // +0x18 (chat: the speaker)
    UWideString mText2C;                    // +0x2c
    UWideString mRoomName;                  // +0x40 (type 2: the rooms box line's name; empty clears the box)
    UWideString mServerName;                // +0x54 (type 8: label 16)
    UWideString mText;                      // +0x68 ("@@@@" / "@@@" bracket a refill)
    char _gap7C[4];
    long mL80;                              // +0x80 (only the chat ctor clears it)
    long mHumans;                           // +0x84
    long mPlayers;                          // +0x88
    long mL8C;                              // +0x8c
    bool mHasPassword;                      // +0x90 (the games line's +0x1c)
    bool mIsFromMe;                         // +0x91 (chat: do not scroll to the end)
    bool mIsInProgress;                     // +0x92 (the games line's +0x1e)
    bool mIsPrivate;                        // +0x93 (the games line's +0x1d)
};

#endif
