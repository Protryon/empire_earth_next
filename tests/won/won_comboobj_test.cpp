// ComboBox as a whole object (R6 WON round 28): the original ComboBox.cpp, the statements copied from the C++ before
// the port with every call on a C++ object (the parts: the input box, the button, the list box and its list area; the
// popup, the event, the combo box's own virtuals) replaced by the host op that makes it (ComboBox.cpp's host runs the
// same calls in the same order) and every Container::X call made on the combo box's real Rust container, against
// ee_won's Rust ComboBox (comboobj.rs). The arithmetic is guictl's (ee_won_gui_combo_*, checked against the original
// by won_guictl_test).
//
// The world is the container goldens' (won_ct_world.h): component 1 the combo box, 2 its parent (both Rust containers
// on each side), 5 the input box, 6 the button, 7 and 9 list boxes, 8 the list area, the rest leaves. The parts'
// state is a model in this test: the list's items (their texts and widths) and selection, the input box's text, the
// undo text, the list box and the list boxes' scroll areas. Every combo op is traced with its answer (the model's
// reads included), so the Rust replay needs no model. A component's SetSize, SetEnabled and SetVisible run Component's
// statements in the world. Texts are small numbers (1..4; 0 the empty text), passed where the C++ passes a GUIString*.
// The events the combo box fires again are traced as the types the C++ delivers.
//
// Each of 20,000 sequences builds the combo box, adds it to its parent (or not), and makes 40 random calls on it
// (strings and items added and deleted, selections, texts, keys, wheels, presses, events from the button, the list
// and the input box, enabling, no-typing, list box sizes and changes), with random changes of the world between them,
// then frees it and the containers. The traces must be the same, every reference given back. The first 40 sequences
// and every later one that makes an op or a call none before made are rust/crates/ee_won/src/golden_comboobj.txt,
// which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "rust/ee_won.h"
#include "ee/wonapi/WONEventTypes.h"

typedef EeWonGuiOp Op;
typedef EeWonCtVirt V;
typedef EeWonCtCall C;
typedef EeWonComboCall B;

