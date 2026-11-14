// C:\EEX\Empire Earth\User Interface Thread\EEUICWonNetRoomsBox.cpp (file name inferred: no __FILE__; RTTI class
// EEUICWonNetRoomsBox). The WON room list: 0x25cead..0x25e2f8 (ctor, ??_G, dtor, CopyData, DrawTextA, then the
// UIListBox<EEUICWNRBLine> instantiation and its vector/line COMDATs). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUICWonNetRoomsBox.h"

// FUNCTION: EXE 0x65cead
// Creates the WON room list
EEUICWonNetRoomsBox::EEUICWonNetRoomsBox(unsigned long inId, GEFont* inFont, const GE2DPoint& inLocation,
                                         unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                                         unsigned long inIndex, unsigned long inBorderThickness)
    : UIListBox<EEUICWNRBLine>(inId, std::vector<EEUICWNRBLine>(), inFont->GetHeight(), false,
                               GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex,
                               inBorderThickness)
{
    this->mCopiedText = new GEText(inFont);
    this->mText = new GEText(inFont);
    this->mText->SetFgColor(GEColor());
    this->mText->SetIsTransparent(true);
    this->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 0.0f));
    this->SetFocusedRectColor(GEColor(0.0f, 0.0f, 0.0f));
}

// SYNTHETIC: EXE 0x65cfd3
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x65cfef
// Destroys the room list and its texts
EEUICWonNetRoomsBox::~EEUICWonNetRoomsBox()
{
    delete this->mCopiedText;
    delete this->mText;
}

// FUNCTION: EXE 0x65d023
// Snapshots the list and its text style for drawing
long EEUICWonNetRoomsBox::CopyData()
{
    long theResult = UIListBox<EEUICWNRBLine>::CopyData();
    *this->mCopiedText = *this->mText;
    return theResult;
}

// FUNCTION: EXE 0x65d045
// Draws each room line: name, description, number of games and number of players
long EEUICWonNetRoomsBox::DrawTextA()
{
    GERectangle theRect;
    UWideString theText;
    long theResult = this->DrawScrollBarText();
    UIControl::DrawTextA();
    theResult = kErrorOK;
    for (unsigned long i = 0; i < this->GetDrawCount(); i++) {
        theRect.SetTop(this->GetCopyTextRectangle().GetTop() + (float)(this->GetLineHeight() * i));
        theRect.SetBottom(theRect.GetTop() + (float)this->GetLineHeight());
        for (unsigned long j = 0; j < 4; j++) {
            switch (j) {
            case 0:
                theRect.SetLeft(this->GetCopyTextRectangle().GetLeft() + 2.0f);
                theRect.SetRight(theRect.GetLeft() + 198.0f);
                this->mCopiedText->SetText(this->mCopyLineArray[i].mName);
                break;
            case 1:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 200.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 445.0f));
                this->mCopiedText->SetText(this->mCopyLineArray[i].mDescription);
                break;
            case 2:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 645.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 185.0f));
                sprintf(theText, "%ld", this->mCopyLineArray[i].mNumGames);
                this->mCopiedText->SetText(theText);
                break;
            case 3:
                theRect.SetLeft((long)(this->GetCopyTextRectangle().GetLeft() + 830.0f));
                theRect.SetRight((long)(theRect.GetLeft() + 100.0f));
                sprintf(theText, "%ld", this->mCopyLineArray[i].mNumPlayers);
                this->mCopiedText->SetText(theText);
                break;
            }
            this->mCopiedText->SetLocation(GE2DPoint(theRect.GetLeft(), theRect.GetTop()));
            theResult = this->GetForm()->DrawClippedText(theRect, this->mCopiedText, this);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x65d2d9
// Template code for the rooms list: UIListBox<EEUICWNRBLine>::UIListBox<EEUICWNRBLine>
// (template COMDAT)

