// The tree list as whole objects (R6 WON round 24): the original TreeArea.cpp, TreeItem.cpp and TreeItemLifetime.cpp,
// the statements copied from the C++ before the port with every call on a C++ object (the ListArea base, the rows'
// and wrapped items' fields and virtuals, the new rows, the cursor, the events, the Graphics) replaced by the op that
// makes it (GuiHost.cpp runs the same calls in the same order; the paint's dotted lines are one draw operation each),
// against ee_won's Rust TreeArea and TreeItem (treeobj.rs) over one scripted host. Both sides' ListArea base is the
// Rust list (listobj.rs), as in the game: its calls back (the list's virtuals, the rows' virtuals) go to the side's
// objects, and a row goes with its last reference (its children's and wrapped item's references back).
//
// Each of 20,000 sequences creates the tree and makes 40 random inward calls (rows inserted under the root and under
// rows, sorted and placed; removals, clears, opens, closes and toggles with and without the subtree and the event,
// sorts of the tree and of rows, tree flags, the expand button's event, the list's mouse over the rows and their
// +/- boxes, paints, size changes, refreshes, the rows' walks and queries). The traces must be the same, and every
// reference taken must be given back when the tree goes. The first 40 sequences and every later one that makes an
// op or a call none before made are rust/crates/ee_won/src/golden_treeobj.txt, which the Rust tests replay;
// `--print` prints them.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;
typedef EeWonListCall LC;
typedef EeWonTreeCall TC;
typedef EeWonTNodeCall NC;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

static const intptr_t kTree = 0x55, kG = 0x6;

struct HostItem {
    int top, height, width;
    bool selected, selectable, row;
    int calcH, calcW, key, refs;
};
struct HostEvent { int type, controlId, top; };
static std::map<intptr_t, HostItem> sItems;
static std::map<intptr_t, HostEvent> sEvents;
static intptr_t sNextItem, sNextEvent;
static int sFields[16];
// the trace (echoed to stderr with WON_ECHO set, to find a crash)
struct Trace : std::vector<std::string> {
    void push_back(const std::string& s)
    {
        static const bool echo = getenv("WON_ECHO") != NULL;
        if (echo) { fprintf(stderr, "%s\n", s.c_str()); fflush(stderr); }
        std::vector<std::string>::push_back(s);
    }
};
static Trace sTrace;
static std::set<int> sOpsMet;

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

struct Side {
    virtual ~Side() {}
    virtual void Gone() = 0;                  // TreeArea::NotifyMouseItemGone
    virtual intptr_t NodePtr(intptr_t id) = 0; // ((TreeItem*)id)->mNode
    virtual void NewNode(intptr_t id) = 0;
    virtual void FreeNode(intptr_t id) = 0;
    virtual intptr_t Node(NC c, intptr_t row, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) = 0;
    virtual intptr_t Tree(TC c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) = 0;
};
static Side* sSide;
static EeWonList* sList;

static bool LineLess(intptr_t a, intptr_t b, int type)
{
    int r = (int)sSide->Node(NC::Compare, a, b);
    return type == 0 ? r < 0 : r > 0;
}