static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c);
static intptr_t CtObjectOf(intptr_t ct);
static bool ExtraRead(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r);
static void ExtraOp(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r);
static bool ExtraLine(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
static intptr_t Host(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
#include "won_ct_world.h"

static const intptr_t kCombo = 1, kParent = 2, kEdit = 5, kButton = 6, kListBox = 7, kArea = 8, kListBox2 = 9;
static intptr_t sEventType, sEventComp, sEventButton;
static EeWonContainer* sCt[kCt + 1];

// ---- the parts' model (the same on both sides; reset per side)

struct Model {
    std::vector<intptr_t> items; // ids
    std::map<intptr_t, int> text; // an item's text
    int sel, edit, undo, nextId;
    intptr_t listBox, area2; // the list box, the second list box's scroll area
};
static Model sM;

static int TextOf(intptr_t item) { return sM.text.count(item) ? sM.text[item] : 0; }
static int WidthOf(intptr_t item) { return TextOf(item) * 37 + (int)(item % 5) * 11; }

// ---- the original (ComboBox.cpp; Container::X on the combo box's Rust container)

namespace orig {
static intptr_t H(Op op, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return Host((void*)kCombo, op, a, b, c, d); }
static intptr_t Ct(C call, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return ee_won_ct_call(sCt[kCombo], call, a, b, c); }
struct ComboMatch { intptr_t mText; };
static bool ComboMatchOf(const void* inMatch, uintptr_t i)
{
    const ComboMatch* aMatch = static_cast<const ComboMatch*>(inMatch);
    intptr_t anItem = H(Op::ComboItemAt, (intptr_t)i);
    return anItem != 0 && H(Op::ComboItemTextIs, anItem, aMatch->mText) != 0;
}

struct ComboBox {
    bool mListBoxVisible, mNoType, mShowButtonWhenDisabled;
    int mListBoxWidth, mListBoxHeight;
    intptr_t Edit() { return H(Op::ComboPart, 0); }
    intptr_t Button() { return H(Op::ComboPart, 1); }
    intptr_t ListBox() { return H(Op::ComboPart, 2); }
    intptr_t ListArea() { return H(Op::ComboPart, 3); }
    ComboBox()
    {
        this->mListBoxVisible = false;
        this->mNoType = false;
        this->mShowButtonWhenDisabled = true;
        this->mListBoxWidth = 200;
        this->mListBoxHeight = 150;
    }
    void Destroy()
    {
        if (this->ListBox() != 0) H(Op::ComboUnlisten);
    }
    intptr_t AddString(intptr_t inText, int inPos)
    {
        if (this->ListArea() != 0) {
            intptr_t item = H(Op::ComboInsertString, inText, inPos);
            this->CalcListBoxSize();
            return item;
        }
        return 0;
    }
    void InsertItem(intptr_t inItem, int inPos)
    {
        if (this->ListArea() != 0) {
            H(Op::ComboInsertItem, inItem, inPos);
            this->CalcListBoxSize();
        } else H(Op::ComboDropItem, inItem);
    }
    void DeleteItem(int inPos)
    {
        if (this->ListArea() != 0) {
            H(Op::ComboDeleteItem, inPos);
            this->CalcListBoxSize();
        }
    }
    void UpdateItem(intptr_t inItem)
    {
        if (this->ListArea() != 0) H(Op::ComboRefreshItem, inItem);
    }
    void Clear(bool inClearInput)
    {
        H(Op::ComboListClear);
        if (inClearInput) H(Op::ComboEditClear, 1);
        this->CalcListBoxSize();
    }
    int GetSelItem()
    {
        intptr_t item = H(Op::ComboSelItem);
        return item == 0 ? -1 : (int)H(Op::ComboItemPos, item);
    }
    void SetSelItem(int inPos, bool inNotify)
    {
        if (this->ListArea() == 0) return;
        H(Op::ComboSetSel, inPos, 1);
        if (H(Op::ComboSelItem) == 0) H(Op::ComboEditClear, inNotify);
        else this->SetInputBoxToListSelection(false, inNotify);
    }
    void SetListBox(intptr_t inListBox)
    {
        H(Op::ComboSetListBox, inListBox);
        if (this->ListArea() != 0 && inListBox != 0 && H(Op::ComboBoxArea, inListBox) != this->ListArea())
            H(Op::ComboSetScroller);
    }
    void AddedToParent()
    {
        if (H(Op::CtParentOf, this->Edit()) != kCombo) this->AddControls();
        Ct(C::AddedToParent);
    }
    void AddControls()
    {
        Ct(C::AddChild, this->Edit());
        Ct(C::AddChild, this->Button());
        H(Op::ComboSetListFlags, 0x1000, 1);
        H(Op::ChildSetFlags, this->ListArea(), 0x8, 0);
        H(Op::ListenTo, this->ListBox());
        H(Op::ButtonSetFlags, this->Button(), 0x40, 1);
        H(Op::ChildSetFlags, this->Button(), 0x8, 0);
    }
    void CalcListBoxSize()
    {
        if (this->mListBoxVisible) return;
        int width, height;
        int aBoxWidth = (int)H(Op::ChildWidth, this->ListBox());
        int anAreaWidth = (int)H(Op::ChildWidth, this->ListArea());
        int aRight = (int)H(Op::ComboListMetric, 0), aLeft = (int)H(Op::ComboListMetric, 1);
        int aTotalWidth = (int)H(Op::ComboListMetric, 2), aTotalHeight = (int)H(Op::ComboListMetric, 3);
        ee_won_gui_combo_list_size(this->mListBoxWidth, this->mListBoxHeight, aBoxWidth, anAreaWidth, aRight, aLeft,
                                   aTotalWidth, (int)H(Op::ChildWidth, kCombo), aTotalHeight, &width, &height);
        H(Op::ChildSetSize, this->ListBox(), width, height);
    }
    void Virtual08() { Ct(C::SizeChanged); this->CalcListBoxSize(); }
    void SetListBoxDimensions(int w, int h) { this->mListBoxWidth = w; this->mListBoxHeight = h; this->CalcListBoxSize(); }
    void SetListBoxWidth(int w) { this->mListBoxWidth = w; this->CalcListBoxSize(); }
    void SetListBoxHeight(int h) { this->mListBoxHeight = h; this->CalcListBoxSize(); }
    void GotFocus()
    {
        Ct(C::GotFocus);
        if (Ct(C::GetField, (intptr_t)EeWonCtField::FocusChild) == 0) H(Op::CtChild, this->Edit(), (intptr_t)V::RequestFocus);
    }
    void LostFocus() { Ct(C::LostFocus); }
    void SetInputBoxToListSelection(bool inActive, bool inFireEvent)
    {
        intptr_t item = H(Op::ComboSelItem);
        if (item == 0) return;
        H(Op::ComboEditItemText, item);
        int anItemWidth = (int)H(Op::ComboItemWidth, item);
        if (ee_won_gui_combo_select_to_end(anItemWidth, (int)H(Op::ChildWidth, this->Edit())))
            H(Op::ComboEditSelect, H(Op::ComboItemTextLength, item), 0);
        else H(Op::ComboEditSelect, 0, -1);
        if (inFireEvent) H(Op::ComboDeliverSel, item, inActive);
    }
    void SetText(intptr_t inText, bool inNotify)
    {
        H(Op::ComboEditSetText, inText, inNotify);
        H(Op::Invalidate);
    }
    void SetTextAndSel(intptr_t inText, bool inNotify)
    {
        if (this->ListArea() != 0) {
            ComboMatch aMatch = {inText};
            int64_t i = ee_won_gui_first_match((size_t)H(Op::ComboItemCount), ComboMatchOf, &aMatch);
            if (i >= 0) {
                this->SetSelItem((int)i, inNotify);
                return;
            }
        }
        this->SetText(inText, inNotify);
    }
    void ShowList()
    {
        if (this->mListBoxVisible) return;
        this->CalcListBoxSize();
        intptr_t selected = H(Op::ComboSelItem);
        if (selected != 0) {
            if (!H(Op::ComboItemTextIs, selected, 0)) {
                ComboMatch aMatch = {0};
                int64_t i = ee_won_gui_first_match((size_t)H(Op::ComboItemCount), ComboMatchOf, &aMatch);
                if (i >= 0) {
                    H(Op::ComboSetSel, (int)i, 0);
                    H(Op::ComboEnsureVisible, H(Op::ComboItemAt, (int)i));
                } else H(Op::ComboSetSel, -1, 0);
            } else H(Op::ComboEnsureVisible, selected);
        }
        H(Op::ComboOpenPopup);
        this->mListBoxVisible = true;
    }
    void HandleComponentEvent(intptr_t inEvent)
    {
        if (H(Op::EventComponent, inEvent) == this->Button()) {
            if (!this->mListBoxVisible) {
                H(Op::ComboUndoSave);
                this->ShowList();
                return;
            }
            this->mListBoxVisible = false;
            H(Op::ClosePopup, this->ListBox());
            return;
        }
        if (H(Op::EventType, inEvent) == 23) {
            if (H(Op::ComboEventButton, inEvent) != 0) return;
            this->SetInputBoxToListSelection(true, true);
            this->mListBoxVisible = false;
            H(Op::ClosePopup, this->ListBox());
            return;
        }
        if (H(Op::EventType, inEvent) == 30) {
            if (this->mListBoxVisible) this->mListBoxVisible = false;
            return;
        }
        if (H(Op::EventType, inEvent) == 21) {
            H(Op::FireEvent, -21, 1); // Fn2F5380(inEvent->mType, 1): see ExtraLine
            return;
        }
        if (H(Op::EventType, inEvent) == 20) {
            H(Op::FireEvent, -20, 1);
            return;
        }
        H(Op::ComponentHandleEvent, inEvent);
    }
    void MouseDown(int x, int y, int button)
    {
        Ct(C::MouseDown, x, y, button);
        if (button == 0) {
            if (Ct(C::GetField, (intptr_t)EeWonCtField::MouseChild) == this->Edit() && this->mNoType) H(Op::DlgClick, this->Button());
            H(Op::RequestFocus);
        }
    }
    bool MouseWheel(int amount)
    {
        return this->mListBoxVisible ? H(Op::CtChild, this->ListArea(), (intptr_t)V::MouseWheel, amount) != 0 : false;
    }
    bool KeyDown(int key)
    {
        switch (ee_won_gui_combo_key(key, this->mListBoxVisible)) {
        case 1:
            H(Op::ComboUndoSave);
            this->SetInputBoxToListSelection(false, true);
            this->ShowList();
            return true;
        case 2:
            H(Op::CtChild, this->ListArea(), (intptr_t)V::KeyDown, key);
            this->SetInputBoxToListSelection(false, true);
            return true;
        case 3:
            this->SetInputBoxToListSelection(true, true);
            H(Op::DlgClick, this->Button());
            return true;
        case 4:
            H(Op::ComboEditUndo);
            H(Op::DlgClick, this->Button());
            return true;
        }
        return Ct(C::KeyDown, key) != 0;
    }
    void SetNoType(bool inNoType)
    {
        if (inNoType == this->mNoType) return;
        this->mNoType = inNoType;
        H(Op::ChildSetFlags, this->Edit(), 0x8, !inNoType);
        H(Op::ComboEditAllowSelection, !inNoType);
        if (inNoType && Ct(C::GetField, (intptr_t)EeWonCtField::FocusChild) == this->Edit()) H(Op::CtVirt, (intptr_t)V::SetFocusChild, 0);
    }
    void SetEnabled(bool inEnabled)
    {
        if (inEnabled == !(H(Op::CtBits, kCombo) & 0x80)) return;
        H(Op::BaseSetEnabled, inEnabled);
        H(Op::ChildSetEnabled, this->Edit(), inEnabled);
        H(Op::ChildSetEnabled, this->Button(), inEnabled);
        if (!inEnabled && this->mListBoxVisible) {
            this->mListBoxVisible = false;
            H(Op::ClosePopup, this->ListBox());
        }
        if (!this->mShowButtonWhenDisabled) {
            H(Op::ChildSetVisible, this->Button(), inEnabled);
            H(Op::ComboRelayout);
        }
    }
    intptr_t Field(int f)
    {
        switch ((EeWonComboField)f) {
        case EeWonComboField::ListBoxVisible: return this->mListBoxVisible;
        case EeWonComboField::NoType: return this->mNoType;
        case EeWonComboField::ShowButtonWhenDisabled: return this->mShowButtonWhenDisabled;
        case EeWonComboField::ListBoxWidth: return this->mListBoxWidth;
        case EeWonComboField::ListBoxHeight: return this->mListBoxHeight;
        }
        return 0;
    }
    intptr_t Call(B call, intptr_t a, intptr_t b, intptr_t c)
    {
        switch (call) {
        case B::SizeChanged: Virtual08(); return 0;
        case B::MouseDown: MouseDown((int)a, (int)b, (int)c); return 0;
        case B::MouseWheel: return MouseWheel((int)a);
        case B::GotFocus: GotFocus(); return 0;
        case B::LostFocus: LostFocus(); return 0;
        case B::KeyDown: return KeyDown((int)a);
        case B::AddedToParent: AddedToParent(); return 0;
        case B::HandleComponentEvent: HandleComponentEvent(a); return 0;
        case B::SetEnabled: SetEnabled(a != 0); return 0;
        case B::AddString: return AddString(a, (int)b);
        case B::InsertItem: InsertItem(a, (int)b); return 0;
        case B::DeleteItem: DeleteItem((int)a); return 0;
        case B::UpdateItem: UpdateItem(a); return 0;
        case B::Clear: Clear(a != 0); return 0;
        case B::GetSelItem: return GetSelItem();
        case B::SetSelItem: SetSelItem((int)a, b != 0); return 0;
        case B::SetListBox: SetListBox(a); return 0;
        case B::AddControls: AddControls(); return 0;
        case B::CalcListBoxSize: CalcListBoxSize(); return 0;
        case B::SetListBoxDimensions: SetListBoxDimensions((int)a, (int)b); return 0;
        case B::SetListBoxWidth: SetListBoxWidth((int)a); return 0;
        case B::SetListBoxHeight: SetListBoxHeight((int)a); return 0;
        case B::SetInputBoxToListSelection: SetInputBoxToListSelection(a != 0, b != 0); return 0;
        case B::SetText: SetText(a, b != 0); return 0;
        case B::SetTextAndSel: SetTextAndSel(a, b != 0); return 0;
        case B::ShowList: ShowList(); return 0;
        case B::SetNoType: SetNoType(a != 0); return 0;
        case B::GetField: return Field((int)a);
        case B::SetField:
            if ((EeWonComboField)a == EeWonComboField::ShowButtonWhenDisabled) this->mShowButtonWhenDisabled = b != 0;
            return 0;
        }
        return 0;
    }
};
}

static orig::ComboBox* sOrig;
static EeWonComboBox* sRust;

static intptr_t ComboCall(B call, intptr_t a, intptr_t b, intptr_t c)
{
    if (sRustSide) return ee_won_combo_call(sRust, call, a, b, c);
    return sOrig->Call(call, a, b, c);
}
static bool ComboAlive() { return sRustSide ? sRust != NULL : sOrig != NULL; }

// the combo box's overrides of the container's virtuals
static B ComboOverride(C call)
{
    switch (call) {
    case C::SizeChanged: return B::SizeChanged;
    case C::MouseDown: return B::MouseDown;
    case C::MouseWheel: return B::MouseWheel;
    case C::GotFocus: return B::GotFocus;
    case C::LostFocus: return B::LostFocus;
    case C::KeyDown: return B::KeyDown;
    case C::AddedToParent: return B::AddedToParent;
    default: return (B)0;
    }
}

static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c)
{
    if (ct == kCombo && ComboAlive() && (int)ComboOverride(call) != 0) return ComboCall(ComboOverride(call), a, b, c);
    return ee_won_ct_call(sCt[ct], call, a, b, c);
}
static intptr_t CtObjectOf(intptr_t ct) { return (intptr_t)sCt[ct]; }

static bool ExtraRead(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r)
{
    (void)self; (void)a; (void)b; (void)c; (void)d;
    switch (op) {
    case Op::EventType: *r = sEventType; return true;
    case Op::EventComponent: *r = sEventComp; return true;
    default: return false;
    }
}

static bool ExtraLine(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)c; (void)d;
    if (op != Op::FireEvent) return false;
    // the original fires its input box's event type again (the enum's); the Rust side's event becomes a type through
    // ee_won_gui_event_type, the call ComboBox.cpp's host makes
    long long type = a < 0 ? -(long long)a : ee_won_gui_event_type((EeWonGuiEvent)a);
    Line("op %lld fire %lld %lld @%lld", (long long)op, type, b, self);
    return true;
}

