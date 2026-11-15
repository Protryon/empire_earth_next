// C:\EEX\Empire Earth\User Interface Thread\EEUICWonNetRoomsBox.h (RECONSTRUCTION; file name inferred). The WON room
// list (RTTI EEUICWonNetRoomsBox > UIListBox<EEUICWNRBLine>; vtable 0x44a2dc; control 6 of the room list form).
// UIListBox<EEUICWNRBLine>::GetCurrentSelection is 0xd19d5. Names are provisional.
// Requires lle/UIListBox.h.
#ifndef EE_EEUICWONNETROOMSBOX_H
#define EE_EEUICWONNETROOMSBOX_H

class GEText;

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
    EEUICWonNetRoomsBox(unsigned long inId, GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth,
                        unsigned long inHeight, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~EEUICWonNetRoomsBox();         // 0x25cfef (??_G 0x25cfd3)
    virtual long CopyData();                // +0x14 0x25d023 (the base's, then mCopiedText = *mText)
    virtual long DrawTextA();               // +0x1c 0x25d045 (four columns per visible line)
    // sizeof 0x13c (its `new`); the ctor 0x25cead news both texts (GEText(font), 0x50 bytes each; the second one's
    // colour is set to white) after the base ctor. Names are provisional
    GEText* mCopiedText;                    // +0x134
    GEText* mText;                          // +0x138
};

#endif