static intptr_t NewRowId()
{
    HostItem h;
    h.top = 0; h.height = 0; h.width = 0;
    h.selected = false; h.selectable = true; h.row = true;
    h.calcH = 0; h.calcW = 0; h.key = 0;
    h.refs = 1; // the creator's SmartPtr
    intptr_t id = 0x2000 + (++sNextItem);
    sItems[id] = h;
    sSide->NewNode(id);
    return id;
}

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp;
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    int n = (int)inOp;
    switch (inOp) {
    case Op::Get: return sFields[a]; // pure reads: not traced
    case Op::NodeOf: return sSide->NodePtr(a); // a pointer: not traced
    case Op::Set: sFields[a] = (int)b; break;
    case Op::BaseSizeChanged:
        Line("%lld base-size", n, 0, 0, 0, 0);
        sFields[(int)Fd::ScrollWidth] = 20 + (int)R(280);
        sFields[(int)Fd::ScrollHeight] = 10 + (int)R(200);
        sFields[(int)Fd::Width] = sFields[(int)Fd::ScrollWidth] + 4;
        sFields[(int)Fd::Height] = sFields[(int)Fd::ScrollHeight] + 4;
        TraceFields();
        ee_won_list_call(sList, LC::AdjustPosition, 0, 0, 0); // ScrollArea::Virtual08 -> the virtual AdjustPosition
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
    case Op::VirtAdjustPosition: Line("%lld virt-adjust %lld", n, (long long)a, 0, 0, 0); ee_won_list_call(sList, LC::AdjustPosition, a, 0, 0); return 0;
    case Op::VirtSort: Line("%lld virt-sort -> 0", n, 0, 0, 0, 0); return 0; // TreeArea::Sort() is false
    case Op::VirtNotifyMouseItemGone: Line("%lld virt-gone", n, 0, 0, 0, 0); sSide->Gone(); return 0;
    case Op::SetVertOffset: sFields[(int)Fd::VertOffset] = (int)a; break;
    case Op::OnBottom: r = R(2); break;
    case Op::RootKeyMod: r = R(8); break;
    case Op::GetViewport: r = 0x77; break;
    case Op::PointerOutside: r = R(2); break;
    case Op::CheckDoubleClick: r = R(3) == 0; break;
    case Op::DeliverItemClicked: {
        const EeWonItemClick* k = (const EeWonItemClick*)b;
        Line("%lld click %llx %lld %lld %lld %lld", n, (long long)a, k->button, k->up, k->x, k->y);
        return 0;
    }
    case Op::SortItems: {
        intptr_t* v = (intptr_t*)a;
        int type = (int)c;
        Line("%lld sort-start %lld", n, type, 0, 0, 0);
        std::sort(v, v + b, [type](intptr_t x, intptr_t y) { return LineLess(x, y, type); });
        std::string s = std::to_string(n) + " sort";
        char buf[24];
        for (intptr_t i = 0; i < b; i++) { snprintf(buf, sizeof(buf), " %llx", (long long)v[i]); s += buf; }
        sTrace.push_back(s);
        return 0;
    }
    case Op::LowerBound: {
        const intptr_t* v = (const intptr_t*)a;
        int type = (int)d;
        Line("%lld lower-bound-start %llx %lld", n, (long long)c, (long long)d, 0, 0);
        r = std::lower_bound(v, v + b, c, [type](intptr_t x, intptr_t y) { return LineLess(x, y, type); }) - v;
        Line("%lld lower-bound -> %lld", n, (long long)r, 0, 0, 0);
        return r;
    }
    case Op::Less: Line("%lld less %llx %llx", n, (long long)a, (long long)b, 0, 0); r = LineLess(a, b, (int)c); break;
    case Op::Draw: {
        const EeWonDrawOp* o = (const EeWonDrawOp*)b;
        char aLine[200];
        snprintf(aLine, sizeof(aLine), "%d draw %llx %d %d %d %d %d", n, (long long)a, o->kind, o->a, o->b, o->c, o->d);
        sTrace.push_back(aLine);
        return 0;
    }
    case Op::NewTreeItem: r = NewRowId(); break;
    case Op::ItemTop: r = sItems[a].top; break;
    case Op::ItemSetTop: sItems[a].top = (int)b; break;
    case Op::ItemHeight: r = sItems[a].height; break;
    case Op::ItemSetHeight: sItems[a].height = (int)b; break;
    case Op::ItemWidth: r = sItems[a].width; break;
    case Op::ItemSetWidth: sItems[a].width = (int)b; break;
    case Op::ItemSelected: r = sItems[a].selected; break;
    case Op::ItemSetSelected: sItems[a].selected = b != 0; break;
    case Op::ItemSelectable: r = sItems[a].selectable; break;
    case Op::ItemSetSelectable: sItems[a].selectable = b != 0; break;
    case Op::ItemSetPosXY: sItems[a].top = (int)c; break;
    case Op::ItemSetPos:
        if (sItems[a].row) { Line("%lld row-set-pos %llx %lld", n, (long long)a, (long long)b, 0, 0); sSide->Node(NC::SetPos, a, 0, b); return 0; }
        sItems[a].top = (int)b;
        break;
    case Op::ItemContains:
        if (sItems[a].row) r = c >= sItems[a].top && c < sItems[a].top + sItems[a].height; // ListItem::Contains
        else r = b >= 0 && b < sItems[a].width && c >= sItems[a].top && c < sItems[a].top + sItems[a].height;
        break;
    case Op::ItemCanSelect:
        if (sItems[a].row) {
            Line("%lld row-can-select %llx %lld %lld", n, (long long)a, (long long)b, (long long)c, 0);
            r = sSide->Node(NC::CanSelect, a, b, c);
            Line("%lld row-can-select -> %lld", n, (long long)r, 0, 0, 0);
            return r;
        }
        r = sItems[a].selectable;
        break;
    case Op::ItemCalcDimensions:
        if (sItems[a].row) { Line("%lld row-calc %llx", n, (long long)a, 0, 0, 0); sSide->Node(NC::CalcDimensions, a); return 0; }
        sItems[a].height = sItems[a].calcH; sItems[a].width = sItems[a].calcW;
        break;
    case Op::ItemSetViewport:
        if (sItems[a].row) { Line("%lld row-viewport %llx %llx", n, (long long)a, (long long)b, 0, 0); sSide->Node(NC::SetViewport, a, b); return 0; }
        break;
    case Op::ItemPaint:
        if (sItems[a].row) { Line("%lld row-paint %llx", n, (long long)a, 0, 0, 0); sSide->Node(NC::Paint, a, b); return 0; }
        break;
    case Op::ItemMouseEnter:
        if (sItems[a].row) { Line("%lld row-enter %llx %lld %lld", n, (long long)a, (long long)b, (long long)c, 0); sSide->Node(NC::MouseEnter, a, b, c); return 0; }
        break;
    case Op::ItemMouseExit:
        if (sItems[a].row) { Line("%lld row-exit %llx", n, (long long)a, 0, 0, 0); sSide->Node(NC::MouseExit, a); return 0; }
        break;
    case Op::ItemMouseMove:
        if (sItems[a].row) { Line("%lld row-move %llx %lld %lld", n, (long long)a, (long long)b, (long long)c, 0); sSide->Node(NC::MouseMove, a, b, c); return 0; }
        break;
    case Op::ItemMouseDrag:
        if (sItems[a].row) { Line("%lld row-drag %llx %lld %lld", n, (long long)a, (long long)b, (long long)c, 0); sSide->Node(NC::MouseDrag, a, b, c); return 0; }
        break;
    case Op::ItemMouseDown:
        if (sItems[a].row) { Line("%lld row-down %llx %lld %lld %lld", n, (long long)a, (long long)b, (long long)c, (long long)d); sSide->Node(NC::MouseDown, a, b, c, d); return 0; }
        break;
    case Op::ItemMouseUp:
        if (sItems[a].row) { Line("%lld row-up %llx %lld %lld %lld", n, (long long)a, (long long)b, (long long)c, (long long)d); sSide->Node(NC::MouseUp, a, b, c, d); return 0; }
        break;
    case Op::ItemHold: sItems[a].refs++; break;
    case Op::ItemRelease:
        sItems[a].refs--;
        Line("%lld release %llx -> %lld", n, (long long)a, sItems[a].refs, 0, 0);
        if (sItems[a].row && sItems[a].refs == 0) sSide->FreeNode(a); // the row goes: its children and item back
        return 0;
    case Op::ItemCompare: { int ka = sItems[a].key, kb = sItems[b].key; r = ka < kb ? -1 : ka > kb ? 1 : 0; break; }
    case Op::EventType: r = sEvents[a].type; break;
    case Op::EventCompControlId: r = sEvents[a].controlId; break;
    case Op::EventComponentTop: r = sEvents[a].top; break;
    default: break;
    }
    Line("%lld %llx %llx %llx %llx -> %llx", n, (long long)a, (long long)b, (long long)c, (long long)d, (long long)r);
    return r;
}

static intptr_t HT(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    return ScriptedHost((void*)kTree, inOp, a, b, c, d);
}
static intptr_t Base(LC c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) { return ee_won_list_call(sList, c, a, b, cc); }
static void DrawOp(int kind, int a, int b, int c, int d)
{
    EeWonDrawOp o;
    memset(&o, 0, sizeof(o));
    o.kind = kind; o.a = a; o.b = b; o.c = c; o.d = d;
    HT(Op::Draw, kG, (intptr_t)&o);
}

