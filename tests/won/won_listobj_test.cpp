// ListArea as a whole object (R6 WON round 23): the original ListArea.cpp's methods and SetSortType
// (ListControlAdapters.cpp), the statements copied from the C++ before the port with every call on a C++ object (the
// ScrollArea base and its fields, the items' fields and virtuals, the list's own virtuals, the sort, the events, the
// tool tip, the Graphics) replaced by the host op that makes it (GuiHost.cpp runs the same calls in the same order),
// against ee_won's Rust ListArea (listobj.rs) over one scripted host. The host keeps the scroll area's fields, the
// items (tops, sizes, selection, selectable flags, the sizes CalcDimensions gives, sort keys, references) and
// sorts with the items' keys; the list's virtuals (AdjustPosition, Sort, NotifyMouseItemGone) and the base class's
// size change call the side's ListArea methods back, as the C++ object without a subclass does.
//
// Each of 20,000 sequences creates the list and makes 40 random inward calls (inserts at places and sorted, batches,
// refreshes that move an item, deletes, ranges, clears, every flag, selections of every kind, the selection walk,
// sorting, size changes, scrolls, paints, the mouse over items with every flag, keys). The traces (every op with its
// arguments and answer, the draws) must be the same, and every reference taken must be given back when the list
// goes. The first 40 sequences and every later one that makes an op or a call none before made are
// rust/crates/ee_won/src/golden_listobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "rust/ee_won.h"
#include "ee/wonapi/WONEventTypes.h"

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

struct HostItem {
    int top, height, width;
    bool selected, selectable;
    int calcH, calcW, key, refs;
};
static std::map<intptr_t, HostItem> sItems;
static intptr_t sNextItem;
static int sFields[16];
static std::vector<std::string> sTrace;
static std::set<int> sOpsMet;

struct Side {
    void (*mAdjust)(bool);
    bool (*mSort)();
    void (*mGone)();
};
static Side sSide;

static intptr_t NewItem()
{
    HostItem h;
    h.top = (int)R(50); h.height = 0; h.width = 0;
    h.selected = R(5) == 0; h.selectable = R(6) != 0;
    h.calcH = R(10) == 0 ? (int)R(3) - 1 : 8 + (int)R(14);
    h.calcW = 20 + (int)R(300);
    h.key = (int)R(12);
    h.refs = 1; // the creator's
    intptr_t id = 0x1000 + (++sNextItem);
    sItems[id] = h;
    return id;
}

static void Line(const char* f, long long a, long long b, long long c, long long d, long long e, long long g = 0)
{
    char aLine[300];
    snprintf(aLine, sizeof(aLine), f, a, b, c, d, e, g);
    sTrace.push_back(aLine);
}
static void TraceFields()
{
    std::string s = "fields";
    char b[24];
    for (int i = 0; i < 12; i++) { snprintf(b, sizeof(b), " %d", sFields[i]); s += b; }
    sTrace.push_back(s);
}
static bool KeyLess(intptr_t a, intptr_t b, int type)
{
    int ka = sItems[a].key, kb = sItems[b].key;
    return type == 0 ? ka < kb : ka > kb;
}

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp;
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    int n = (int)inOp;
    switch (inOp) {
    case Op::Get: return sFields[a]; // pure reads: not traced
    case Op::Set: sFields[a] = (int)b; break;
    case Op::FireEvent:
        // the type listeners receive: the original's enum (a negative code here), the Rust's through
        // ee_won_gui_event_type, the call GuiHost.cpp's host makes (round 30: traced as the type)
        Line("%lld fire %lld", n, a < 0 ? -(long long)a : (long long)ee_won_gui_event_type((EeWonGuiEvent)a), 0, 0, 0);
        return 0;
    case Op::BaseSizeChanged:
        Line("%lld base-size", n, 0, 0, 0, 0);
        sFields[(int)Fd::ScrollWidth] = 20 + (int)R(280);
        sFields[(int)Fd::ScrollHeight] = 10 + (int)R(200);
        sFields[(int)Fd::Width] = sFields[(int)Fd::ScrollWidth] + 4;
        sFields[(int)Fd::Height] = sFields[(int)Fd::ScrollHeight] + 4;
        TraceFields();
        sSide.mAdjust(false); // ScrollArea::Virtual08 calls the virtual AdjustPosition
        return 0;
    case Op::BaseAdjustPosition: {
        int aMaxV = sFields[(int)Fd::TotalHeight] + sFields[(int)Fd::ExtraHeight] - sFields[(int)Fd::ScrollHeight];
        int aMaxH = sFields[(int)Fd::TotalWidth] - sFields[(int)Fd::ScrollWidth];
        int& v = sFields[(int)Fd::VertOffset];
        int& h = sFields[(int)Fd::HorzOffset];
        if (v > aMaxV) v = aMaxV;
        if (v < 0) v = 0;
        if (h > aMaxH) h = aMaxH;
        if (h < 0) h = 0;
        Line("%lld base-adjust %lld -> %lld %lld", n, (long long)a, v, h, 0);
        return 0;
    }
    case Op::BaseKeyDown: r = R(8) == 0; break;
    case Op::ScrollTranslate: {
        int* x = (int*)a; int* y = (int*)b;
        long long ix = *x, iy = *y;
        *x += sFields[(int)Fd::HorzOffset] - 2;
        *y += sFields[(int)Fd::VertOffset] - 2;
        Line("%lld translate %lld %lld -> %lld %lld", n, ix, iy, *x, *y);
        return 0;
    }
    case Op::VirtAdjustPosition: Line("%lld virt-adjust %lld", n, (long long)a, 0, 0, 0); sSide.mAdjust(a != 0); return 0;
    case Op::VirtSort: { Line("%lld virt-sort", n, 0, 0, 0, 0); bool s = sSide.mSort(); Line("%lld virt-sort -> %lld", n, s, 0, 0, 0); return s; }
    case Op::VirtNotifyMouseItemGone: Line("%lld virt-gone", n, 0, 0, 0, 0); sSide.mGone(); return 0;
    case Op::SetVertOffset: sFields[(int)Fd::VertOffset] = (int)a; break;
    case Op::OnBottom: r = R(2); break;
    case Op::RootKeyMod: r = R(8); break;
    case Op::GetViewport: r = 0x77; break;
    case Op::PointerOutside: r = R(2); break;
    case Op::DeliverItemClicked: {
        const EeWonItemClick* k = (const EeWonItemClick*)b;
        Line("%lld click %llx %lld %lld %lld %lld", n, (long long)a, k->button, k->up, k->x, k->y);
        return 0;
    }
    case Op::SortItems: {
        intptr_t* v = (intptr_t*)a;
        int type = (int)c;
        std::sort(v, v + b, [type](intptr_t x, intptr_t y) { return KeyLess(x, y, type); });
        std::string s = std::to_string(n) + " sort";
        char buf[24];
        for (intptr_t i = 0; i < b; i++) { snprintf(buf, sizeof(buf), " %llx", (long long)v[i]); s += buf; }
        sTrace.push_back(s);
        return 0;
    }
    case Op::LowerBound: {
        const intptr_t* v = (const intptr_t*)a;
        int type = (int)d;
        r = std::lower_bound(v, v + b, c, [type](intptr_t x, intptr_t y) { return KeyLess(x, y, type); }) - v;
        Line("%lld lower-bound %llx %lld -> %lld", n, (long long)c, (long long)d, (long long)r, 0);
        return r;
    }
    case Op::Draw: {
        const EeWonDrawOp* o = (const EeWonDrawOp*)b;
        char aLine[200];
        snprintf(aLine, sizeof(aLine), "%d draw %llx %d %d %d %d %d", n, (long long)a, o->kind, o->a, o->b, o->c, o->d);
        sTrace.push_back(aLine);
        return 0;
    }
    case Op::Less: r = KeyLess(a, b, (int)c); break;
    case Op::ItemTop: r = sItems[a].top; break;
    case Op::ItemSetTop: sItems[a].top = (int)b; break;
    case Op::ItemHeight: r = sItems[a].height; break;
    case Op::ItemSetHeight: sItems[a].height = (int)b; break;
    case Op::ItemWidth: r = sItems[a].width; break;
    case Op::ItemSelected: r = sItems[a].selected; break;
    case Op::ItemSetSelected: sItems[a].selected = b != 0; break;
    case Op::ItemSelectable: r = sItems[a].selectable; break;
    case Op::ItemSetPos: sItems[a].top = (int)b; break;
    case Op::ItemContains: r = c >= sItems[a].top && c < sItems[a].top + sItems[a].height; break;
    case Op::ItemCanSelect: r = sItems[a].selectable && (b & 15) != 3; break;
    case Op::ItemCalcDimensions: sItems[a].height = sItems[a].calcH; sItems[a].width = sItems[a].calcW; break;
    case Op::ItemHold: sItems[a].refs++; break;
    case Op::ItemRelease: sItems[a].refs--; break;
    default: break;
    }
    Line("%lld %llx %llx %llx %llx -> %llx", n, (long long)a, (long long)b, (long long)c, (long long)d, (long long)r);
    return r;
}

