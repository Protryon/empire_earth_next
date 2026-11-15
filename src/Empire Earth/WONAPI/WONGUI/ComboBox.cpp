// ComboBox (R6 WON round 28): the Rust ComboBox's calls out (rust/crates/ee_won/src/comboobj.rs, EeWonGuiOp's combo
// ops; the container's fall back to Container.cpp's), its constructor, destructor and list box setter. The original's
// ComboBox.cpp (the list box's size and popup, the selection, the keys, the events, no-typing, enabling) is the Rust
// object; each op here is a run of its C++ statements with no decision between them.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_won.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

static Component* Comp(intptr_t a) { return (Component*)a; }
static TextListItem* Item(intptr_t a) { return (TextListItem*)a; }

extern "C" intptr_t ee_won_guicpp_combo_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    ComboBox* t = static_cast<ComboBox*>((Component*)inComp);
    switch (op) {
    case Op::ComboPart:
        switch (a) {
        case 0: return (intptr_t)static_cast<Component*>(t->mEdit.get());
        case 1: return (intptr_t)static_cast<Component*>(t->mButton.get());
        case 2: return (intptr_t)static_cast<Component*>(t->mListBox.get());
        default: return (intptr_t)static_cast<Component*>(t->mListArea.get());
        }
    case Op::ComboInsertString: return (intptr_t)t->mListArea->InsertString(*(const GUIString*)a, (int)b);
    case Op::ComboInsertItem: t->mListArea->InsertItem(Item(a), (int)b); return 0;
    case Op::ComboDropItem: { SmartPtr<TextListItem> held = Item(a); return 0; }
    case Op::ComboDeleteItem: t->mListArea->DeleteItem((int)a); return 0;
    case Op::ComboRefreshItem: t->mListArea->RefreshItem(Item(a)); return 0;
    case Op::ComboListClear: t->mListArea->Clear(true); return 0;
    case Op::ComboSelItem: return (intptr_t)t->mListArea->GetSelItem();
    case Op::ComboItemPos: return t->mListArea->GetItemPos((ListItem*)a);
    case Op::ComboSetSel: t->mListArea->SetSelItem((int)a, b != 0); return 0;
    case Op::ComboSetListBox: t->mListBox = static_cast<Scroller*>(Comp(a)); return 0;
    case Op::ComboBoxArea: return (intptr_t)static_cast<Component*>(static_cast<Scroller*>(Comp(a))->GetScrollArea());
    case Op::ComboSetScroller: t->mListArea->SetScroller(t->mListBox.get()); return 0;
    case Op::ComboSetListFlags: t->mListArea->SetListFlags((int)a, b != 0); return 0;
    case Op::ComboListMetric:
        switch (a) {
        case 0: return t->mListArea->GetRightPad();
        case 1: return t->mListArea->GetLeftPad();
        case 2: return t->mListArea->GetTotalWidth();
        default: return t->mListArea->GetTotalHeight();
        }
    case Op::ComboEditClear: t->mEdit->SetText(GUIString(new GUIStringData("")), a != 0); return 0;
    case Op::ComboEditItemText: t->mEdit->SetText(Item(a)->mText, true); return 0;
    case Op::ComboEditSetText: t->mEdit->SetText(*(const GUIString*)a, b != 0); return 0;
    case Op::ComboEditUndo: t->mEdit->SetText(t->mUndoStr, true); return 0;
    case Op::ComboUndoSave: t->mUndoStr = t->mEdit->GetText(); return 0;
    case Op::ComboEditSelect: t->mEdit->SelectRange((int)a, (int)b); return 0;
    case Op::ComboEditAllowSelection: t->mEdit->SetAllowSelection(a != 0); return 0;
    case Op::ComboItemWidth: return Item(a)->mWidth;
    case Op::ComboItemTextLength: return Item(a)->mText.length();
    case Op::ComboDeliverSel: t->DeliverEvent(new ComboSelChangedEvent(t, Item(a), b != 0), false); return 0;
    case Op::ComboItemCount: return t->mListArea->GetNumItems();
    case Op::ComboItemAt: return (intptr_t)t->mListArea->GetItem((int)a);
    case Op::ComboItemTextIs:
        if (b == 0) return Item(a)->mText == t->mEdit->GetText() ? 1 : 0;
        return Item(a)->mText == *(const GUIString*)b ? 1 : 0;
    case Op::ComboOpenPopup: {
        Window* window = t->GetRoot();
        int x = 0, y = t->Height();
        t->LocalToRoot(x, y);
        PopupParams params(t->mListBox.get(), 0x2347, t, x, y);
        params.mInputComponent = t; // input component (+0x08)
        params.mAvoidRect.y = y - t->Height();
        params.mAvoidRect.height = t->Height();
        window->OpenPopup(params);
        return 0;
    }
    case Op::ComboEnsureVisible: t->mListArea->EnsureVisible((ListItem*)a); return 0;
    case Op::ComboEventButton: return static_cast<ListItemClickedEvent*>((ComponentEvent*)a)->mButton;
    case Op::ComboRelayout: t->Relayout(); return 0;
    case Op::ComboUnlisten: t->mListBox->AddListener(NULL); return 0;
    case Op::ClosePopup: t->GetRoot()->ClosePopup(Comp(a)); return 0;
    case Op::ListenTo: Comp(a)->AddListener(new ComponentListenerComp(t)); return 0;
    case Op::DlgClick: static_cast<Button*>(Comp(a))->Fn2F3440(); return 0;
    case Op::ButtonSetFlags: static_cast<Button*>(Comp(a))->SetButtonFlags((int)b, c != 0); return 0;
    case Op::ChildSetEnabled: Comp(a)->SetEnabled(b != 0); return 0;
    case Op::ChildSetVisible: Comp(a)->SetVisible(b != 0); return 0;
    case Op::ChildSetSize: Comp(a)->SetSize((int)b, (int)c); return 0;
    case Op::BaseSetEnabled: t->Component::SetEnabled(a != 0); return 0;
    case Op::EventType: return ((ComponentEvent*)a)->mType;
    case Op::EventComponent: return (intptr_t)((ComponentEvent*)a)->mComponent;
    case Op::ComponentHandleEvent: t->Component::HandleComponentEvent((ComponentEvent*)a); return 0;
    // the type is Rust's map, which the golden checks against the enum
    case Op::FireEvent: t->Fn2F5380(ee_won_gui_event_type((EeWonGuiEvent)a), b != 0); return 0;
    default: return ContainerHostCall(t, op, a, b, c, d);
    }
}

namespace WONAPI {
// FUNCTION: WON 0x006f3970
// Creates a combo box that takes focus and passes input to its children (the Rust combo box: the list hidden, a
// 200 x 150 list box)
ComboBox::ComboBox()
{
    this->mCombo=ee_won_combo_new(static_cast<Component*>(this),ee_won_guicpp_combo_call,this->mCt);
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
    this->SetOnlyChildInput(true); // byte at +0x7c
}
// FUNCTION: WON 0x006f3a50
// Destroys the combo box, first detaching it from its list box's listeners (the Rust combo box)
ComboBox::~ComboBox() { ee_won_combo_free(this->mCombo); }
// FUNCTION: WON 0x006f3e70
// Sets the popup list box and attaches the list area to it if it is not already its scroll area
void ComboBox::SetListBox(Scroller* inListBox)
{
    this->BCall(EeWonComboCall::SetListBox,(intptr_t)static_cast<Component*>(inListBox));
}
}