// ---- the original (the three units; the ListArea base the Rust list, every other call through the host)

namespace orig {
enum { ListSortType_None = 2, StandardColor_3DShadow = 3, StandardColor_Text = 14 };
typedef intptr_t Item;
struct TreeItem;
struct TreeArea;
static TreeItem* NodeOf(Item id) { return (TreeItem*)sSide->NodePtr(id); }

struct TreeItem {
    Item mMe;
    Item mParent;
    int mX, mIndex;
    bool mOpen;
    Item mItem;
    std::vector<Item> mChildren;
    explicit TreeItem(Item me) : mMe(me) { this->mX = 0; this->mParent = 0; this->mIndex = -1; this->mOpen = false; this->mItem = 0; }
    ~TreeItem()
    {
        std::vector<Item> gone;
        gone.swap(this->mChildren);
        for (size_t i = 0; i < gone.size(); i++) Own(Op::ItemRelease, gone[i]);
        if (this->mItem) Own(Op::ItemRelease, this->mItem);
    }
    intptr_t Own(Op op, intptr_t a, intptr_t b = 0, intptr_t c = 0) { return ScriptedHost((void*)this->mMe, op, a, b, c, 0); }
    bool IsOpen() { return this->mOpen; }
    int Top() { return (int)HT(Op::ItemTop, this->mMe); }
    void SetItem(Item item)
    {
        if (this->mItem != item) {
            if (this->mItem) Own(Op::ItemRelease, this->mItem);
            this->mItem = item;
            if (item) Own(Op::ItemHold, item);
        }
    }
    void ClearChildren()
    {
        std::vector<Item> gone;
        gone.swap(this->mChildren);
        for (size_t i = 0; i < gone.size(); i++) Own(Op::ItemRelease, gone[i]);
    }
    Item GetMouseItem(TreeArea* aTree, int& x, int& y);
    void MouseEnter(TreeArea* t, int x, int y) { this->GetMouseItem(t, x, y); }
    void MouseExit(TreeArea* t);
    void MouseMove(TreeArea* t, int x, int y) { Item anItem = this->GetMouseItem(t, x, y); if (anItem) HT(Op::ItemMouseMove, anItem, x, y); }
    void MouseDrag(TreeArea* t, int x, int y);
    void MouseUp(TreeArea* t, int x, int y, int b) { Item anItem = this->GetMouseItem(t, x, y); if (anItem) HT(Op::ItemMouseUp, anItem, x, y, b); }
    void MouseDown(TreeArea* t, int x, int y, int b);
    Item GetChild(int thePos)
    {
        if (!ee_won_gui_tree_child_in_range(thePos, this->mChildren.size())) return 0;
        return this->mChildren[thePos];
    }
    Item GetNextNonChild();
    int GetNextNonChildPos(TreeArea*)
    {
        Item anItem = this->GetNextNonChild();
        if (anItem) return (int)Base(LC::GetItemPos, anItem);
        return -1;
    }
    int GetInsertPos(TreeArea* tree, Item item, int position);
    void InsertChild(TreeArea* t, Item theItem, int thePos);
    void RemoveAllChildren(TreeArea* t)
    {
        this->SetOpen(t, false, false, false);
        this->ClearChildren();
        HT(Op::Invalidate);
    }
    void RemoveChild(TreeArea* t, Item theItem);
    void RemoveFromParent(TreeArea* t) { if (this->mParent == 0) return; NodeOf(this->mParent)->RemoveChild(t, this->mMe); }
    void CloseAll();
    void InsertChildren(TreeArea* t, int thePos);
    void SetOpen(TreeArea* t, bool isOpen, bool closeAll, bool sendEvent);
    void ToggleOpen(TreeArea* t, bool closeAll, bool sendEvent) { this->SetOpen(t, !this->mOpen, closeAll, sendEvent); }
    bool IsLastChild() { return this->mParent != 0 && ee_won_gui_tree_is_last(this->mIndex, NodeOf(this->mParent)->mChildren.size()); }
    void SetPos(int y) { Own(Op::ItemSetTop, this->mMe, y); Own(Op::ItemSetPosXY, this->mItem, this->mX, y); }
    void Paint(TreeArea* t);
    void SetViewport(intptr_t v) { HT(Op::ItemSetViewport, this->mItem, v); }
    void CalcDimensions()
    {
        HT(Op::ItemCalcDimensions, this->mItem);
        HT(Op::ItemSetWidth, this->mMe, ee_won_gui_tree_row_width(this->mX, (int)HT(Op::ItemWidth, this->mItem)));
        HT(Op::ItemSetHeight, this->mMe, HT(Op::ItemHeight, this->mItem));
    }
    int Compare(Item other) { return (int)HT(Op::ItemCompare, this->mItem, NodeOf(other)->mItem); }
    bool CanSelect(int x) { return ee_won_gui_tree_can_select(HT(Op::ItemSelectable, this->mMe) != 0, x, this->mX); }
    bool ContainsPoint(int x, int y)
    {
        int aX;
        if (!ee_won_gui_tree_local_x(x, this->mX, &aX)) return false;
        return Own(Op::ItemContains, this->mItem, aX, y + (int)Own(Op::ItemTop, this->mMe)) != 0;
    }
    void Sort(TreeArea* t, bool recursive);
};

struct TreeArea {
    Item mRoot;
    int mTreeFlags;
    Item mMouseColItem;
    int mTreeSortType;
    TreeArea() : mRoot(0), mTreeFlags(0), mMouseColItem(0), mTreeSortType(ListSortType_None) {}
    void Init()
    {
        this->mRoot = HT(Op::NewTreeItem);
        NodeOf(this->mRoot)->SetOpen(this, true, false, false);
        Base(LC::SetListFlags, 0x30, 0);
        Base(LC::SetListFlags, 0x200, 1);
        this->mTreeFlags = 3;
        this->mMouseColItem = 0;
    }
    ~TreeArea() { if (this->mRoot) HT(Op::ItemRelease, this->mRoot); }
    bool TreeFlagSet(int f) { return (this->mTreeFlags & f) != 0; }
    void SetSortType(int theType)
    {
        if (this->mTreeSortType == theType) return;
        this->mTreeSortType = theType;
        HT(Op::VirtSort);
    }
    void NotifyMouseItemGone() { Base(LC::NotifyMouseItemGone); this->mMouseColItem = 0; }
    Item InsertTreeItem(Item theParent, Item theItem, int thePos)
    {
        if (theParent == 0) theParent = this->mRoot;
        Item anItem = HT(Op::NewTreeItem);
        NodeOf(anItem)->SetItem(theItem);
        HT(Op::ItemSetSelectable, anItem, HT(Op::ItemSelectable, theItem));
        NodeOf(theParent)->InsertChild(this, anItem, thePos);
        HT(Op::ItemRelease, anItem);
        return anItem;
    }
    void RemoveTreeItem(Item theItem) { NodeOf(theItem)->RemoveFromParent(this); }
    void RemoveAllItems(Item theParent)
    {
        if (theParent == 0) {
            Base(LC::Clear, 1);
            NodeOf(this->mRoot)->ClearChildren();
        } else NodeOf(theParent)->RemoveAllChildren(this);
    }
    void OpenItem(Item theItem, bool sendEvent) { NodeOf(theItem)->SetOpen(this, true, false, sendEvent); }
    void CloseItem(Item theItem, bool closeAll, bool sendEvent) { NodeOf(theItem)->SetOpen(this, false, closeAll, sendEvent); }
    void ToggleItem(Item theItem, bool closeAll, bool sendEvent) { NodeOf(theItem)->ToggleOpen(this, closeAll, sendEvent); }
    void HandleComponentEvent(intptr_t theEvent)
    {
        int type = (int)HT(Op::EventType, theEvent);
        if (ee_won_gui_tree_is_expand_press(type, (int)HT(Op::EventCompControlId, theEvent))) {
            Item anItem = Base(LC::FindItemAt, HT(Op::EventComponentTop, theEvent));
            if (anItem != 0) this->ToggleItem(anItem, this->TreeFlagSet(4), true);
        } else HT(Op::BaseHandleEvent, theEvent);
    }
    void Sort(Item theItem, bool recursive)
    {
        bool open = theItem != 0 && NodeOf(theItem)->IsOpen();
        uint8_t aPlan = ee_won_gui_tree_sort_plan(this->mTreeSortType == ListSortType_None, theItem == 0, open,
                                                  open ? (int)Base(LC::GetItemPos, theItem) : -1);
        if (aPlan == 0) return;
        if (aPlan == 1) {
            Base(LC::Clear, 0);
            NodeOf(this->mRoot)->Sort(this, recursive);
            NodeOf(this->mRoot)->InsertChildren(this, 0);
            return;
        }
        bool isVisible = aPlan == 3;
        if (isVisible) NodeOf(theItem)->SetOpen(this, false, false, false);
        NodeOf(theItem)->Sort(this, recursive);
        if (isVisible) NodeOf(theItem)->SetOpen(this, true, false, false);
    }
    void SetTreeFlags(int theFlags, bool on) { this->mTreeFlags = ee_won_gui_tree_flags(this->mTreeFlags, theFlags, on); }
};

static void* TreeOpParent(void*, void* n) { return (void*)NodeOf((Item)n)->mParent; }
static int TreeOpIndex(void*, void* n) { return NodeOf((Item)n)->mIndex; }
static size_t TreeOpCount(void*, void* n) { return NodeOf((Item)n)->mChildren.size(); }
static void* TreeOpChild(void*, void* n, size_t i) { return (void*)NodeOf((Item)n)->mChildren[i]; }
static bool TreeOpIsOpen(void*, void* n) { return NodeOf((Item)n)->IsOpen(); }
static void TreeOpInsert(void*, void* n, int pos) { Base(LC::InsertItem, (Item)n, pos); }
static void TreeOpClose(void*, void* n) { NodeOf((Item)n)->mOpen = false; }
static EeWonTreeOps TreeOps()
{
    EeWonTreeOps anOps = {NULL, TreeOpParent, TreeOpIndex, TreeOpCount, TreeOpChild, TreeOpIsOpen, TreeOpInsert, TreeOpClose};
    return anOps;
}
struct TreeMouseCtx { Item mItems[2]; int x, y; };
static bool TreeOpContains(void* ctx, int which)
{
    TreeMouseCtx* c = (TreeMouseCtx*)ctx;
    return HT(Op::ItemContains, c->mItems[which], c->x, c->y) != 0;
}

Item TreeItem::GetMouseItem(TreeArea* aTree, int& x, int& y)
{
    Item aNewMouseItem = this->mItem;
    Item aMouseColItem = aTree->mMouseColItem;
    TreeMouseCtx aCtx = {{aMouseColItem, aNewMouseItem}, x, y + this->Top()};
    uint8_t aSteps = ee_won_gui_tree_mouse_item(aMouseColItem != 0, aMouseColItem == aNewMouseItem, aNewMouseItem != 0,
                                                TreeOpContains, &aCtx);
    if (aSteps & 1) {
        HT(Op::ItemMouseExit, aMouseColItem);
        HT(Op::SetCursor, (intptr_t)EeWonGuiCursor::None);
        aTree->mMouseColItem = 0;
    }
    if (aSteps & 2) {
        aTree->mMouseColItem = aNewMouseItem;
        HT(Op::ItemMouseEnter, aNewMouseItem, x, y);
    }
    x -= this->mX;
    return aTree->mMouseColItem;
}
void TreeItem::MouseExit(TreeArea* t)
{
    Item anItem = t->mMouseColItem;
    if (anItem != 0) {
        HT(Op::ItemMouseExit, anItem);
        Base(LC::ClearMouseItem);
    }
}
void TreeItem::MouseDrag(TreeArea* t, int x, int y)
{
    Item anItem = t->mMouseColItem;
    if (anItem != 0) HT(Op::ItemMouseDrag, anItem, x - this->mX, y);
}
void TreeItem::MouseDown(TreeArea* t, int x, int y, int theButton)
{
    if (theButton == 0) {
        if (ee_won_gui_tree_box_hit(x, this->mX) || (HT(Op::CheckDoubleClick, theButton) && t->TreeFlagSet(8))) {
            this->ToggleOpen(t, t->TreeFlagSet(4), true);
            return;
        }
    }
    Item anItem = this->GetMouseItem(t, x, y);
    if (anItem != 0) HT(Op::ItemMouseDown, anItem, x, y, theButton);
}
Item TreeItem::GetNextNonChild()
{
    EeWonTreeOps anOps = TreeOps();
    return (Item)ee_won_gui_tree_next_non_child(&anOps, (void*)this->mMe);
}
int TreeItem::GetInsertPos(TreeArea* tree, Item item, int position)
{
    if (tree->mTreeSortType == ListSortType_None) return position;
    std::vector<Item> v = this->mChildren;
    return (int)HT(Op::LowerBound, v.empty() ? 0 : (intptr_t)&v[0], v.size(), item, tree->mTreeSortType);
}
void TreeItem::InsertChild(TreeArea* t, Item theItem, int thePos)
{
    thePos = this->GetInsertPos(t, theItem, thePos);
    size_t aPlace;
    NodeOf(theItem)->mX = ee_won_gui_tree_child(this->mParent == 0, t->TreeFlagSet(2 | 1) ? 1 : 0, this->mX, thePos,
                                                this->mChildren.size(), &aPlace);
    thePos = (int)aPlace;
    bool onEnd = thePos == (int)this->mChildren.size();
    this->mChildren.insert(this->mChildren.begin() + thePos, theItem);
    Own(Op::ItemHold, theItem);
    NodeOf(theItem)->mParent = this->mMe;
    for (int i = thePos; i < (int)this->mChildren.size(); i++) NodeOf(this->mChildren[i])->mIndex = i;
    uint8_t aRow = ee_won_gui_tree_insert_row(this->IsOpen(), onEnd);
    if (aRow == 0) return;
    int aPos;
    if (aRow == 1) aPos = this->GetNextNonChildPos(t);
    else aPos = (int)Base(LC::GetItemPos, this->mChildren[thePos + 1]);
    Base(LC::InsertItem, theItem, aPos);
}
void TreeItem::RemoveChild(TreeArea* t, Item theItem)
{
    Base(LC::BeginUpdate);
    TreeItem* aChild = NodeOf(theItem);
    uint8_t aPlan = ee_won_gui_tree_remove_plan(aChild->IsOpen(), this->IsOpen(), this->mChildren.size());
    if (aPlan & 1) aChild->SetOpen(t, false, false, false);
    if (aPlan & 2) Base(LC::RemoveItem, theItem);
    size_t i0 = (size_t)aChild->mIndex;
    if (i0 < this->mChildren.size()) {
        Item g = this->mChildren[i0];
        this->mChildren.erase(this->mChildren.begin() + i0);
        Own(Op::ItemRelease, g);
    }
    for (int i = 0; i < (int)this->mChildren.size(); i++) NodeOf(this->mChildren[i])->mIndex = i;
    if (aPlan & 4) HT(Op::Invalidate);
    Base(LC::EndUpdate);
}
void TreeItem::CloseAll()
{
    EeWonTreeOps anOps = TreeOps();
    ee_won_gui_tree_close_all(&anOps, (void*)this->mMe);
}
void TreeItem::InsertChildren(TreeArea*, int thePos)
{
    EeWonTreeOps anOps = TreeOps();
    ee_won_gui_tree_insert_children(&anOps, (void*)this->mMe, thePos);
}
void TreeItem::SetOpen(TreeArea* t, bool isOpen, bool closeAll, bool sendEvent)
{
    uint8_t anAction = ee_won_gui_tree_set_open_action(this->mOpen, isOpen, this->mChildren.empty());
    if (anAction == 0) return;
    this->mOpen = isOpen;
    if (anAction == 1) return;
    if (anAction == 2) {
        int aPos1 = (int)Base(LC::GetItemPos, this->mChildren.front());
        int aPos2 = this->GetNextNonChildPos(t);
        Base(LC::DeleteRange, aPos1, aPos2);
        if (closeAll) this->CloseAll();
    } else {
        Base(LC::BeginUpdate);
        this->InsertChildren(t, (int)Base(LC::GetItemPos, this->mMe) + 1);
        Base(LC::EndUpdate);
        Item anItem = this->GetNextNonChild();
        int aTotal = (int)HT(Op::Get, (intptr_t)Fd::TotalHeight);
        int aSibling = anItem != 0 ? (int)HT(Op::ItemTop, anItem) : 0;
        int aVert = (int)HT(Op::Get, (intptr_t)Fd::VertOffset);
        int aView = (int)HT(Op::Get, (intptr_t)Fd::ScrollHeight);
        int aTop = this->Top();
        int aVertOffset;
        if (ee_won_gui_tree_open_scroll(aTotal, anItem != 0, aSibling, aVert, aView, aTop, &aVertOffset))
            HT(Op::SetVertOffset, aVertOffset);
    }
    if (sendEvent) HT(Op::DeliverTreeOpened, this->mMe);
}
void TreeItem::Paint(TreeArea* aTreeArea)
{
    bool drawButton = aTreeArea->TreeFlagSet(2) && !this->mChildren.empty();
    int squareLeft = this->mX - 14;
    int squareRight = squareLeft + 8;
    int squareTop = ((int)HT(Op::ItemHeight, this->mMe) - 8) / 2;
    int squareBottom = squareTop + 8;
    int lineTop = squareTop;
    int lineBottom = squareBottom;
    int lineLeft = squareRight;
    if (!drawButton) {
        lineTop = lineBottom = squareTop + 4;
        lineLeft = squareLeft + 6;
    }
    if (aTreeArea->TreeFlagSet(1)) {
        DrawOp(EE_WON_DRAW_SET_SYS_COLOR, StandardColor_3DShadow, 0, 0, 0);
        if (this->Top() != 0) DrawOp(EE_WON_DRAW_DOTTED_LINE, squareLeft + 4, 0, lineTop + 1, 1);
        if (!this->IsLastChild()) DrawOp(EE_WON_DRAW_DOTTED_LINE, squareLeft + 4, lineBottom, (int)HT(Op::ItemHeight, this->mMe) - lineBottom, 1);
        DrawOp(EE_WON_DRAW_DOTTED_LINE, lineLeft, squareTop + 4, this->mX - lineLeft, 0);
        Item anItem = this->mParent;
        while (anItem != 0 && NodeOf(anItem)->mX > 0) {
            if (!NodeOf(anItem)->IsLastChild()) DrawOp(EE_WON_DRAW_DOTTED_LINE, NodeOf(anItem)->mX - 10, 0, (int)HT(Op::ItemHeight, this->mMe), 1);
            anItem = NodeOf(anItem)->mParent;
        }
    }
    if (drawButton) {
        DrawOp(EE_WON_DRAW_SET_SYS_COLOR, StandardColor_3DShadow, 0, 0, 0);
        DrawOp(EE_WON_DRAW_RECT, squareLeft, squareTop, 9, 9);
        DrawOp(EE_WON_DRAW_SET_SYS_COLOR, StandardColor_Text, 0, 0, 0);
        DrawOp(EE_WON_DRAW_LINE, squareLeft + 2, squareTop + 4, squareRight - 2, squareTop + 4);
        if (!this->mOpen) DrawOp(EE_WON_DRAW_LINE, squareLeft + 4, squareTop + 2, squareLeft + 4, squareBottom - 2);
    }
    DrawOp(EE_WON_DRAW_TRANSLATE, this->mX, 0, 0, 0);
    HT(Op::ItemSetSelected, this->mItem, HT(Op::ItemSelected, this->mMe));
    HT(Op::ItemPaint, this->mItem, kG);
    DrawOp(EE_WON_DRAW_TRANSLATE, -this->mX, 0, 0, 0);
}
void TreeItem::Sort(TreeArea* t, bool recursive)
{
    if (this->mChildren.empty()) return;
    std::vector<Item> v = this->mChildren;
    HT(Op::SortItems, (intptr_t)&v[0], v.size(), t->mTreeSortType);
    this->mChildren = v;
    for (size_t i = 0; i < v.size(); i++) {
        TreeItem* anItem = NodeOf(v[i]);
        anItem->mIndex = (int)i;
        if (recursive) anItem->Sort(t, recursive);
    }
}
} // namespace orig

