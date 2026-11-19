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

// Creates a chat display line from its controls table entry: bold name and message texts, a hidden timer (the
// record holds them by address: hud_ctl_ffi.rs ChatCpp works them)
EEUICChatLine::EEUICChatLine(const GERectangle& inRectangle, UIControlsTable& inTable, UIForm* inForm)
    : EEUIRustControl(inTable.GetID(), inRectangle, inForm, inTable.GetFormIndex(), inTable.GetBorderThickness()),
      EeScreensChatLineFields()
{
    ee_screens_chat_line_init(this, static_cast<EEUIRustControl*>(this), inTable.GetBackgroundOpacity());
    this->SetAlwaysDirty(inTable.GetAlwaysDirty());
    this->SetBgColor(inTable.GetBackground());
    this->SetBgOpacity(this->mOpacity);
    GEFont* theFont = inTable.GetFont();
    void** theTexts[4] = {&this->mName, &this->mDrawnName, &this->mMessage, &this->mDrawnMessage};
    for (int i = 0; i < 4; i++) {
        GEText* theText = new GEText(i >= 2 ? theFont : UIFontTable::FindOrCreateFont(theFont->GetFontName(),
                                     theFont->GetFontStyle() | kFontStyleBold, theFont->GetFontQuality(), theFont->GetFontSize()));
        inTable.SetupGEText(theText);
        if (i < 2)
            theText->SetLocation(this->GetRectangle().GetTopLeft());
        *theTexts[i] = theText;
    }
    TMPoll* theTimer = new TMPoll(1, 0, false);
    theTimer->Stop();
    this->mTimer = theTimer;
}

// Destroys the chat line, its texts and its timer
EEUICChatLine::~EEUICChatLine()
{
    delete static_cast<GEText*>(this->mName);
    delete static_cast<GEText*>(this->mDrawnName);
    delete static_cast<GEText*>(this->mMessage);
    delete static_cast<GEText*>(this->mDrawnMessage);
    delete static_cast<TMPoll*>(this->mTimer);
}

// CopyData, Display, DrawPolygons, DrawTextA: ee_screens chat_line.rs over hud_ctl_ffi.rs ChatCpp (the timer and the
// texts); the message's cut is the toolkit's
int64_t EEUICChatLine::RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    if (inOp != EE_SCREENS_CHL_CUT)
        return 0;
    return UIGetStringCutOffIndex(*reinterpret_cast<const UWideString*>(inA), static_cast<GEText*>(this->mMessage)->GetFont(),
                                  (unsigned long)inB, true, true);
}