// Component's SetSize, SetEnabled and SetVisible in the world
static void WorldSetSize(intptr_t a, int w, int h)
{
    int ow, oh;
    if (!ee_won_gui_set_size(sW[a].w, sW[a].h, w, h, &ow, &oh)) return;
    Set((int)a, 6, oh);
    Set((int)a, 5, ow);
    VCall(a, V::SizeChanged);
}
static void WorldStateChanged(intptr_t a)
{
    if (sW[a].parent != 0) Do(sW[a].parent, C::ChildStateChanged, a);
    VCall(a, V::Invalidate);
}
static void WorldSetEnabled(intptr_t a, bool on)
{
    if (on == !(sW[a].bits & 0x80)) return;
    Set((int)a, 0, on ? (sW[a].bits & ~0x80u) : (sW[a].bits | 0x80));
    WorldStateChanged(a);
}

static void ExtraOp(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r)
{
    (void)d;
    int n = (int)sM.items.size();
    switch (op) {
    case Op::ComboPart: *r = a == 0 ? kEdit : a == 1 ? kButton : a == 2 ? sM.listBox : kArea; break;
    case Op::ComboInsertString: case Op::ComboInsertItem: {
        intptr_t id = op == Op::ComboInsertString ? sM.nextId++ : a;
        if (op == Op::ComboInsertString) sM.text[id] = (int)a;
        int pos = (int)b;
        if (pos < 0 || pos > n) pos = n;
        sM.items.insert(sM.items.begin() + pos, id);
        if (sM.sel >= pos) sM.sel++;
        if (op == Op::ComboInsertString) *r = id;
        break;
    }
    case Op::ComboDeleteItem:
        if (a >= 0 && a < n) {
            sM.items.erase(sM.items.begin() + a);
            if (sM.sel == a) sM.sel = -1;
            else if (sM.sel > a) sM.sel--;
        }
        break;
    case Op::ComboListClear: sM.items.clear(); sM.sel = -1; break;
    case Op::ComboSelItem: *r = sM.sel >= 0 ? sM.items[sM.sel] : 0; break;
    case Op::ComboItemPos:
        *r = -1;
        for (int i = 0; i < n; i++) if (sM.items[i] == a) *r = i;
        break;
    case Op::ComboSetSel: sM.sel = a >= 0 && a < n ? (int)a : -1; break;
    case Op::ComboSetListBox: sM.listBox = a; break;
    case Op::ComboBoxArea: *r = a == kListBox ? kArea : sM.area2; break;
    case Op::ComboListMetric: {
        int w = 0;
        for (int i = 0; i < n; i++) if (WidthOf(sM.items[i]) > w) w = WidthOf(sM.items[i]);
        *r = a == 0 ? 2 : a == 1 ? 3 : a == 2 ? w : n * 15;
        break;
    }
    case Op::ComboEditClear: sM.edit = 0; break;
    case Op::ComboEditItemText: sM.edit = TextOf(a); break;
    case Op::ComboEditSetText: sM.edit = (int)a; break;
    case Op::ComboEditUndo: sM.edit = sM.undo; break;
    case Op::ComboUndoSave: sM.undo = sM.edit; break;
    case Op::ComboItemWidth: *r = WidthOf(a); break;
    case Op::ComboItemTextLength: *r = TextOf(a); break;
    case Op::ComboItemCount: *r = n; break;
    case Op::ComboItemAt: *r = a >= 0 && a < n ? sM.items[a] : 0; break;
    case Op::ComboItemTextIs: *r = TextOf(a) == (b != 0 ? (int)b : sM.edit); break;
    case Op::ComboEventButton: *r = sEventButton; break;
    case Op::ChildSetSize: WorldSetSize(a, (int)b, (int)c); break;
    case Op::ChildSetEnabled: WorldSetEnabled(a, b != 0); break;
    case Op::BaseSetEnabled: WorldSetEnabled(self, a != 0); break;
    case Op::ChildSetVisible:
        if ((b != 0) == !(sW[a].bits & 1)) break;
        Set((int)a, 0, b ? (sW[a].bits & ~1u) : (sW[a].bits | 1));
        Set((int)a, 1, 0);
        WorldStateChanged(a);
        VCall(a, V::NotifyRootVisibility, b);
        break;
    default: break; // the rest traced only (the popup, the selection's range, the events, the clicks)
    }
}