static intptr_t H(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    return ScriptedHost((void*)0x55, inOp, a, b, c, d);
}
static int G(Fd f) { return (int)H(Op::Get, (intptr_t)f); }
static void S(Fd f, int v) { H(Op::Set, (intptr_t)f, v); }

// ---- the original (ListArea.cpp; the base ScrollArea, the items and the list's virtuals through the host)

namespace orig {
enum { ListSortType_None = 2, VK_PRIOR = 0x21, VK_NEXT = 0x22, VK_LEFT = 0x25, VK_UP = 0x26, VK_RIGHT = 0x27, VK_DOWN = 0x28 };
static const intptr_t kG = 0x6;
typedef intptr_t Item;
static int Top(Item i) { return (int)H(Op::ItemTop, i); }
static int Height(Item i) { return (int)H(Op::ItemHeight, i); }
static int Width(Item i) { return (int)H(Op::ItemWidth, i); }
static bool Selected(Item i) { return H(Op::ItemSelected, i) != 0; }
static void SetSelected(Item i, bool on) { H(Op::ItemSetSelected, i, on); }
static int32_t TopOf(const void* ctx, uintptr_t i) { return Top((*(const std::vector<Item>*)ctx)[i]); }
static bool SelectableOf(const void* ctx, uintptr_t i) { return H(Op::ItemSelectable, (*(const std::vector<Item>*)ctx)[i]) != 0; }
static EeWonRowPlace PlaceOf(const void* ctx, size_t i)
{
    const std::vector<Item>& v = *(const std::vector<Item>*)ctx;
    EeWonRowPlace r;
    r.x = 0;
    r.top = Top(v[i]);
    r.height = Height(v[i]);
    return r;
}

struct ListArea {
    std::vector<Item> mItems;
    int mTopLineVectorPos, mListFlags, mDoingMultiChange, mPaintWidth, mSortType;
    Item mSelItem, mKeyItem, mMouseItem;
    int mSelItemItr;
    ListArea()
    {
        this->mTopLineVectorPos = 0; this->mListFlags = 0xb0; this->mSortType = ListSortType_None;
        this->mSelItem = this->mKeyItem = this->mMouseItem = 0;
        this->mPaintWidth = -1; this->mDoingMultiChange = 0; this->mSelItemItr = 0;
    }
    ~ListArea() { for (size_t i = 0; i < this->mItems.size(); i++) H(Op::ItemRelease, this->mItems[i]); }
    bool ListFlagSet(int f) { return (this->mListFlags & f) != 0; }
    void Invalidate() { H(Op::Invalidate); }
    void AdjustPositionV(bool b) { H(Op::VirtAdjustPosition, b); }
    bool SortV() { return H(Op::VirtSort) != 0; }
    void NotifyMouseItemGoneV() { H(Op::VirtNotifyMouseItemGone); }

