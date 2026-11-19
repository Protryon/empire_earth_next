// C:\EEX\Empire Earth\User Interface Thread\EEUICChatLine.h (RECONSTRUCTION; file name inferred). One line of the
// in-game chat display: a bold sender name and the message, each drawn from a copy, faded out by a timer (RTTI
// EEUICChatLine > UIControl; vtable 0x449df8; sizeof 0xd0, the user-defined control factory 0x27496f; EEUICChatLine.cpp
// 0x257223..0x257881). Whole in Rust (ee_screens chat_line.rs, round 29): the text rectangle, opacity, shadow offset
// and showing flag and the texts and timer it owns (by address) are the record EeScreensChatLineFields. Requires lle/UIControl.h,
// lle/UIControlsTable.h, lle/GEText.h, lle/TMPoll.h.
#ifndef EE_EEUICCHATLINE_H
#define EE_EEUICCHATLINE_H

#include "ee/EEUIRustControl.h"

class EEUICChatLine : public EEUIRustControl, protected EeScreensChatLineFields {
public:
    EEUICChatLine(const GERectangle& inRectangle, UIControlsTable& inTable, UIForm* inForm);
    virtual ~EEUICChatLine();
    virtual long CopyData() { return ee_screens_chat_line_copy_data(this, this->Host()); }
    virtual long DrawPolygons() { return ee_screens_chat_line_draw(this, this->Host()); }
    virtual long DrawTextA() { return ee_screens_chat_line_draw_text(this, this->Host()); }
};

#endif
