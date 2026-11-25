// C:\EEX\Empire Earth\User Interface Thread\EEUICWonNetRoomsBox.h (RECONSTRUCTION; file name inferred). The WON room
// list (RTTI EEUICWonNetRoomsBox > UIListBox<EEUICWNRBLine>; vtable 0x44a2dc; control 6 of the room list form).
// UIListBox<EEUICWNRBLine>::GetCurrentSelection is 0xd19d5. Names are provisional.
// Requires lle/UIListBox.h.
#ifndef EE_EEUICWONNETROOMSBOX_H
#define EE_EEUICWONNETROOMSBOX_H

#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/GERectangle.h"
#include "lle/UIListBox.h"
#include "rust/won_lobby/wonfront.h"

#include "lle/LLEForward.h"

// one line of the rooms box (sizeof 0x30: GetCurrentSelection divides by 0x30)
struct EEUICWNRBLine {
    UWideString mName;                      // +0x00 (joining passes it to EEWonNet::JoinRoom)
    // decoded from EEAWonNet::Virtual4 0x564a (a frame local: two string ctors/dtors, then the action's fields)
    UWideString mDescription;               // +0x14 (the action's mText)
    long mNumPlayers;                       // +0x28 (the action's mL84)
    long mNumGames;                         // +0x2c (the action's mL8C)
};

class EEUICWonNetRoomsBox : public UIListBox<EEUICWNRBLine> {
public:
    // 0x25cead (ret 0x20): UIListBox(inId, an empty line vector, the font's height, no auto-select, the rectangle
    // at inLocation of (inWidth - 1) x (inHeight - 1), ...), then the two texts
    // (R6 WON round 32: the bodies are Rust, ee_won_lobby wonfront.rs, over the screens' host ee/EEWonFrontHost.inl)
    EEUICWonNetRoomsBox(unsigned long inId, GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth,
                        unsigned long inHeight, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : UIListBox<EEUICWNRBLine>(inId, std::vector<EEUICWNRBLine>(), inFont->GetHeight(), false,
                                   GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex, inBorderThickness)
    {
        ee_won_front_call(EeWonFrontCall::RoomsConstruct, this, (intptr_t)inFont);
    }
    virtual ~EEUICWonNetRoomsBox() { ee_won_front_call(EeWonFrontCall::RoomsDestruct, this, 0); }
    // +0x14 0x25d023 (the base's, then mCopiedText = *mText)
    virtual long CopyData() { return (long)ee_won_front_call(EeWonFrontCall::RoomsCopyData, this, 0); }
    // +0x1c 0x25d045: four columns per visible line: name, description, number of games and number of players
    virtual long DrawTextA() { return (long)ee_won_front_call(EeWonFrontCall::RoomsDrawText, this, 0); }
    // sizeof 0x13c (its `new`); the ctor 0x25cead news both texts (GEText(font), 0x50 bytes each; the second one's
    // colour is set to white) after the base ctor. Names are provisional
    // the screens' host's reads of the protected base (R6 WON round 32: ee/EEWonFrontHost.inl)
    long FrontScrollBarText() { return this->DrawScrollBarText(); }
    const GERectangle& FrontCopyRect() const { return this->GetCopyTextRectangle(); }
    unsigned long FrontDrawCount() const { return this->GetDrawCount(); }
    unsigned long FrontLineHeight() const { return this->GetLineHeight(); }
    unsigned long FrontCopyCount() const { return (unsigned long)this->mCopyLineArray.size(); }
    const EEUICWNRBLine& FrontCopyLine(unsigned long inIndex) const { return this->mCopyLineArray[inIndex]; }
    void FrontClearCopiedSelection() { this->SetCopiedHasSelection(false); }
    GEText* mCopiedText;                    // +0x134
    GEText* mText;                          // +0x138
};

#endif
