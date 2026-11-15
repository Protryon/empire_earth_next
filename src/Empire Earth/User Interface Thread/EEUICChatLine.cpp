// C:\EEX\Empire Earth\User Interface Thread\EEUICChatLine.cpp (file name inferred: no __FILE__; RTTI class
// EEUICChatLine). One line of the in-game chat display: 0x257223..0x257881 (ctor, MouseMove, ??_G, dtor, Copy,
// CopyData, Display, DrawPolygons, DrawTextA; EEUICCommandUnits at 0x257881). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/TMPoll.h"
#include "lle/UIControl.h"
#include "lle/UIFontTable.h"
#include "lle/UIForm.h"
#include "lle/GEText.h"
#include "lle/UIControlsTable.h"
#include "lle/User Interface.h"
#include "ee/EEUICChatLine.h"

// Creates a chat display line from its controls table entry: bold name and message texts, a hidden timer
EEUICChatLine::EEUICChatLine(const GERectangle& inRectangle, UIControlsTable& inTable, UIForm* inForm)
    : EEUIRustControl(inTable.GetID(), inRectangle, inForm, inTable.GetFormIndex(), inTable.GetBorderThickness()),
      EeScreensChatLineFields()
{
    ee_screens_chat_line_init(this, inTable.GetBackgroundOpacity());
    this->SetAlwaysDirty(inTable.GetAlwaysDirty());
    this->SetBgColor(inTable.GetBackground());
    this->SetBgOpacity(this->mOpacity);
    GEFont* theFont = inTable.GetFont();
    GEText** theTexts[4] = {&this->mName, &this->mDrawnName, &this->mMessage, &this->mDrawnMessage};
    for (int i = 0; i < 4; i++) {
        *theTexts[i] = new GEText(i >= 2 ? theFont : UIFontTable::FindOrCreateFont(theFont->GetFontName(),
                                  theFont->GetFontStyle() | kFontStyleBold, theFont->GetFontQuality(), theFont->GetFontSize()));
        inTable.SetupGEText(*theTexts[i]);
        if (i < 2)
            (*theTexts[i])->SetLocation(this->GetRectangle().GetTopLeft());
    }
    this->mTimer = new TMPoll(1, 0, false);
    this->mTimer->Stop();
}

// Destroys the chat line, its texts and its timer
EEUICChatLine::~EEUICChatLine()
{
    delete this->mName;
    delete this->mDrawnName;
    delete this->mMessage;
    delete this->mDrawnMessage;
    delete this->mTimer;
}

// Shows another chat line's name, message, colour and remaining time on this line
unsigned long EEUICChatLine::Copy(const EEUICChatLine& inLine)
{
    unsigned long theTime = inLine.mTimer->GetTimeLeft();
    return this->Display(inLine.mName->GetFgColor(), inLine.mName->GetText(), inLine.mMessage->GetText(), theTime);
}

// CopyData, Display, DrawPolygons, DrawTextA: ee_screens chat_line.rs over the texts' and timer's ops below
int64_t EEUICChatLine::RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    GEText* theTexts[4] = {this->mName, this->mMessage, this->mDrawnName, this->mDrawnMessage};
    const UWideString& theMessage = *reinterpret_cast<const UWideString*>(inA);
    switch (inOp) {
    case EE_SCREENS_CHL_POLL: return this->mTimer->Poll();
    case EE_SCREENS_CHL_TIME_LEFT: return this->mTimer->GetTimeLeft();
    case EE_SCREENS_CHL_STOP: this->mTimer->Stop(); break;
    case EE_SCREENS_CHL_START:
        this->mTimer->SetTimeDelay((unsigned long)inA);
        this->mTimer->Start();
        break;
    case EE_SCREENS_CHL_COPY_TEXTS:
        *this->mDrawnName = *this->mName;
        *this->mDrawnMessage = *this->mMessage;
        break;
    case EE_SCREENS_CHL_TEXT: {
        const GERectangle& theExtent = theTexts[inA]->GetTextExtent();
        ioF[0] = theExtent.GetLeft();
        ioF[1] = theExtent.GetTop();
        ioF[2] = theExtent.GetRight();
        ioF[3] = theExtent.GetBottom();
        ioF[4] = theTexts[inA]->GetLocation().GetX();
        ioF[5] = theTexts[inA]->GetLocation().GetY();
        return theTexts[inA]->GetText().GetLength();
    }
    case EE_SCREENS_CHL_LOCATE: theTexts[inA]->SetLocation(ioF[0], ioF[1]); break;
    case EE_SCREENS_CHL_SET_NAME:
        this->mName->SetText(*reinterpret_cast<const UWideString*>(inB));
        this->mName->SetFgColor(*reinterpret_cast<const GEColor*>(inA));
        break;
    case EE_SCREENS_CHL_CUT:
        return UIGetStringCutOffIndex(theMessage, this->mMessage->GetFont(), (unsigned long)inB, true, true);
    case EE_SCREENS_CHL_SET_MESSAGE:
        if (inB >= 0) {
            UWideString theCutMessage;
            theCutMessage.CopyString((const char*)theMessage, (unsigned long)inB);
            this->mMessage->SetText(theCutMessage);
        } else
            this->mMessage->SetText(theMessage);
        break;
    case EE_SCREENS_CHL_LENGTH: return theMessage.GetLength();
    case EE_SCREENS_CHL_DRAW:
        return this->GetForm()->DrawClippedText(GERectangle(ioF[0], ioF[1], ioF[2], ioF[3]), theTexts[inA], this);
    }
    return 0;
}