    void BeginUpdate() { ++this->mDoingMultiChange; }
    void EndUpdate()
    {
        if (--this->mDoingMultiChange <= 0) {
            this->mDoingMultiChange = 0;
            if (!this->SortV()) this->EnforceInvariants();
        }
    }
    size_t FindLineAt(int y) { return ee_won_gui_list_line_at(this->mItems.size(), y, TopOf, &this->mItems); }
    Item FindItemAt(int y)
    {
        size_t i = this->FindLineAt(y);
        if (i == this->mItems.size()) return 0;
        Item item = this->mItems[i];
        int t = Top(item), h = Height(item);
        return ee_won_gui_list_covers(t, h, y) ? item : 0;
    }
    void FindTopLine() { this->mTopLineVectorPos = (int)this->FindLineAt(G(Fd::VertOffset)); }
    bool IsItemVisible(Item item)
    {
        int t = Top(item), h = Height(item);
        return ee_won_gui_list_visible(t, h, G(Fd::VertOffset), G(Fd::ScrollHeight), this->ListFlagSet(0x80));
    }
    void EnsureVisible(Item item)
    {
        int t = Top(item), h = Height(item);
        int offset = ee_won_gui_list_ensure_visible(t, h, G(Fd::VertOffset), G(Fd::ScrollHeight));
        if (offset != G(Fd::VertOffset)) H(Op::SetVertOffset, offset);
    }
    bool SetSelItem(Item item, bool add, bool fire)
    {
        if (item == 0) return this->ClearSelectionPrv(fire) > 0;
        Item old = this->mSelItem;
        if (item != this->mSelItem) {
            if (this->mSelItem != 0 && (!this->ListFlagSet(0x400) || !add)) {
                bool selected = Selected(item);
                this->ClearSelectionPrv(false);
                SetSelected(item, selected);
            }
            this->mSelItem = item;
        }
        if (this->mSelItem != 0 && !Selected(this->mSelItem)) this->mSelItem = 0;
        if (item != old) this->Invalidate();
        if (this->mSelItem != old) {
            this->Invalidate();
            H(Op::FocusChildLost);
            if (fire) H(Op::FireEvent, -(intptr_t)WONAPI::ComponentEvent_ListSelChanged);
            H(Op::VirtListSelChangedHook);
            return true;
        }
        return false;
    }
    void SelectRange(int start, int end)
    {
        this->ClearSelectionPrv(false);
        int step = 1;
        if (!ee_won_gui_list_select_range(start, end, (int)this->mItems.size(), &start, &end, &step)) return;
        for (int i = start; i != end + step; i += step) SetSelected(this->GetItem(i), true);
        this->SetSelItem(this->GetItem(start), false, true);
    }
    void SetSelItemAt(int index, bool fire)
    {
        if (index >= 0 && index < (int)this->mItems.size()) {
            Item item = this->mItems[index];
            SetSelected(item, true);
            this->mKeyItem = item;
            this->SetSelItem(item, false, fire);
        } else {
            this->SetSelItem(0, false, fire);
            this->mKeyItem = 0;
        }
    }
    void RewindSelections() { this->mSelItemItr = -1; this->FindNextSelection(); }
    bool HasMoreSelections() { return (unsigned int)this->mSelItemItr < this->mItems.size(); }
    void FindNextSelection()
    {
        ++this->mSelItemItr;
        while ((unsigned int)this->mSelItemItr < this->mItems.size() && !Selected(this->mItems[this->mSelItemItr])) ++this->mSelItemItr;
    }
    Item GetNextSelection()
    {
        if (this->HasMoreSelections()) {
            Item item = this->mItems[this->mSelItemItr];
            this->FindNextSelection();
            return item;
        }
        return 0;
    }
    int ClearSelectionPrv(bool fire)
    {
        if (this->mSelItem == 0) return 0;
        int count = 0;
        if (this->ListFlagSet(0x400)) {
            for (size_t i = 0; i < this->mItems.size(); i++)
                if (Selected(this->mItems[i])) { SetSelected(this->mItems[i], false); ++count; }
        } else { count = 1; SetSelected(this->mSelItem, false); }
        this->mSelItem = 0;
        this->Invalidate();
        if (fire) { H(Op::FireEvent, -(intptr_t)WONAPI::ComponentEvent_ListSelChanged); H(Op::VirtListSelChangedHook); }
        return count;
    }
    int GetItemPos(Item item)
    {
        if (item == 0) return -1;
        size_t i = this->FindLineAt(Top(item));
        while (i != this->mItems.size()) {
            if (this->mItems[i] == item) return (int)i;
            if (Top(this->mItems[i]) > Top(item)) break;
            ++i;
        }
        return -1;
    }
    Item GetItem(int index)
    {
        if (index < 0 || (unsigned int)index >= this->mItems.size()) return 0;
        return this->mItems[index];
    }
    void Clear(bool fire)
    {
        S(Fd::TotalHeight, 0); S(Fd::TotalWidth, 0); S(Fd::HorzOffset, 0); S(Fd::VertOffset, 0);
        this->ClearSelectionPrv(fire);
        this->mSelItem = 0;
        this->mKeyItem = 0;
        if (this->mMouseItem != 0) this->NotifyMouseItemGoneV();
        std::vector<Item> gone;
        gone.swap(this->mItems);
        for (size_t i = 0; i < gone.size(); i++) H(Op::ItemRelease, gone[i]);
        this->AdjustPositionV(false);
    }
    void EnforceInvariants()
    {
        if (this->mDoingMultiChange) return;
        size_t aCount = this->mItems.size();
        std::vector<int> aHeights(aCount), aWidths(aCount), aTops(aCount);
        for (size_t k = 0; k < aCount; ++k) { aHeights[k] = Height(this->mItems[k]); aWidths[k] = Width(this->mItems[k]); }
        int th = 0, tw = 0;
        ee_won_gui_list_stack(aCount ? &aHeights[0] : NULL, aCount ? &aWidths[0] : NULL, aCount ? &aTops[0] : NULL, aCount, &th, &tw);
        S(Fd::TotalHeight, th);
        S(Fd::TotalWidth, tw);
        for (size_t k = 0; k < aCount; ++k) H(Op::ItemSetPos, this->mItems[k], aTops[k]);
        this->AdjustPositionV(false);
    }
    bool Sort()
    {
        if (this->mSortType == ListSortType_None) return false;
        std::vector<Item> v = this->mItems;
        H(Op::SortItems, v.empty() ? 0 : (intptr_t)&v[0], v.size(), this->mSortType);
        this->mItems = v;
        this->EnforceInvariants();
        return true;
    }
    int FindInsertPos(Item item, int position)
    {
        if (!this->mDoingMultiChange && this->mSortType != ListSortType_None) {
            std::vector<Item> v = this->mItems;
            return (int)H(Op::LowerBound, v.empty() ? 0 : (intptr_t)&v[0], v.size(), item, this->mSortType);
        }
        if (position < 0 || (unsigned int)position > this->mItems.size()) position = (int)this->mItems.size();
        return position;
    }
    void AdjustPosition(bool scrollbar)
    {
        S(Fd::ExtraHeight, 0);
        int th = G(Fd::TotalHeight), sh = G(Fd::ScrollHeight);
        if (this->ListFlagSet(0x40) && th > sh) {
            size_t i = this->FindLineAt(th - sh + 1);
            if (i != this->mItems.size()) {
                Item item = this->mItems[i];
                int t = Top(item), h = Height(item);
                S(Fd::ExtraHeight, ee_won_gui_list_full_line_extra(th, sh, t, h));
            }
        }
        H(Op::BaseAdjustPosition, scrollbar);
        this->FindTopLine();
        if (!this->ListFlagSet(0x40) || (unsigned int)this->mTopLineVectorPos >= this->mItems.size()) return;
        Item item = this->mItems[this->mTopLineVectorPos];
        S(Fd::VertOffset, Top(item));
        S(Fd::DownScrollAmount, Height(item));
        if (this->mTopLineVectorPos > 0) S(Fd::UpScrollAmount, Height(this->mItems[this->mTopLineVectorPos - 1]));
        H(Op::BaseAdjustPosition, 1);
    }
    void Virtual08() { H(Op::BaseSizeChanged); this->AdjustPositionV(false); }
    void ScrollPaint()
    {
        int paintWidth = this->mPaintWidth;
        EeWonDrawOp ops[200];
        size_t n = ee_won_gui_draw_list(G(Fd::TotalWidth), G(Fd::ScrollWidth), this->mPaintWidth, (size_t)this->mTopLineVectorPos,
                                        this->mItems.size(), G(Fd::VertOffset), G(Fd::ScrollHeight), this->ListFlagSet(0x80),
                                        PlaceOf, &this->mItems, &paintWidth, ops, 200);
        this->mPaintWidth = paintWidth;
        std::vector<Item> items = this->mItems;
        for (size_t i = 0; i < n && i < 200; i++) {
            if (ops[i].kind == 16) H(Op::ItemPaint, (size_t)ops[i].b < items.size() ? items[ops[i].b] : 0, kG);
            else H(Op::Draw, kG, (intptr_t)&ops[i]);
        }
        this->mPaintWidth = -1;
    }
    void CheckMouseSel(Item item, int x, int y, bool down)
    {
        if (item == 0) { if (this->ListFlagSet(4)) this->ClearSelectionPrv(true); return; }
        if (!this->IsItemVisible(item)) return;
        int ry = y - Top(item);
        if (!H(Op::ItemCanSelect, item, x, ry)) { if (this->ListFlagSet(4)) this->ClearSelectionPrv(true); return; }
        this->mKeyItem = item;
        bool add = false;
        if (down && this->ListFlagSet(0x400)) {
            int modifiers = (int)H(Op::RootKeyMod);
            if (modifiers & 4) {
                add = true; SetSelected(item, !Selected(item)); this->Invalidate();
            } else if ((modifiers & 1) && this->mSelItem != 0) {
                int a = this->GetItemPos(this->mSelItem);
                int b = this->GetItemPos(item);
                this->SelectRange(a, b);
                return;
            } else {
                bool noChange = this->mSelItem == item && this->ClearSelectionPrv(false) == 1;
                SetSelected(item, true);
                if (noChange) this->mSelItem = item;
            }
        } else if (this->ListFlagSet(2) || !this->ListFlagSet(8)) SetSelected(item, true);
        else SetSelected(item, !Selected(item));
        this->SetSelItem(item, add, true);
    }
    Item FindSelectableItem(int start, bool up, bool opposite)
    {
        long long i = ee_won_gui_list_find_selectable(this->mItems.size(), start, up, opposite, this->ListFlagSet(0x800),
                                                      SelectableOf, &this->mItems);
        return i < 0 ? 0 : this->mItems[(size_t)i];
    }
    Item InsertItem(Item item, int position)
    {
        position = this->FindInsertPos(item, position);
        intptr_t viewport = H(Op::GetViewport);
        H(Op::ItemSetViewport, item, viewport);
        H(Op::ItemCalcDimensions, item);
        if (Height(item) <= 0) H(Op::ItemSetHeight, item, 1);
        SetSelected(item, false);
        bool end = (unsigned int)position == this->mItems.size();
        H(Op::ItemHold, item);
        this->mItems.insert(this->mItems.begin() + position, item);
        if (this->mSelItemItr >= position) ++this->mSelItemItr;
        bool fixed = this->ListFlagSet(0x1000);
        if (end) {
            H(Op::ItemSetPos, item, G(Fd::TotalHeight));
            S(Fd::TotalHeight, G(Fd::TotalHeight) + Height(item));
            int w = Width(item);
            if (w > G(Fd::TotalWidth)) S(Fd::TotalWidth, w);
            bool onBottom = H(Op::OnBottom) != 0;
            int h = Height(item);
            S(Fd::VertOffset, ee_won_gui_list_insert_scroll(true, fixed, onBottom, this->mTopLineVectorPos, position,
                G(Fd::TotalHeight), G(Fd::ScrollHeight), G(Fd::VertOffset), h));
            this->AdjustPositionV(false);
        } else {
            bool onBottom = H(Op::OnBottom) != 0;
            int h = Height(item);
            S(Fd::VertOffset, ee_won_gui_list_insert_scroll(false, fixed, onBottom, this->mTopLineVectorPos, position,
                G(Fd::TotalHeight), G(Fd::ScrollHeight), G(Fd::VertOffset), h));
            if (this->mDoingMultiChange) {
                int prev = position == 0 ? 0 : Top(this->mItems[position - 1]);
                int next = Top(this->mItems[position + 1]);
                H(Op::ItemSetTop, item, ee_won_gui_list_batch_top(position != 0, prev, next));
            } else this->EnforceInvariants();
        }
        return item;
    }
    bool Less(Item a, Item b) { return H(Op::Less, a, b, this->mSortType) != 0; }
    void RefreshItem(Item item)
    {
        int oldHeight = Height(item);
        intptr_t viewport = H(Op::GetViewport);
        H(Op::ItemSetViewport, item, viewport);
        H(Op::ItemCalcDimensions, item);
        if (Height(item) <= 0) H(Op::ItemSetHeight, item, 1);
        if (this->mDoingMultiChange) return;
        int position = this->GetItemPos(item);
        if (position >= 0 && this->mSortType != ListSortType_None) {
            Item prev = this->GetItem(position - 1);
            Item next = this->GetItem(position + 1);
            bool sort = false;
            if (prev != 0 && this->Less(item, prev)) sort = true;
            else if (next != 0 && this->Less(next, item)) sort = true;
            if (sort) {
                if (prev != 0) this->mItems[position] = prev;
                else this->mItems[position] = next;
                int insert = ee_won_gui_list_refresh_place(position, this->FindInsertPos(item, 0), this->mItems.size());
                if (position > insert) {
                    for (int i = position; i > insert; --i) this->mItems[i] = this->mItems[i - 1];
                } else if (position < insert) {
                    for (int i = position; i < insert; ++i) this->mItems[i] = this->mItems[i + 1];
                }
                this->mItems[insert] = item;
            }
        }
        if (position < 0) return;
        bool onBottom = H(Op::OnBottom) != 0;
        int t = Top(item), h = Height(item);
        S(Fd::VertOffset, ee_won_gui_list_refresh_scroll(this->ListFlagSet(0x1000), onBottom, t, G(Fd::VertOffset),
            G(Fd::TotalHeight), G(Fd::ScrollHeight), h, oldHeight));
        this->EnforceInvariants();
    }
    void DeleteItemPrv(int position)
    {
        Item item = this->mItems[position];
        H(Op::ItemSetViewport, item, 0);
        int t = Top(item), h = Height(item);
        S(Fd::VertOffset, ee_won_gui_list_delete_scroll(this->ListFlagSet(0x1000), t, G(Fd::VertOffset), h));
        if (item == this->mSelItem) { SetSelected(item, false); this->SetSelItemAt(-1, true); }
        if (item == this->mKeyItem) this->mKeyItem = 0;
        if (item == this->mMouseItem) this->NotifyMouseItemGoneV();
    }
    void RemoveItem(Item item)
    {
        int position = this->GetItemPos(item);
        if (position >= 0) this->DeleteRange(position, position + 1);
    }
    void DeleteRange(int start, int end)
    {
        int amount = 0;
        if (!ee_won_gui_list_delete_range(start, end, this->mItems.size(), &start, &amount)) return;
        for (int i = 0; i < amount; ++i) this->DeleteItemPrv(start + i);
        if (this->mSelItemItr >= start) this->mSelItemItr -= amount;
        std::vector<Item> gone(this->mItems.begin() + start, this->mItems.begin() + start + amount);
        this->mItems.erase(this->mItems.begin() + start, this->mItems.begin() + start + amount);
        for (size_t i = 0; i < gone.size(); i++) H(Op::ItemRelease, gone[i]);
        this->EnforceInvariants();
    }
    bool MouseContains(Item item, int x, int y, bool widthCheck)
    {
        if (!H(Op::ItemContains, item, x, y)) return false;
        if (widthCheck && x > Width(item)) return false;
        return true;
    }
    void ItemMouseAt(int& x, int& y)
    {
        H(Op::ScrollTranslate, (intptr_t)&x, (intptr_t)&y);
        bool check = this->ListFlagSet(0x200);
        if (this->mMouseItem != 0 && !this->MouseContains(this->mMouseItem, x, y, check)) {
            H(Op::ItemMouseExit, this->mMouseItem);
            H(Op::SetCursorDefault);
            this->NotifyMouseItemGoneV();
        }
        if (this->mMouseItem != 0) return;
        this->mMouseItem = this->FindItemAt(y);
        if (this->mMouseItem == 0) return;
        if (this->MouseContains(this->mMouseItem, x, y, check)) H(Op::ItemMouseEnter, this->mMouseItem, x, y - Top(this->mMouseItem));
        else this->mMouseItem = 0;
    }
    void MouseDrag(int x, int y)
    {
        H(Op::BaseMouseDrag, x, y);
        if (this->ListFlagSet(2)) { this->ItemMouseAt(x, y); this->CheckMouseSel(this->mMouseItem, x, y, false); }
        else H(Op::ScrollTranslate, (intptr_t)&x, (intptr_t)&y);
        if (this->mMouseItem != 0) H(Op::ItemMouseDrag, this->mMouseItem, x, y - Top(this->mMouseItem));
    }
    bool KeyDown(int theKey)
    {
        if (H(Op::BaseKeyDown, theKey)) return true;
        if (this->mKeyItem == 0) {
            if (theKey == VK_DOWN || theKey == VK_UP || theKey == VK_NEXT || theKey == VK_PRIOR) {
                Item anItem = this->FindItemAt(G(Fd::VertOffset));
                if (anItem == 0) return false;
                int aPos = this->GetItemPos(anItem);
                anItem = this->FindSelectableItem(aPos, theKey == VK_UP || theKey == VK_PRIOR, true);
                if (anItem != 0) { this->mKeyItem = anItem; SetSelected(anItem, true); this->SetSelItem(anItem, false, true); }
                return true;
            }
            return false;
        }
        int y = Top(this->mKeyItem);
        int anItemPos = -1;
        bool up = false;
        if (theKey == VK_RIGHT) S(Fd::HorzOffset, G(Fd::HorzOffset) + 10);
        else if (theKey == VK_LEFT) S(Fd::HorzOffset, G(Fd::HorzOffset) - 10);
        int h = Height(this->mKeyItem);
        if (!ee_won_gui_list_key_target(theKey, y, h, G(Fd::ScrollHeight), G(Fd::TotalHeight), this->ListFlagSet(0x800), &anItemPos, &up))
            return false;
        if (anItemPos != -1) {
            size_t anItr = this->FindLineAt(anItemPos);
            if (anItr == this->mItems.size()) return false;
            Item anItem = this->mItems[anItr];
            int aPos = this->GetItemPos(anItem);
            anItem = this->FindSelectableItem(aPos, up, true);
            if (anItem != 0) {
                if (this->mSelItem != 0 && this->ListFlagSet(0x400) && (H(Op::RootKeyMod) & 1)) {
                    int a = this->GetItemPos(this->mSelItem);
                    int b = this->GetItemPos(anItem);
                    this->SelectRange(a, b);
                } else { SetSelected(anItem, true); this->SetSelItem(anItem, false, true); }
                this->mKeyItem = anItem;
                int t = Top(anItem), hh = Height(anItem);
                S(Fd::VertOffset, ee_won_gui_list_key_scroll(t, hh, G(Fd::VertOffset), G(Fd::ScrollHeight), G(Fd::ExtraHeight)));
            }
        }
        this->AdjustPositionV(false);
        return true;
    }
    void SetListFlags(int flags, bool on)
    {
        bool over = this->ListFlagSet(4);
        if (on) this->mListFlags |= flags; else this->mListFlags &= ~flags;
        if (over || !this->ListFlagSet(4)) return;
        if (H(Op::PointerOutside)) this->ClearSelectionPrv(true);
    }
    void MouseMove(int x, int y)
    {
        H(Op::BaseMouseMove, x, y);
        this->ItemMouseAt(x, y);
        if (this->ListFlagSet(2)) this->CheckMouseSel(this->mMouseItem, x, y, false);
        if (this->mMouseItem != 0) H(Op::ItemMouseMove, this->mMouseItem, x, y - Top(this->mMouseItem));
        if (!this->ListFlagSet(0x2000)) return;
        bool show = false;
        if (this->mMouseItem != 0) {
            int w = Width(this->mMouseItem), h = Height(this->mMouseItem), t = Top(this->mMouseItem);
            show = ee_won_gui_list_item_tip(w, h, t, G(Fd::ScrollWidth), G(Fd::VertOffset), G(Fd::ScrollHeight));
        }
        if (show) H(Op::TipShow, this->mMouseItem);
        else H(Op::TipHide);
    }
    void Deliver(Item item, int button, bool up, int x, int y)
    {
        int ry = y - Top(item);
        H(up ? Op::ItemMouseUp : Op::ItemMouseDown, item, x, ry, button);
        EeWonItemClick k;
        k.button = button; k.up = up; k.x = x; k.y = y - Top(item);
        H(Op::DeliverItemClicked, item, (intptr_t)&k);
    }
    void MouseDown(int x, int y, int button)
    {
        H(Op::BaseMouseDown, x, y, button);
        this->ItemMouseAt(x, y);
        if (button != 1) { H(Op::RequestFocus); this->CheckMouseSel(this->mMouseItem, x, y, true); }
        if (this->mMouseItem != 0) this->Deliver(this->mMouseItem, button, false, x, y);
    }
    void MouseUp(int x, int y, int button)
    {
        H(Op::BaseMouseUp, x, y, button);
        this->ItemMouseAt(x, y);
        if (this->mMouseItem != 0) this->Deliver(this->mMouseItem, button, true, x, y);
    }
    void MouseEnter(int x, int y) { H(Op::BaseMouseEnter, x, y); this->ItemMouseAt(x, y); }
    void MouseExit()
    {
        H(Op::BaseMouseExit);
        if (this->mMouseItem != 0) { H(Op::ItemMouseExit, this->mMouseItem); this->NotifyMouseItemGoneV(); }
        if (this->ListFlagSet(4)) this->ClearSelectionPrv(true);
        if (this->ListFlagSet(0x2000)) H(Op::TipHide);
    }
    void SetSortType(int type)
    {
        if (this->mSortType == type) return;
        this->mSortType = type;
        this->SortV();
    }
};
} // namespace orig

