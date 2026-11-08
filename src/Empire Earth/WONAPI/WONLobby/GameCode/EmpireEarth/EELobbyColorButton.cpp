// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyColorButton.cpp (file name inferred: no __FILE__;
// it sorts before EELobbyConnectDialog.cpp, and .data opens with the colour table before the two classes' type
// descriptors). A player's colour button in the game room and the 4 x 4 colour chooser popup it opens; the lobby's
// sixteen player colours: 0x690578..0x6909a1 (the first lobby unit: Component's and Button's inline virtuals are its
// COMDATs).
#include <windows.h>
#include <string>
#include <list>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"

using namespace WONAPI;

// 0x8d01f4 (filled by EELobbyInit.cpp from the game's player colours, 0x00rrggbb)
unsigned long gEELobbyColors[16] = {
    0x0000ff, 0xff0000, 0xffff00, 0xff00ff, 0xff8800, 0x008800, 0x00ffff, 0x880000,
    0x888888, 0x000088, 0xbbbbbb, 0xdddddd, 0xffff88, 0x88ff88, 0x8800ff, 0xff8888,
};

namespace WONAPI {

// the colour chooser popup: 4 x 4 swatches of 20 pixels, 28 apart, 8 from the edge (0x48 bytes; vtable 0x84c140)
class EELobbyColorChooser : public Component {
public:
    EELobbyColorChooser();
    virtual ~EELobbyColorChooser();
    virtual void GetDesiredSize(int& outWidth, int& outHeight);
    virtual void Paint(Graphics& inGraphics);
    virtual void MouseEnter(int inA, int inB);
    virtual void MouseExit();
    virtual void MouseDown(int inA, int inB, int inC);
    virtual void MouseMove(int inX, int inY);
    virtual void AddedToParent();

    int mHover;                             // +0x30 (the swatch under the mouse, -1)
    int mSelected;                          // +0x34 (the chosen one, -1)
    bool mTaken[16];                        // +0x38 (another player's colour)
};

}

// FUNCTION: EXE 0x690578
// Creates a colour button showing the first player colour
EELobbyColorButton::EELobbyColorButton()
{
    this->mColor = 0;
    this->SetButtonFlags(0x40, true);
}

// FUNCTION: EXE 0x6905ca
// Destroys the colour button (members only)
EELobbyColorButton::~EELobbyColorButton()
{
}

// FUNCTION: EXE 0x6905e8
// Shows another of the sixteen player colours; returns whether the colour changed
bool EELobbyColorButton::SetColor(int inColor)
{
    if (this->mColor != inColor && inColor >= 0 && inColor < 16) {
        this->mColor = inColor;
        this->Invalidate();
        return true;
    }
    return false;
}

// FUNCTION: EXE 0x69060b
// Fills the button with its player colour and frames it unless disabled
void EELobbyColorButton::Paint(Graphics& inGraphics)
{
    Component::Paint(inGraphics);
    inGraphics.SetColor(gEELobbyColors[this->mColor]);
    inGraphics.FillRect(0, 0, this->Width(), this->Height());
    if (!this->IsDisabled())
        LobbyDraw3DFrame(inGraphics, 0, 0, this->Width(), this->Height(), mButtonFlag5);
}

// FUNCTION: EXE 0x690669
// Opens the colour chooser popup centred below the button, or closes it when it is already open
bool EELobbyColorButton::HandleActivate()
{
    if (this->mChooser.mPtr == NULL) {
        this->mChooser = new EELobbyColorChooser;
        this->mChooser->SetDesiredSize(0, 0);
        this->mChooser->AddListener(new ComponentListenerComp(this));
    } else if (this->mChooser->mParent != NULL) {
        this->GetRoot()->ClosePopup(this->mChooser.mPtr);
        return false;
    }
    this->mChooser->mSelected = -1;
    int theX = (this->Width() - this->mChooser->Width()) / 2;
    int theY = this->Height();
    this->LocalToRoot(theX, theY);
    PopupParams theParams(this->mChooser.mPtr, 0x23e7, this, theX, theY);
    theParams.mAvoidRect.height = this->Height();
    theParams.mAvoidRect.y = theY - this->Height();
    theParams.mInputComponent = this;
    this->GetRoot()->OpenPopup(theParams);
    return false;
}

// FUNCTION: EXE 0x690751
// Takes the colour picked in the closed chooser and reports a change to the player rows
void EELobbyColorButton::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType == ComponentEvent_EndPopup) {
        if (this->SetColor(this->mChooser->mSelected))
            this->Fn2F5380(100064, 0);
    } else
        Button::HandleComponentEvent(inEvent);
}