struct OrigSide : Side {
    orig::TreeArea* mTree;
    std::map<intptr_t, orig::TreeItem*> mNodes;
    OrigSide() : mTree(new orig::TreeArea) {}
    void Gone() override { this->mTree->NotifyMouseItemGone(); }
    intptr_t NodePtr(intptr_t id) override { return (intptr_t)this->mNodes[id]; }
    void NewNode(intptr_t id) override { this->mNodes[id] = new orig::TreeItem(id); }
    void FreeNode(intptr_t id) override { orig::TreeItem* r = this->mNodes[id]; this->mNodes.erase(id); delete r; }
    intptr_t Node(NC c, intptr_t row, intptr_t a, intptr_t b, intptr_t cc) override
    {
        orig::TreeItem* r = this->mNodes[row];
        orig::TreeArea* t = this->mTree;
        if (r == NULL) return 0;
        int x = (int)a, y = (int)b;
        switch (c) {
        case NC::Paint: r->Paint(t); return 0;
        case NC::CalcDimensions: r->CalcDimensions(); return 0;
        case NC::SetViewport: r->SetViewport(a); return 0;
        case NC::Compare: return r->Compare(a);
        case NC::CanSelect: return r->CanSelect(x);
        case NC::SetPos: r->SetPos(y); return 0;
        case NC::MouseEnter: r->MouseEnter(t, x, y); return 0;
        case NC::MouseExit: r->MouseExit(t); return 0;
        case NC::MouseMove: r->MouseMove(t, x, y); return 0;
        case NC::MouseDrag: r->MouseDrag(t, x, y); return 0;
        case NC::MouseDown: r->MouseDown(t, x, y, (int)cc); return 0;
        case NC::MouseUp: r->MouseUp(t, x, y, (int)cc); return 0;
        case NC::GetChild: return r->GetChild(x);
        case NC::GetNextNonChild: return r->GetNextNonChild();
        case NC::GetNextNonChildPos: return r->GetNextNonChildPos(t);
        case NC::InsertChild: r->InsertChild(t, a, y); return 0;
        case NC::RemoveAllChildren: r->RemoveAllChildren(t); return 0;
        case NC::RemoveChild: r->RemoveChild(t, a); return 0;
        case NC::RemoveFromParent: r->RemoveFromParent(t); return 0;
        case NC::CloseAll: r->CloseAll(); return 0;
        case NC::InsertChildren: r->InsertChildren(t, x); return 0;
        case NC::SetOpen: r->SetOpen(t, a != 0, b != 0, cc != 0); return 0;
        case NC::ToggleOpen: r->ToggleOpen(t, a != 0, b != 0); return 0;
        case NC::IsLastChild: return r->IsLastChild();
        case NC::ContainsPoint: return r->ContainsPoint(x, y);
        case NC::Sort: r->Sort(t, a != 0); return 0;
        case NC::Item: return r->mItem;
        case NC::NumChildren: return (intptr_t)r->mChildren.size();
        case NC::Parent: return r->mParent;
        case NC::IsOpen: return r->mOpen;
        case NC::Indent: return r->mX;
        case NC::Index: return r->mIndex;
        }
        return 0;
    }
    intptr_t Tree(TC c, intptr_t a, intptr_t b, intptr_t cc) override
    {
        orig::TreeArea* t = this->mTree;
        switch (c) {
        case TC::Init: t->Init(); return 0;
        case TC::HandleEvent: t->HandleComponentEvent(a); return 0;
        case TC::NotifyMouseItemGone: t->NotifyMouseItemGone(); return 0;
        case TC::SetSortType: t->SetSortType((int)a); return 0;
        case TC::SortItem: t->Sort(a, b != 0); return 0;
        case TC::SetTreeFlags: t->SetTreeFlags((int)a, b != 0); return 0;
        case TC::TreeFlags: return t->mTreeFlags;
        case TC::InsertTreeItem: return t->InsertTreeItem(a, b, (int)cc);
        case TC::RemoveTreeItem: t->RemoveTreeItem(a); return 0;
        case TC::RemoveAllItems: t->RemoveAllItems(a); return 0;
        case TC::OpenItem: t->OpenItem(a, b != 0); return 0;
        case TC::CloseItem: t->CloseItem(a, b != 0, cc != 0); return 0;
        case TC::ToggleItem: t->ToggleItem(a, b != 0, cc != 0); return 0;
        case TC::Root: return t->mRoot;
        case TC::SortType: return t->mTreeSortType;
        case TC::MouseColItem: return t->mMouseColItem;
        }
        return 0;
    }
    void Free() { delete this->mTree; }
};