static orig::ListArea* sOrig;
static const EeWonList* sRust;
typedef EeWonListCall C;
static void OrigAdjust(bool b) { sOrig->AdjustPosition(b); }
static bool OrigSort() { return sOrig->Sort(); }
static void OrigGone() { sOrig->mMouseItem = 0; }
static void RustAdjust(bool b) { ee_won_list_call(sRust, C::AdjustPosition, b, 0, 0); }
static bool RustSort() { return ee_won_list_call(sRust, C::Sort, 0, 0, 0) != 0; }
static void RustGone() { ee_won_list_call(sRust, C::NotifyMouseItemGone, 0, 0, 0); }

enum { kInsert, kInsertEnd, kBatch, kRefresh, kDelete, kRemove, kRange, kClear, kFlags, kSelAt, kSel, kEnsure, kClearSel,
       kSelRange, kFindAt, kVisible, kEnforce, kWalk, kPosAtY, kSelectable, kSortType, kSort, kSize, kAdjust, kPaint,
       kEnter, kExit, kDown, kUp, kMove, kDrag, kKey, kItemPos, kGetItem, kNum, kGone, kCalls };
static const char* kCallNames[kCalls] = {"insert", "insert-end", "batch", "refresh", "delete", "remove", "range", "clear",
    "flags", "sel-at", "sel", "ensure", "clear-sel", "sel-range", "find-at", "visible", "enforce", "walk", "pos-at-y",
    "selectable", "sort-type", "sort", "size", "adjust", "paint", "enter", "exit", "down", "up", "move", "drag", "key",
    "item-pos", "get-item", "num", "gone"};