// FUNCTION: EXE 0x690789
// Creates the colour chooser with no swatch hovered or chosen
EELobbyColorChooser::EELobbyColorChooser() : mHover(-1), mSelected(-1)
{
}

// FUNCTION: EXE 0x6907c2
// Destroys the colour chooser (members only)
EELobbyColorChooser::~EELobbyColorChooser()
{
}

// FUNCTION: EXE 0x6907cd
// Clears the hover and marks the colours other players in the lobby game already use
void EELobbyColorChooser::AddedToParent()
{
    Component::AddedToParent();
    this->mHover = -1;
    EELobbyGame* theGame = static_cast<EELobbyGame*>(GetMyLobbyGame());
    if (theGame != NULL)
        theGame->GetTakenColors(this->mTaken, -1);
}

// FUNCTION: EXE 0x6907f1
// Passes the mouse entering to the base component
void EELobbyColorChooser::MouseEnter(int inA, int inB)
{
    Component::MouseEnter(inA, inB);
}

// FUNCTION: EXE 0x690801
// Highlights the swatch under the mouse, none in the gaps between swatches
void EELobbyColorChooser::MouseMove(int inX, int inY)
{
    bool theOutside = false;
    inX -= 8;
    if (inX < 0 || inX % 28 >= 20)
        theOutside = true;
    else
        inX /= 28;
    inY -= 8;
    if (inY < 0 || inY % 28 >= 20)
        theOutside = true;
    else
        inY /= 28;
    int theHover = theOutside ? -1 : inX + inY * 4;
    if (theHover != this->mHover) {
        this->mHover = theHover;
        this->Invalidate();
    }
}

// FUNCTION: EXE 0x69086e
// Chooses the hovered colour and closes the popup, unless the colour is taken
void EELobbyColorChooser::MouseDown(int inA, int inB, int inC)
{
    if (this->mHover >= 0 && this->mHover < 16 && !this->mTaken[this->mHover]) {
        this->mSelected = this->mHover;
        this->GetRoot()->ClosePopup(this);
    }
}

// FUNCTION: EXE 0x690894
// Clears the hovered swatch when the mouse leaves
void EELobbyColorChooser::MouseExit()
{
    Component::MouseExit();
    if (this->mHover != -1) {
        this->mHover = -1;
        this->Invalidate();
    }
}

// FUNCTION: EXE 0x6908af
// Returns the chooser's fixed size of 120 x 120 pixels
void EELobbyColorChooser::GetDesiredSize(int& outWidth, int& outHeight)
{
    outWidth = 120;
    outHeight = 120;
}

// FUNCTION: EXE 0x6908c1
// Draws the 4 x 4 swatches of the colours still free, framing the hovered one
void EELobbyColorChooser::Paint(Graphics& inGraphics)
{
    Component::Paint(inGraphics);
    inGraphics.SetSysColor(StandardColor_MenuBack);
    inGraphics.FillRect(0, 0, this->Width(), this->Height());
    LobbyDrawFrame(inGraphics, 0, 0, this->Width(), this->Height(), false);
    int theIndex = 0;
    int theY = 8;
    for (int theRow = 0; theRow < 4; theRow++) {
        int theX = 8;
        for (int theColumn = 0; theColumn < 4; theColumn++) {
            if (!this->mTaken[theIndex]) {
                inGraphics.SetColor(gEELobbyColors[theIndex]);
                inGraphics.FillRect(theX, theY, 20, 20);
                inGraphics.SetColor(0x888888);
                inGraphics.DrawRect(theX, theY, 20, 20);
                if (theIndex == this->mHover)
                    LobbyDraw3DFrame(inGraphics, theX, theY, 20, 20, false);
            }
            theX += 28;
            theIndex++;
        }
        theY += 28;
    }
}

// The unit's compiler-generated functions:
// SYNTHETIC: EXE 0x69059d
// (Component::MouseWheel, inline; ICF also Virtual50/54/58)
// SYNTHETIC: EXE 0x6905a2
// (Component::GetMaxWidth, inline; ICF also GetMaxHeight)
// SYNTHETIC: EXE 0x6905a8
// (Button::GetText, inline)
// SYNTHETIC: EXE 0x6905ae
// (??_G EELobbyColorButton)
// (0x6907a3 Component::MouseUp, inline, is ICF-shared with game code: named there)
// SYNTHETIC: EXE 0x6907a6
// (??_G EELobbyColorChooser)

// The ICF-shared Component::MouseUp copy LINK kept in this unit's range:
// SYNTHETIC: EXE 0x6907a3