struct RustSide : Side {
    EeWonTree* mTree;
    std::map<intptr_t, EeWonTNode*> mNodes;
    RustSide() { this->mTree = ee_won_tree_new((void*)kTree, ScriptedHost, sList); }
    void Gone() override { ee_won_tree_call(this->mTree, TC::NotifyMouseItemGone, 0, 0, 0); }
    intptr_t NodePtr(intptr_t id) override { return (intptr_t)this->mNodes[id]; }
    void NewNode(intptr_t id) override { this->mNodes[id] = ee_won_tnode_new((void*)id, ScriptedHost); }
    void FreeNode(intptr_t id) override { EeWonTNode* r = this->mNodes[id]; this->mNodes.erase(id); ee_won_tnode_free(r); }
    intptr_t Node(NC c, intptr_t row, intptr_t a, intptr_t b, intptr_t cc) override
    {
        EeWonTNode* r = this->mNodes[row];
        if (r == NULL) return 0;
        if (c == NC::Paint) a = kG;
        return ee_won_tnode_call(r, this->mTree, c, a, b, cc);
    }
    intptr_t Tree(TC c, intptr_t a, intptr_t b, intptr_t cc) override { return ee_won_tree_call(this->mTree, c, a, b, cc); }
    void Free() { ee_won_tree_free(this->mTree); }
};