static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }

// an item of the host (mostly one in the list), or 0
static intptr_t SomeItem()
{
    std::vector<intptr_t> in;
    for (std::map<intptr_t, HostItem>::const_iterator i = sItems.begin(); i != sItems.end(); ++i)
        if (i->second.refs > 0) in.push_back(i->first);
    if (in.empty() || DR(10) == 0) return sItems.empty() ? 0 : 0x1001 + (intptr_t)DR((unsigned)sItems.size());
    return in[DR((unsigned)in.size())];
}

static intptr_t Do(bool inRust, int k, intptr_t a, intptr_t b, intptr_t c)
{
    if (inRust) {
        const EeWonList* t = sRust;
        switch (k) {
        case kInsert: case kInsertEnd: return ee_won_list_call(t, C::InsertItem, a, b, 0);
        case kRefresh: ee_won_list_call(t, C::RefreshItem, a, 0, 0); return 0;
        case kDelete: ee_won_list_call(t, C::DeleteItem, a, 0, 0); return 0;
        case kRemove: ee_won_list_call(t, C::RemoveItem, a, 0, 0); return 0;
        case kRange: ee_won_list_call(t, C::DeleteRange, a, b, 0); return 0;
        case kClear: ee_won_list_call(t, C::Clear, a, 0, 0); return 0;
        case kFlags: ee_won_list_call(t, C::SetListFlags, a, b, 0); return ee_won_list_call(t, C::ListFlags, 0, 0, 0);
        case kSelAt: ee_won_list_call(t, C::SetSelItemAt, a, b, 0); return ee_won_list_call(t, C::SelItem, 0, 0, 0);
        case kSel: return ee_won_list_call(t, C::SetSelItem, a, b, c);
        case kEnsure: ee_won_list_call(t, C::EnsureVisible, a, 0, 0); return 0;
        case kClearSel: ee_won_list_call(t, C::ClearSelection, a, 0, 0); return 0;
        case kSelRange: ee_won_list_call(t, C::SelectRange, a, b, 0); return 0;
        case kFindAt: return ee_won_list_call(t, C::FindItemAt, a, 0, 0);
        case kVisible: return ee_won_list_call(t, C::IsItemVisible, a, 0, 0);
        case kEnforce: ee_won_list_call(t, C::EnforceInvariants, 0, 0, 0); return 0;
        case kWalk: {
            ee_won_list_call(t, C::RewindSelections, 0, 0, 0);
            intptr_t sum = 0;
            while (ee_won_list_call(t, C::HasMoreSelections, 0, 0, 0)) sum = sum * 3 + ee_won_list_call(t, C::GetNextSelection, 0, 0, 0);
            return sum;
        }
        case kPosAtY: return ee_won_list_call(t, C::GetItemPosAtYPos, a, 0, 0);
        case kSelectable: return ee_won_list_call(t, C::FindSelectableItem, a, b, c);
        case kSortType: ee_won_list_call(t, C::SetSortType, a, 0, 0); return ee_won_list_call(t, C::SortType, 0, 0, 0);
        case kSort: return ee_won_list_call(t, C::Sort, 0, 0, 0);
        case kSize: ee_won_list_call(t, C::SizeChanged, 0, 0, 0); return 0;
        case kAdjust: ee_won_list_call(t, C::AdjustPosition, a, 0, 0); return 0;
        case kPaint: ee_won_list_call(t, C::ScrollPaint, orig::kG, 0, 0); return ee_won_list_call(t, C::PaintWidth, 0, 0, 0);
        case kEnter: ee_won_list_call(t, C::MouseEnter, a, b, 0); return ee_won_list_call(t, C::MouseItem, 0, 0, 0);
        case kExit: ee_won_list_call(t, C::MouseExit, 0, 0, 0); return 0;
        case kDown: ee_won_list_call(t, C::MouseDown, a, b, c); return ee_won_list_call(t, C::SelItem, 0, 0, 0);
        case kUp: ee_won_list_call(t, C::MouseUp, a, b, c); return 0;
        case kMove: ee_won_list_call(t, C::MouseMove, a, b, 0); return ee_won_list_call(t, C::MouseItem, 0, 0, 0);
        case kDrag: ee_won_list_call(t, C::MouseDrag, a, b, 0); return 0;
        case kKey: return ee_won_list_call(t, C::KeyDown, a, 0, 0);
        case kItemPos: return ee_won_list_call(t, C::GetItemPos, a, 0, 0);
        case kGetItem: return ee_won_list_call(t, C::GetItem, a, 0, 0);
        case kNum: return ee_won_list_call(t, C::NumItems, 0, 0, 0);
        case kGone: ee_won_list_call(t, C::ClearMouseItem, 0, 0, 0); return 0;
        }
    } else {
        orig::ListArea* t = sOrig;
        switch (k) {
        case kInsert: case kInsertEnd: return t->InsertItem(a, (int)b);
        case kRefresh: t->RefreshItem(a); return 0;
        case kDelete: t->DeleteRange((int)a, (int)a + 1); return 0;
        case kRemove: t->RemoveItem(a); return 0;
        case kRange: t->DeleteRange((int)a, (int)b); return 0;
        case kClear: t->Clear(a != 0); return 0;
        case kFlags: t->SetListFlags((int)a, b != 0); return t->mListFlags;
        case kSelAt: t->SetSelItemAt((int)a, b != 0); return t->mSelItem;
        case kSel: return t->SetSelItem(a, b != 0, c != 0);
        case kEnsure: t->EnsureVisible(a); return 0;
        case kClearSel: t->ClearSelectionPrv(a != 0); return 0;
        case kSelRange: t->SelectRange((int)a, (int)b); return 0;
        case kFindAt: return t->FindItemAt((int)a);
        case kVisible: return t->IsItemVisible(a);
        case kEnforce: t->EnforceInvariants(); return 0;
        case kWalk: {
            t->RewindSelections();
            intptr_t sum = 0;
            while (t->HasMoreSelections()) sum = sum * 3 + t->GetNextSelection();
            return sum;
        }
        case kPosAtY: return (intptr_t)t->FindLineAt((int)a);
        case kSelectable: return t->FindSelectableItem((int)a, b != 0, c != 0);
        case kSortType: t->SetSortType((int)a); return t->mSortType;
        case kSort: return t->Sort();
        case kSize: t->Virtual08(); return 0;
        case kAdjust: t->AdjustPosition(a != 0); return 0;
        case kPaint: t->ScrollPaint(); return t->mPaintWidth;
        case kEnter: t->MouseEnter((int)a, (int)b); return t->mMouseItem;
        case kExit: t->MouseExit(); return 0;
        case kDown: t->MouseDown((int)a, (int)b, (int)c); return t->mSelItem;
        case kUp: t->MouseUp((int)a, (int)b, (int)c); return 0;
        case kMove: t->MouseMove((int)a, (int)b); return t->mMouseItem;
        case kDrag: t->MouseDrag((int)a, (int)b); return 0;
        case kKey: return t->KeyDown((int)a);
        case kItemPos: return t->GetItemPos(a);
        case kGetItem: return t->GetItem((int)a);
        case kNum: return (intptr_t)t->mItems.size();
        case kGone: t->mMouseItem = 0; return 0;
        }
    }
    return 0;
}