// ---- the driver

static void ComboTop(B call, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    Line("combo %lld %lld %lld %lld", (long long)call, a, b, c);
    long long r = ComboCall(call, a, b, c);
    Line("-> %lld 0", r);
}

static void SetEvent(intptr_t type, intptr_t comp, intptr_t button)
{
    sEventType = type; sEventComp = comp; sEventButton = button;
    Line("env 10 %lld", sEventType);
    Line("env 13 %lld %lld", sEventComp, sEventButton);
}

static void RandomComboCall()
{
    int n = (int)sM.items.size();
    int pos = (int)DR((unsigned)n + 3) - 1;
    switch (DR(32)) {
    case 0: case 1: case 2: ComboTop(B::AddString, 1 + DR(4), DR(3) ? -1 : pos); break;
    case 3: {
        intptr_t id = 200 + DR(1000);
        sM.text[id] = 1 + (int)(id % 4); // (the driver makes the item: both sides see it)
        ComboTop(B::InsertItem, id, pos);
        break;
    }
    case 4: ComboTop(B::DeleteItem, pos); break;
    case 5: ComboTop(B::UpdateItem, n > 0 ? sM.items[DR((unsigned)n)] : 0); break;
    case 6: ComboTop(B::Clear, DR(2)); break;
    case 7: ComboTop(B::GetSelItem); break;
    case 8: case 9: ComboTop(B::SetSelItem, pos, DR(2)); break;
    case 10: ComboTop(B::SetListBox, DR(2) ? kListBox : kListBox2); break;
    case 11: ComboTop(B::CalcListBoxSize); break;
    case 12: ComboTop(B::SetListBoxDimensions, 20 + DR(200), 10 + DR(150)); break;
    case 13: ComboTop(DR(2) ? B::SetListBoxWidth : B::SetListBoxHeight, 10 + DR(200)); break;
    case 14: ComboTop(B::SetInputBoxToListSelection, DR(2), DR(2)); break;
    case 15: ComboTop(B::SetText, DR(5), DR(2)); break;
    case 16: ComboTop(B::SetTextAndSel, 1 + DR(4), DR(2)); break;
    case 17: ComboTop(B::ShowList); break;
    case 18: ComboTop(B::SetNoType, DR(2)); break;
    case 19: ComboTop(B::SetEnabled, DR(3) != 0); break;
    case 20: { static const int kKeys[7] = {40, 38, 33, 34, 13, 27, 65}; ComboTop(B::KeyDown, kKeys[DR(7)]); break; }
    case 21: ComboTop(B::MouseWheel, (int)DR(5) - 2); break;
    case 22: {
        // half of the presses on the input box
        int x = (int)DR(260) - 30, y = (int)DR(260) - 30;
        if (DR(2)) { x = sW[kEdit].x + (int)DR((unsigned)sW[kEdit].w); y = sW[kEdit].y + (int)DR((unsigned)sW[kEdit].h); }
        ComboTop(B::MouseDown, x, y, DR(4) ? 0 : 1);
        break;
    }
    case 23: ComboTop(DR(2) ? B::GotFocus : B::LostFocus); break;
    case 24: case 25: {
        static const int kTypes[6] = {23, 23, 30, 21, 20, 15};
        SetEvent(kTypes[DR(6)], DR(3) ? kEdit : kButton, DR(4) ? 0 : 1);
        ComboTop(B::HandleComponentEvent, kEvent);
        break;
    }
    case 26: ComboTop(B::GetField, 1 + DR(5)); break;
    case 27: ComboTop(B::SetField, (intptr_t)EeWonComboField::ShowButtonWhenDisabled, DR(2)); break;
    case 28: TopCall(kParent, DR(3) ? C::AddChild : C::RemoveChild, kCombo, 0, 0); break;
    case 29: ComboTop(B::SizeChanged); break;
    case 30: sM.area2 = DR(2) ? kArea : 11; Line("env 15 %lld", sM.area2); break;
    default: ComboTop(B::AddControls); break;
    }
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sCursorRefs.clear();
    sRustSide = inRust;
    sDriverSeed = inSeq * 40503u + 13u;
    sAnswerSeed = inSeq * 2654435761u + 7u;
    InitWorld();
    sEventType = sEventComp = sEventButton = 0;
    sM = Model();
    sM.sel = -1; sM.edit = 0; sM.undo = 0; sM.nextId = 100; sM.listBox = kListBox; sM.area2 = 11;
    for (int ct = 1; ct <= kCt; ct++) {
        Line("new %lld", ct);
        sCt[ct] = ee_won_ct_new((void*)(intptr_t)ct, Host, false);
    }
    Line("combo-new");
    if (inRust) sRust = ee_won_combo_new((void*)kCombo, Host, sCt[kCombo]);
    else sOrig = new orig::ComboBox;
    if (DR(2)) for (int i = (int)DR(5); i > 0; i--) ComboTop(B::AddString, 1 + DR(4), -1);
    if (DR(4)) TopCall(kParent, C::AddChild, kCombo, 0, 0);
    Env(0, DR(2) ? kParent : 1);
    for (int i = 0; i < inCalls; i++) {
        if (DR(3) == 0) RandomChange();
        RandomComboCall();
    }
    Line("free-combo");
    if (inRust) { ee_won_combo_free(sRust); sRust = NULL; }
    else { sOrig->Destroy(); delete sOrig; sOrig = NULL; }
    for (int ct = kCt; ct >= 1; ct--) {
        Line("free %lld", ct);
        ee_won_ct_free(sCt[ct]);
        sCt[ct] = NULL;
    }
    for (int id = 1; id <= kN; id++) CHECK(sW[id].refs == 0);
    for (std::map<intptr_t, int>::iterator i = sCursorRefs.begin(); i != sCursorRefs.end(); ++i) CHECK(i->second == 0);
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    // the events the combo box fires again are the original enum's types
    CHECK(ee_won_gui_event_type(EeWonGuiEvent::InputReturn) == WONAPI::ComponentEvent_InputReturn);
    CHECK(ee_won_gui_event_type(EeWonGuiEvent::InputTextChanged) == WONAPI::ComponentEvent_InputTextChanged);
    CHECK(WONAPI::ComponentEvent_ListItemClicked == 23 && WONAPI::ComponentEvent_EndPopup == 30);
    const int kSequences = 20000;
    const int kCallsPer = aPrint ? 16 : 40;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sOpsMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCallsPer);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCallsPer);
        aLines += a.size();
        bool same = a == b;
        CHECK(same);
        if (!same) {
            aDiffering++;
            if (aDiffering <= 3) {
                printf("sequence %d differs\n", seq);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "<end>";
                    const char* y = i < b.size() ? b[i].c_str() : "<end>";
                    if (strcmp(x, y) != 0) {
                        printf("  line %u\n", (unsigned)i);
                        for (size_t j = i > 6 ? i - 6 : 0; j < i; j++) printf("  both: %s\n", a[j].c_str());
                        printf("  orig: %s\n  rust: %s\n", x, y);
                        break;
                    }
                }
            }
        }
        bool aNew = seq < 40;
        for (std::set<int>::const_iterator i = sOpsMet.begin(); i != sOpsMet.end(); ++i)
            if (aSeen.insert("op" + std::to_string(*i)).second) aNew = true;
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].compare(0, 6, "combo ") == 0) {
                std::string key = a[i].substr(0, a[i].find(' ', 6));
                if (aSeen.insert(key).second) aNew = true;
            }
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_comboobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