// ---- the driver

enum { kInsert, kRemove, kRemoveAll, kOpen, kClose, kToggle, kSortType, kSort, kFlags, kEvent, kEnter, kMove, kDown, kUp,
       kDrag, kExit, kPaint, kSize, kRefresh, kListFlags, kChild, kNext, kNextPos, kLast, kContains, kCloseAll, kQuery,
       kGone, kSel, kCalls };
static const char* kCallNames[kCalls] = {"insert", "remove", "remove-all", "open", "close", "toggle", "sort-type", "sort",
    "flags", "event", "enter", "move", "down", "up", "drag", "exit", "paint", "size", "refresh", "list-flags", "child",
    "next", "next-pos", "last", "contains", "close-all", "query", "gone", "sel"};
static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }

static intptr_t NewWrapped()
{
    HostItem h;
    h.top = (int)DR(50); h.height = 0; h.width = 0;
    h.selected = DR(5) == 0; h.selectable = DR(6) != 0; h.row = false;
    h.calcH = 8 + (int)DR(14);
    h.calcW = 10 + (int)DR(120);
    h.key = (int)DR(12);
    h.refs = 1; // the creator's
    intptr_t id = 0x1000 + (++sNextItem);
    sItems[id] = h;
    return id;
}
// a live row other than the root, or 0
static intptr_t SomeRow()
{
    intptr_t root = sSide->Tree(TC::Root);
    std::vector<intptr_t> in;
    for (std::map<intptr_t, HostItem>::const_iterator i = sItems.begin(); i != sItems.end(); ++i)
        if (i->second.row && i->second.refs > 0 && i->first != root) in.push_back(i->first);
    if (in.empty()) return 0;
    return in[DR((unsigned)in.size())];
}
// a shown row (one of the list's), or 0: the calls that change the tree take shown rows, as the room control
// does (a hidden row left open by its parent's close would put its children's rows in the list under a closed
// parent, and their parent's removal would leave them reading a freed row in the original)
static intptr_t ShownRow()
{
    int n = (int)Base(LC::NumItems);
    if (n == 0) return 0;
    return Base(LC::GetItem, (intptr_t)DR((unsigned)n));
}