static void Run(bool inRust)
{
    static const int kWeights[kCalls] = {8, 6, 2, 3, 2, 2, 1, 1, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 2,
                                         1, 1, 4, 2, 4, 2, 4, 1, 1, 1, 1};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r0 = (int)DR(aTotal), k = 0;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    long long a = 0, b = 0, c = 0;
    int w = sFields[(int)Fd::ScrollWidth] + 20, h = sFields[(int)Fd::TotalHeight] + 40;
    switch (k) {
    case kInsert: a = NewItem(); b = (int)DR(12) - 2; break;
    case kInsertEnd: a = NewItem(); b = -1; break;
    case kRefresh: a = SomeItem(); if (a) { sItems[(intptr_t)a].calcH = 6 + (int)DR(16); sItems[(intptr_t)a].key = (int)DR(12); } break;
    case kDelete: a = (int)DR(14) - 1; break;
    case kRemove: case kEnsure: case kVisible: case kItemPos: a = SomeItem(); break;
    case kRange: a = (int)DR(10) - 1; b = (int)DR(14) - 2; break;
    case kClear: case kClearSel: a = DR(2); break;
    case kFlags: { static const int kF[] = {2, 4, 8, 0x40, 0x80, 0x200, 0x400, 0x800, 0x1000, 0x2000}; a = kF[DR(10)]; b = DR(3) != 0; break; }
    case kSelAt: a = (int)DR(14) - 2; b = DR(2); break;
    case kSel: a = DR(6) == 0 ? 0 : SomeItem(); b = DR(2); c = DR(2); break;
    case kSelRange: a = (int)DR(14) - 2; b = (int)DR(14) - 2; break;
    case kFindAt: case kPosAtY: a = (int)DR(h) - 10; break;
    case kSelectable: a = (int)DR(14) - 2; b = DR(2); c = DR(2); break;
    case kSortType: a = DR(3); break;
    case kAdjust: a = DR(2); break;
    case kEnter: case kMove: case kDrag: a = (int)DR(w) - 10; b = (int)DR(sFields[(int)Fd::ScrollHeight] + 20) - 10; break;
    case kDown: case kUp: a = (int)DR(w) - 10; b = (int)DR(sFields[(int)Fd::ScrollHeight] + 20) - 10; c = DR(5) == 0 ? 1 : 0; break;
    case kKey: { static const int kK[] = {0x21, 0x22, 0x25, 0x26, 0x27, 0x28, 0x41}; a = kK[DR(7)]; break; }
    case kGetItem: a = (int)DR(14) - 2; break;
    default: break;
    }
    char aLine[160];
    snprintf(aLine, sizeof(aLine), "call %s %llx %llx %llx", kCallNames[k], a, b, c);
    sTrace.push_back(aLine);
    size_t aCallLine = sTrace.size() - 1;
    intptr_t r = 0;
    if (k == kBatch) {
        if (inRust) ee_won_list_call(sRust, C::BeginUpdate, 0, 0, 0); else sOrig->BeginUpdate();
        int n = 1 + (int)DR(4);
        for (int i = 0; i < n; i++) {
            intptr_t it = NewItem();
            int pos = (int)DR(8) - 1;
            snprintf(aLine, sizeof(aLine), "call batch-insert %llx %x 0", (long long)it, pos);
            sTrace.push_back(aLine);
            Do(inRust, kInsert, it, pos, 0);
            sItems[it].refs--; // the creator's reference goes
        }
        if (inRust) ee_won_list_call(sRust, C::EndUpdate, 0, 0, 0); else sOrig->EndUpdate();
    } else {
        r = Do(inRust, k, (intptr_t)a, (intptr_t)b, (intptr_t)c);
        if (k == kInsert || k == kInsertEnd) sItems[(intptr_t)a].refs--;
    }
    sTrace[aCallLine] += " -> " + std::to_string((long long)r);
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sItems.clear();
    sNextItem = 0;
    sSeed = inSeq * 2654435761u + 37u;
    sDriverSeed = inSeq * 40503u + 17u;
    memset(sFields, 0, sizeof(sFields));
    sFields[(int)Fd::ScrollWidth] = 40 + (int)R(260);
    sFields[(int)Fd::ScrollHeight] = 20 + (int)R(180);
    sFields[(int)Fd::Width] = sFields[(int)Fd::ScrollWidth] + 4;
    sFields[(int)Fd::Height] = sFields[(int)Fd::ScrollHeight] + 4;
    sFields[(int)Fd::UpScrollAmount] = sFields[(int)Fd::DownScrollAmount] = 10;
    TraceFields();
    if (inRust) {
        sSide.mAdjust = RustAdjust; sSide.mSort = RustSort; sSide.mGone = RustGone;
        EeWonList* t = ee_won_list_new((void*)0x55, ScriptedHost);
        sRust = t;
        for (int i = 0; i < inCalls; i++) Run(true);
        sTrace.push_back("free");
        ee_won_list_free(t);
        sRust = NULL;
    } else {
        sSide.mAdjust = OrigAdjust; sSide.mSort = OrigSort; sSide.mGone = OrigGone;
        sOrig = new orig::ListArea;
        for (int i = 0; i < inCalls; i++) Run(false);
        sTrace.push_back("free");
        delete sOrig;
        sOrig = NULL;
    }
    bool aBack = true;
    for (std::map<intptr_t, HostItem>::const_iterator i = sItems.begin(); i != sItems.end(); ++i)
        if (i->second.refs != 0) aBack = false;
    sTrace.push_back(aBack ? "refs back" : "refs LEAKED");
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCalls = aPrint ? 16 : 40;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sOpsMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCalls);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCalls);
        aLines += a.size();
        CHECK(a.back() == "refs back");
        bool same = a == b;
        CHECK(same);
        if (!same) {
            aDiffering++;
            if (aDiffering <= 3) {
                printf("sequence %d differs\n", seq);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "<end>";
                    const char* y = i < b.size() ? b[i].c_str() : "<end>";
                    if (strcmp(x, y) != 0) { printf("  line %u\n  orig: %s\n  rust: %s\n", (unsigned)i, x, y); break; }
                }
            }
        }
        bool aNew = seq < 40;
        for (std::set<int>::const_iterator i = sOpsMet.begin(); i != sOpsMet.end(); ++i)
            if (aSeen.insert("op" + std::to_string(*i)).second) aNew = true;
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].compare(0, 5, "call ") == 0)
                if (aSeen.insert(a[i].substr(0, a[i].find(' ', 5))).second) aNew = true;
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_listobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