// FUNCTION: EXE 0x65d553
// Template code for the rooms list: UIListBox<EEUICWNRBLine>::~UIListBox<EEUICWNRBLine>
// (template COMDAT)

// FUNCTION: EXE 0x65d5e7
// Template code for the rooms list: UIListBox<EEUICWNRBLine>::AppendLine
// (template COMDAT)

// FUNCTION: EXE 0x65d60d
// Template code for the rooms list: UIListBox<EEUICWNRBLine>::AppendStringArray
// (template COMDAT)

// FUNCTION: EXE 0x65d662
// Template code for the rooms list: UIListBox<EEUICWNRBLine>::CopyData
// (template COMDAT)

// FUNCTION: EXE 0x65d8be
// Template code for the rooms list: UIListBox<EEUICWNRBLine>::DeleteAllEntries
// (template COMDAT)

// FUNCTION: EXE 0x65d8ef
// Template code for the rooms list: UIListBox<EEUICWNRBLine>::DeleteLine
// (template COMDAT)

// FUNCTION: EXE 0x65d97d
// Template code for the rooms list: UIListBox<EEUICWNRBLine>::ProcessHome
// (template COMDAT)

// FUNCTION: EXE 0x65d9bc
// Template code for the rooms list: UIListBox<EEUICWNRBLine>::`scalar deleting destructor'
// (template COMDAT)

// FUNCTION: EXE 0x65d9d8
// Template code for the rooms list: EEUICWNRBLine::operator=
// (template COMDAT)

// FUNCTION: EXE 0x65da07
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::operator=
// (template COMDAT)

// FUNCTION: EXE 0x65db94
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::resize
// (template COMDAT)

// FUNCTION: EXE 0x65dbbe
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::size
// (template COMDAT)

// FUNCTION: EXE 0x65dbd4
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::push_back
// (template COMDAT)

// FUNCTION: EXE 0x65dc00
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::erase
// (template COMDAT)

// FUNCTION: EXE 0x65dc73
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::clear
// (template COMDAT)

// FUNCTION: EXE 0x65dcae
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::_Buy
// (template COMDAT)

// FUNCTION: EXE 0x65dce8
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::_Tidy
// (template COMDAT)

// FUNCTION: EXE 0x65dd20
// Template code for the rooms list: steps a line iterator forward (std::_Ptrit::operator+)
// (template COMDAT)

// FUNCTION: EXE 0x65dd35
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::resize
// (template COMDAT)

// FUNCTION: EXE 0x65ddf9
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::insert
// (template COMDAT)

// FUNCTION: EXE 0x65de53
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::erase
// (template COMDAT)

// FUNCTION: EXE 0x65debd
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::insert
// (template COMDAT)

// FUNCTION: EXE 0x65e1a2
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::_Ufill
// (template COMDAT)

// FUNCTION: EXE 0x65e1cd
// Template code for the rooms list: steps a line iterator back (std::_Ptrit::operator-)
// (template COMDAT)

// FUNCTION: EXE 0x65e1e4
// Template code for the rooms list: std::copy
// (template COMDAT)

// FUNCTION: EXE 0x65e20d
// Template code for the rooms list: std::copy
// (template COMDAT)

// FUNCTION: EXE 0x65e231
// Template code for the rooms list: std::vector<EEUICWNRBLine,A>::_Ucopy
// (template COMDAT)

// FUNCTION: EXE 0x65e25d
// Template code for the rooms list: std::fill
// (template COMDAT)

// FUNCTION: EXE 0x65e27a
// Template code for the rooms list: std::copy_backward
// (template COMDAT)

// FUNCTION: EXE 0x65e2a6
// Template code for the rooms list: std::_Destroy
// (template COMDAT)

// FUNCTION: EXE 0x65e2be
// Template code for the rooms list: std::_Construct
// (template COMDAT)

// FUNCTION: EXE 0x65e2d0
// Template code for the rooms list: EEUICWNRBLine::`scalar deleting destructor'
// (template COMDAT)