static void Run()
{
    // CloseAll alone (no rows deleted) leaves rows of a closed subtree in the list, whose parent can then go: the
    // original then reads a freed row (undefined); only SetOpen's close calls it, after deleting the rows
    static const int kWeights[kCalls] = {14, 3, 1, 3, 2, 4, 2, 2, 2, 2, 2, 3, 3, 2, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r0 = (int)DR(aTotal), k = 0;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    long long a = 0, b = 0, c = 0;
    int w = sFields[(int)Fd::ScrollWidth] + 20;
    int h = sFields[(int)Fd::ScrollHeight] + 20;
    switch (k) {
    case kInsert: a = DR(3) == 0 ? 0 : ShownRow(); b = NewWrapped(); c = (int)DR(5) - 1; break;
    case kRemove: a = ShownRow(); break;
    case kRemoveAll: a = DR(3) == 0 ? 0 : ShownRow(); break;
    case kOpen: a = ShownRow(); b = DR(2); break;
    case kClose: case kToggle: a = ShownRow(); b = DR(2); c = DR(2); break;
    case kSortType: a = DR(3); break;
    case kSort: a = DR(3) == 0 ? 0 : ShownRow(); b = DR(2); break;
    case kFlags: { static const int kF[] = {1, 2, 4, 8}; a = kF[DR(4)]; b = DR(2); break; }
    case kEvent: {
        a = 0x6000 + (++sNextEvent);
        HostEvent e;
        e.type = DR(4) == 0 ? 3 : 15;
        e.controlId = DR(4) == 0 ? 7 : 100003;
        e.top = (int)DR(sFields[(int)Fd::TotalHeight] + 20) - 5;
        sEvents[(intptr_t)a] = e;
        break;
    }
    case kEnter: case kMove: case kDrag: a = (int)DR(w) - 10; b = (int)DR(h) - 10; break;
    case kDown: case kUp: a = (int)DR(w) - 10; b = (int)DR(h) - 10; c = DR(4) == 0 ? 1 : 0; break;
    case kRefresh: a = ShownRow(); break;
    case kListFlags: { static const int kF[] = {0x10, 2, 4, 0x200, 0x80}; a = kF[DR(5)]; b = DR(2); break; }
    case kChild: a = DR(4) == 0 ? sSide->Tree(TC::Root) : SomeRow(); b = (int)DR(5) - 1; break;
    case kNext: case kNextPos: case kLast: case kCloseAll: a = SomeRow(); break;
    case kSel: a = ShownRow(); break;
    case kContains: a = SomeRow(); b = (int)DR(80) - 10; c = (int)DR(h); break;
    case kQuery: a = SomeRow(); b = DR(6); break;
    default: break;
    }
    char aLine[160];
    snprintf(aLine, sizeof(aLine), "call %s %llx %llx %llx", kCallNames[k], a, b, c);
    sTrace.push_back(aLine);
    size_t aCallLine = sTrace.size() - 1;
    intptr_t r = 0;
    switch (k) {
    case kInsert: r = sSide->Tree(TC::InsertTreeItem, (intptr_t)a, (intptr_t)b, (intptr_t)c); sItems[(intptr_t)b].refs--; break;
    case kRemove: if (a) sSide->Tree(TC::RemoveTreeItem, (intptr_t)a); break;
    case kRemoveAll: sSide->Tree(TC::RemoveAllItems, (intptr_t)a); break;
    case kOpen: if (a) sSide->Tree(TC::OpenItem, (intptr_t)a, (intptr_t)b); break;
    case kClose: if (a) sSide->Tree(TC::CloseItem, (intptr_t)a, (intptr_t)b, (intptr_t)c); break;
    case kToggle: if (a) sSide->Tree(TC::ToggleItem, (intptr_t)a, (intptr_t)b, (intptr_t)c); break;
    case kSortType: sSide->Tree(TC::SetSortType, (intptr_t)a); r = sSide->Tree(TC::SortType); break;
    case kSort: sSide->Tree(TC::SortItem, (intptr_t)a, (intptr_t)b); break;
    case kFlags: sSide->Tree(TC::SetTreeFlags, (intptr_t)a, (intptr_t)b); r = sSide->Tree(TC::TreeFlags); break;
    case kEvent: sSide->Tree(TC::HandleEvent, (intptr_t)a); break;
    case kEnter: Base(LC::MouseEnter, (intptr_t)a, (intptr_t)b); r = sSide->Tree(TC::MouseColItem); break;
    case kMove: Base(LC::MouseMove, (intptr_t)a, (intptr_t)b); r = sSide->Tree(TC::MouseColItem); break;
    case kDown: Base(LC::MouseDown, (intptr_t)a, (intptr_t)b, (intptr_t)c); r = sSide->Tree(TC::MouseColItem); break;
    case kUp: Base(LC::MouseUp, (intptr_t)a, (intptr_t)b, (intptr_t)c); break;
    case kDrag: Base(LC::MouseDrag, (intptr_t)a, (intptr_t)b); break;
    case kExit: Base(LC::MouseExit); r = sSide->Tree(TC::MouseColItem); break;
    case kPaint: Base(LC::ScrollPaint, kG); break;
    case kSize: Base(LC::SizeChanged); break;
    case kRefresh: if (a) Base(LC::RefreshItem, (intptr_t)a); break;
    case kListFlags: Base(LC::SetListFlags, (intptr_t)a, (intptr_t)b); break;
    case kChild: if (a) r = sSide->Node(NC::GetChild, (intptr_t)a, (intptr_t)b); break;
    case kNext: if (a) r = sSide->Node(NC::GetNextNonChild, (intptr_t)a); break;
    case kNextPos: if (a) r = sSide->Node(NC::GetNextNonChildPos, (intptr_t)a); break;
    case kLast: if (a) r = sSide->Node(NC::IsLastChild, (intptr_t)a); break;
    case kContains: if (a) r = sSide->Node(NC::ContainsPoint, (intptr_t)a, (intptr_t)b, (intptr_t)c); break;
    case kCloseAll: if (a) sSide->Node(NC::CloseAll, (intptr_t)a); break;
    case kQuery: {
        static const NC kQ[] = {NC::Item, NC::NumChildren, NC::Parent, NC::IsOpen, NC::Indent, NC::Index};
        if (a) r = sSide->Node(kQ[b], (intptr_t)a);
        break;
    }
    case kGone: Base(LC::ClearMouseItem); sSide->Tree(TC::NotifyMouseItemGone); break;
    case kSel: if (a) r = Base(LC::SetSelItem, (intptr_t)a, 0, 1); break;
    }
    sTrace[aCallLine] += " -> " + std::to_string((long long)r);
}

static Trace RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sItems.clear();
    sEvents.clear();
    sNextItem = sNextEvent = 0;
    sSeed = inSeq * 2654435761u + 37u;
    sDriverSeed = inSeq * 40503u + 17u;
    memset(sFields, 0, sizeof(sFields));
    sFields[(int)Fd::ScrollWidth] = 40 + (int)R(260);
    sFields[(int)Fd::ScrollHeight] = 20 + (int)R(180);
    sFields[(int)Fd::Width] = sFields[(int)Fd::ScrollWidth] + 4;
    sFields[(int)Fd::Height] = sFields[(int)Fd::ScrollHeight] + 4;
    sFields[(int)Fd::UpScrollAmount] = sFields[(int)Fd::DownScrollAmount] = 10;
    TraceFields();
    sList = ee_won_list_new((void*)kTree, ScriptedHost);
    OrigSide* o = NULL;
    RustSide* rs = NULL;
    if (inRust) sSide = rs = new RustSide;
    else sSide = o = new OrigSide;
    sTrace.push_back("call new");
    sSide->Tree(TC::Init);
    if (DR(2)) { // the config's sort type
        char aLine[64];
        unsigned s = DR(2);
        snprintf(aLine, sizeof(aLine), "call init-sort %x 0 0 -> 0", s);
        sTrace.push_back(aLine);
        sSide->Tree(TC::SetSortType, s);
    }
    for (int i = 0; i < inCalls; i++) Run();
    sTrace.push_back("free");
    if (inRust) rs->Free(); else o->Free();
    ee_won_list_free(sList);
    delete sSide;
    sSide = NULL;
    sList = NULL;
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
    int aFirst = getenv("WON_FIRST_SEQ") ? atoi(getenv("WON_FIRST_SEQ")) : 0;
    for (int seq = aFirst; seq < kSequences; seq++) {
        sOpsMet.clear();
        if (getenv("WON_TRACE_SEQ")) { fprintf(stderr, "seq %d\n", seq); fflush(stderr); }
        Trace a = RunSide(false, (unsigned)seq, kCalls);
        Trace b = RunSide(true, (unsigned)seq, kCalls);
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
    if (!aPrint) {
        printf("ops met:");
        for (std::set<std::string>::const_iterator i = aSeen.begin(); i != aSeen.end(); ++i)
            if (i->compare(0, 2, "op") == 0) printf(" %s", i->c_str() + 2);
        printf("\n");
        printf("won_treeobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    }
    return sFailures;
}
