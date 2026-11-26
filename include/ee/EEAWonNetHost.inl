#include "rust/won_lobby/wonaction.h"
// The WON front end's action's host (R6 WON lobby and network round 37): every C++ statement the Rust action
// (ee_won_lobby wonaction.rs, EEAWonNet::Virtual4 as it was) runs on a form, a label, a list box or the resource
// file, one statement each (EeWonActionOp documents them). Included by EEAWonNet.cpp and by
// tests/won/won_action37a_test.cpp (over its stand-ins), so both run these very statements. Needs, in scope: the UI
// classes, the games and rooms boxes, gUIManager, gResource and the forms' and controls' ids.
static intptr_t WonActionHost(EeWonActionOp inOp, intptr_t a, intptr_t b)
{
    typedef EeWonActionOp Op;
    UITextLabel* aLabel = static_cast<UITextLabel*>(reinterpret_cast<UIControl*>(a));
    UIListBox<UWideString>* aList = static_cast<UIListBox<UWideString>*>(reinterpret_cast<UIControl*>(a));
    EEUICWonNetGamesBox* aGames = reinterpret_cast<EEUICWonNetGamesBox*>(a);
    EEUICWonNetRoomsBox* aRooms = reinterpret_cast<EEUICWonNetRoomsBox*>(a);
    const EeWonActionLine* aLine = reinterpret_cast<const EeWonActionLine*>(b);
    switch (inOp) {
    // forms, texts
    case Op::CurrentFormIs: return gUIManager->GetCurrentForm() == gUIManager->GetFormByID(a);
    case Op::CurrentControl: return (intptr_t)gUIManager->GetCurrentForm()->GetControl(a);
    case Op::FormControl: return (intptr_t)gUIManager->GetFormByID(a)->GetControl(b);
    case Op::TextNew: return (intptr_t)new UWideString;
    case Op::TextBytes: return (intptr_t)(const char*)*reinterpret_cast<UWideString*>(a);
    case Op::TextDelete: delete reinterpret_cast<UWideString*>(a); return 0;
    case Op::ResString: gResource->GetString(a, *reinterpret_cast<UWideString*>(b)); return 0;
    // a label (the chat, the room list's totals)
    case Op::LabelSetText: aLabel->SetText(UWideString((const char*)b)); return 0;
    case Op::LabelHasScrollBar: return aLabel->GetScrollBar() != NULL;
    case Op::LabelSliderMax: return (intptr_t)aLabel->GetScrollBar()->GetSlider()->GetMaxRange();
    case Op::LabelSliderValue: return (intptr_t)aLabel->GetScrollBar()->GetSlider()->GetCurrentRangeValue();
    case Op::LabelAppendText: aLabel->AppendText(UWideString((const char*)b)); return 0;
    case Op::LabelSetLine: aLabel->SetStartingDisplayLine((unsigned long)b); return 0;
    // the game room's player list
    case Op::ListDisplayLine: return (intptr_t)aList->GetDisplayLine();
    case Op::ListDeleteAll: aList->DeleteAllEntries(); return 0;
    case Op::ListLineCount: return (intptr_t)aList->GetLineCount();
    case Op::ListSetLine: aList->SetStartingDisplayLine((unsigned long)b); return 0;
    case Op::ListAppend: aList->AppendLine(UWideString((const char*)b)); return 0;
    // the games box
    case Op::GamesBox: return (intptr_t)dynamic_cast<EEUICWonNetGamesBox*>(gUIManager->GetFormByID(kFormAvailableGames)->GetControl(kAvailableGamesAvalGamesListBox));
    case Op::GamesLineCount: return (intptr_t)aGames->GetLineCount();
    case Op::GamesDisplayLine: return (intptr_t)aGames->GetDisplayLine();
    case Op::GamesSelection: return aGames->GetCurrentSelection(*reinterpret_cast<unsigned long*>(b));
    case Op::GamesName: return (intptr_t)(const char*)aGames->GetData((unsigned long)b).mName;
    case Op::GamesDeleteAll: aGames->DeleteAllEntries(); return 0;
    case Op::GamesSetLine: aGames->SetStartingDisplayLine((unsigned long)b); return 0;
    case Op::GamesSelect: aGames->ChangeSelectionIndex((unsigned long)b, true, false); return 0;
    case Op::GamesAppend: aGames->AppendLine(EEUICWNGBLine{UWideString(aLine->name), aLine->humans, aLine->players, aLine->has_password, aLine->is_private, aLine->is_in_progress}); return 0;
    // the rooms box
    case Op::RoomsBox: return (intptr_t)dynamic_cast<EEUICWonNetRoomsBox*>(gUIManager->GetFormByID(kFormAvailableRooms)->GetControl(kAvailableRoomsAvailRoomsListBox));
    case Op::RoomsDeleteAll: aRooms->DeleteAllEntries(); return 0;
    case Op::RoomsAppend: aRooms->AppendLine(EEUICWNRBLine{UWideString(aLine->name), UWideString(aLine->description), aLine->humans, aLine->players}); return 0;
    }
    return 0;
}
