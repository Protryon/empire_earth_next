// The multi-column list as whole objects (R6 WON round 24): the original MultiListArea.cpp, MultiListItem.cpp,
// MultiListHeader.cpp and MultiListArea::NotifyMouseItemGone (ListControlAdapters.cpp), the statements copied from the
// C++ before the port with every call on a C++ object (the ListArea base, the cells' fields and virtuals, the column
// viewports, the header's buttons, the Container base, the cursor, the events, the Graphics) replaced by the op that
// makes it (GuiHost.cpp runs the same calls in the same order), against ee_won's Rust MultiListArea, MultiListItem and
// MultiListHeader (multiobj.rs) over one scripted host. Both sides' ListArea base is the Rust list (listobj.rs), as in
// the game: its calls back (AdjustPosition, Sort, NotifyMouseItemGone, the rows' virtuals) go to the side's objects.
// The host keeps the list's scroll area, the rows' and cells' fields (tops, sizes, selection, sort keys, references),
// the viewports' and buttons' references and places, the cursors and the events; the sort compares the rows through
// the side's row Compare.
//
// Each of 20,000 sequences creates the list (1-4 columns) and its header and makes 40 random inward calls (rows
// inserted, cells set and appended, updates with auto grow, column counts, widths, size modes, alignments, sort
// columns and functions, the dynamic column, size changes, paints, the list's mouse over the rows and cells, deletes
// and clears, the header's buttons, layout, mouse drags and presses, events). The traces must be the same, and every
// reference taken must be given back when the objects go. The first 40 sequences and every later one that makes an
// op or a call none before made are rust/crates/ee_won/src/golden_multiobj.txt, which the Rust tests replay;
// `--print` prints them.
#include <stdio.h>
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
typedef EeWonMultiCall MC;
typedef EeWonMRowCall RC;
typedef EeWonMHeadCall HC;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

static const intptr_t kList = 0x55, kHead = 0x66, kG = 0x6;
static const intptr_t kFuncAsc = 0x5001, kFuncDesc = 0x5002;

struct HostItem {
    int top, height, width;
    bool selected, selectable, row;
    int calcH, calcW, key, refs;
};
struct HostComp { int left, width, controlId, desired, refs; bool visible, enabled; };
static std::map<intptr_t, HostItem> sItems;
static std::map<intptr_t, HostComp> sComps;
static std::map<intptr_t, intptr_t> sEvents; // event -> its child under the header (0: the climb ends at NULL)
static std::map<intptr_t, int> sCursor;
static intptr_t sNextItem, sNextComp, sNextEvent;
static int sFields[16];
static int sHeadW, sHeadH;
static std::vector<std::string> sTrace;
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
    snprintf(b, sizeof(b), " %d %d", sHeadW, sHeadH);
    s += b;
    sTrace.push_back(s);
}

// ---- the side: the original objects or the Rust ones
struct Side {
    virtual ~Side() {}
    virtual void Adjust(bool b) = 0;   // MultiListArea::AdjustPosition (the list's virtual)
    virtual void Gone() = 0;           // MultiListArea::NotifyMouseItemGone
    virtual intptr_t MultiPtr() = 0;   // ((MultiListArea*)list)->mMulti
    virtual intptr_t Row(RC c, intptr_t row, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) = 0;
    virtual void NewRow(intptr_t id) = 0;
    virtual void FreeRow(intptr_t id) = 0;
    virtual intptr_t Multi(MC c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) = 0;
    virtual intptr_t Head(HC c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) = 0;
};
static Side* sSide;
static EeWonList* sList;

static int RowCompare(intptr_t a, intptr_t b) { return (int)sSide->Row(RC::Compare, a, b); }
static bool LineLess(intptr_t a, intptr_t b, int type)
{
    int r = RowCompare(a, b);
    return type == 0 ? r < 0 : r > 0;
}

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    intptr_t comp = (intptr_t)inComp;
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    int n = (int)inOp;
    switch (inOp) {
    case Op::Get: // pure reads: not traced
        if (comp == kHead) return a == (intptr_t)Fd::Width ? sHeadW : a == (intptr_t)Fd::Height ? sHeadH : 0;
        return sFields[a];
    case Op::Set: sFields[a] = (int)b; break;
    case Op::BaseSizeChanged:
        if (comp == kHead) {
            sHeadW = 50 + (int)R(300);
            sHeadH = 10 + (int)R(20);
            Line("%lld head-base-size", n, 0, 0, 0, 0);
            TraceFields();
            return 0;
        }
        Line("%lld base-size", n, 0, 0, 0, 0);
        sFields[(int)Fd::ScrollWidth] = 20 + (int)R(280);
        sFields[(int)Fd::ScrollHeight] = 10 + (int)R(200);
        sFields[(int)Fd::Width] = sFields[(int)Fd::ScrollWidth] + 4;
        sFields[(int)Fd::Height] = sFields[(int)Fd::ScrollHeight] + 4;
        TraceFields();
        sSide->Adjust(false); // ScrollArea::Virtual08 calls the virtual AdjustPosition
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
    case Op::VirtAdjustPosition: Line("%lld virt-adjust %lld", n, (long long)a, 0, 0, 0); sSide->Adjust(a != 0); return 0;
    case Op::VirtSort: {
        Line("%lld virt-sort", n, 0, 0, 0, 0);
        intptr_t s = ee_won_list_call(sList, LC::Sort, 0, 0, 0); // MultiListArea keeps ListArea::Sort
        Line("%lld virt-sort -> %lld", n, s, 0, 0, 0);
        return s;
    }
    case Op::VirtNotifyMouseItemGone: Line("%lld virt-gone", n, 0, 0, 0, 0); sSide->Gone(); return 0;
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
    case Op::ItemTop: r = sItems[a].top; break;
    case Op::ItemSetTop: sItems[a].top = (int)b; break;
    case Op::ItemHeight: r = sItems[a].height; break;
    case Op::ItemSetHeight: sItems[a].height = (int)b; break;
    case Op::ItemWidth: r = sItems[a].width; break;
    case Op::ItemSetWidth: sItems[a].width = (int)b; break;
    case Op::ItemSelected: r = sItems[a].selected; break;
    case Op::ItemSetSelected: sItems[a].selected = b != 0; break;
    case Op::ItemSelectable: r = sItems[a].selectable; break;
    case Op::ItemSetPos:
        if (sItems[a].row) { Line("%lld row-set-pos %llx %lld", n, (long long)a, (long long)b, 0, 0); sSide->Row(RC::SetPos, a, 0, b); return 0; }
        sItems[a].top = (int)b;
        break;
    case Op::ItemContains:
        if (sItems[a].row) r = c >= sItems[a].top && c < sItems[a].top + sItems[a].height; // MultiListItem::Contains
        else r = b >= 0 && b < sItems[a].width && c >= sItems[a].top && c < sItems[a].top + sItems[a].height;
        break;
    case Op::ItemCanSelect:
        if (sItems[a].row) {
            Line("%lld row-can-select %llx %lld %lld", n, (long long)a, (long long)b, (long long)c, 0);
            r = sSide->Row(RC::CanSelect, a, b, c);
            Line("%lld row-can-select -> %lld", n, (long long)r, 0, 0, 0);
            return r;
        }
        r = sItems[a].selectable && (b & 15) != 3;
        break;
    case Op::ItemCalcDimensions:
        if (sItems[a].row) { Line("%lld row-calc %llx", n, (long long)a, 0, 0, 0); sSide->Row(RC::CalcDimensions, a); return 0; }
        sItems[a].height = sItems[a].calcH; sItems[a].width = sItems[a].calcW;
        break;
    case Op::ItemSetViewport:
        if (sItems[a].row) { Line("%lld row-viewport %llx %llx", n, (long long)a, (long long)b, 0, 0); sSide->Row(RC::SetViewport, a, b); return 0; }
        break;
    case Op::ItemPaint:
        if (sItems[a].row) { Line("%lld row-paint %llx", n, (long long)a, 0, 0, 0); sSide->Row(RC::Paint, a, b); return 0; }
        break;
    case Op::ItemMouseEnter:
        if (sItems[a].row) { Line("%lld row-enter %llx %lld %lld", n, (long long)a, (long long)b, (long long)c, 0); sSide->Row(RC::MouseEnter, a, b, c); return 0; }
        break;
    case Op::ItemMouseExit:
        if (sItems[a].row) { Line("%lld row-exit %llx", n, (long long)a, 0, 0, 0); sSide->Row(RC::MouseExit, a); return 0; }
        break;
    case Op::ItemMouseMove:
        if (sItems[a].row) { Line("%lld row-move %llx %lld %lld", n, (long long)a, (long long)b, (long long)c, 0); sSide->Row(RC::MouseMove, a, b, c); return 0; }
        break;
    case Op::ItemMouseDrag:
        if (sItems[a].row) { Line("%lld row-drag %llx %lld %lld", n, (long long)a, (long long)b, (long long)c, 0); sSide->Row(RC::MouseDrag, a, b, c); return 0; }
        break;
    case Op::ItemMouseDown:
        if (sItems[a].row) { Line("%lld row-down %llx %lld %lld %lld", n, (long long)a, (long long)b, (long long)c, (long long)d); sSide->Row(RC::MouseDown, a, b, c, d); return 0; }
        break;
    case Op::ItemMouseUp:
        if (sItems[a].row) { Line("%lld row-up %llx %lld %lld %lld", n, (long long)a, (long long)b, (long long)c, (long long)d); sSide->Row(RC::MouseUp, a, b, c, d); return 0; }
        break;
    case Op::ItemHold: sItems[a].refs++; break;
    case Op::ItemRelease:
        sItems[a].refs--;
        Line("%lld release %llx -> %lld", n, (long long)a, sItems[a].refs, 0, 0);
        if (sItems[a].row && sItems[a].refs == 0) sSide->FreeRow(a); // the row goes: its cells' references back
        return 0;
    case Op::ItemCompare: { int ka = sItems[a].key, kb = sItems[b].key; r = ka < kb ? -1 : ka > kb ? 1 : 0; break; }
    case Op::CallSortFunc: { int ka = sItems[b].key, kb = sItems[c].key; r = a == kFuncAsc ? ka - kb : kb - ka; break; }
    case Op::SetCursor: sCursor[comp] = (int)a; break;
    case Op::CursorIs: r = sCursor[comp] == (int)a; break;
    case Op::SelBackground: r = 0xabcdef; break;
    case Op::NewColumnViewport: {
        r = 0x4000 + (++sNextComp);
        HostComp h = {0, 0, 0, 0, 1, true, true};
        sComps[r] = h;
        break;
    }
    case Op::ChildRelease: sComps[a].refs--; break;
    case Op::ChildSetPos: sComps[a].left = (int)b; break;
    case Op::HeaderNotify: Line("%lld header-notify %llx", n, (long long)a, 0, 0, 0); sSide->Head(HC::NotifyColumnChange); return 0;
    case Op::HeaderSetSortColumn:
        Line("%lld header-sort %llx %lld %lld", n, (long long)a, (long long)b, (long long)c, 0);
        sSide->Head(HC::SetSortColumn, b, c);
        return 0;
    case Op::RowCell: r = sSide->Row(RC::GetItem, a, b); break;
    case Op::RowSetCell: Line("%lld row-set-cell %llx %llx %lld", n, (long long)a, (long long)b, (long long)c, 0); sSide->Row(RC::SetItem, a, b, c); return 0;
    case Op::MultiOf: return sSide->MultiPtr(); // not traced (a pointer)
    case Op::ChildSetEnabled: sComps[a].enabled = b != 0; break;
    case Op::ChildSetVisible: sComps[a].visible = b != 0; break;
    case Op::ChildSetPosSize: {
        const EeWonRect4* q = (const EeWonRect4*)b;
        sComps[a].left = q->x;
        sComps[a].width = q->w;
        Line("%lld pos-size %llx %lld %lld %lld %lld", n, (long long)a, q->x, q->y, q->w, q->h);
        return 0;
    }
    case Op::ChildLeft: r = sComps[a].left; break;
    case Op::ChildRight: r = sComps[a].left + sComps[a].width; break;
    case Op::ChildWidth: r = sComps[a].width; break;
    case Op::ChildControlId: r = sComps[a].controlId; break;
    case Op::ChildSetControlId: sComps[a].controlId = (int)b; break;
    case Op::ChildDesiredSize: {
        int* wh = (int*)b;
        wh[0] = sComps[a].desired;
        wh[1] = 20;
        Line("%lld desired %llx -> %lld", n, (long long)a, wh[0], 0, 0);
        return 0;
    }
    case Op::EventIsButtonPress: r = (a & 1) != 0; break;
    case Op::EventChildUnder: r = sEvents[a]; break;
    default: break;
    }
    Line("%lld %llx %llx %llx %llx -> %llx", n, (long long)a, (long long)b, (long long)c, (long long)d, (long long)r);
    return r;
}

static intptr_t HL(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    return ScriptedHost((void*)kList, inOp, a, b, c, d);
}
static intptr_t HH(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    return ScriptedHost((void*)kHead, inOp, a, b, c, d);
}
static intptr_t Base(LC c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) { return ee_won_list_call(sList, c, a, b, cc); }

// ---- the original (the three units; the ListArea base the Rust list, every other call through the host)

namespace orig {
enum { ListSortType_None = 2 };
typedef intptr_t Item;

struct MultiListHeader;
struct MultiListArea {
    struct ColumnInfo {
        intptr_t mContainer;
        int mx, mWidth;
        intptr_t mSortFunc;
        unsigned char mSizeMode;
        int mPartnerColumn;
        unsigned char mHAlign;
        ColumnInfo() : mContainer(0), mx(0), mWidth(0), mSortFunc(0), mSizeMode(1), mPartnerColumn(-1), mHAlign(0) {}
    };
    std::vector<ColumnInfo> mColumnInfoVector;
    intptr_t mSortColFunc;
    int mColumnPaintContext, mTotalColumnWidth, mSortColumn, mSortColumnPrev, mColumnBorderWidth, mDynamicSizeColumn;
    bool mAutoGrowColumns;
    Item mMouseColItem;
    int mMouseColNum, mMaxTotalColumnWidth;
    intptr_t mListHeader;

    MultiListArea()
    {
        this->mTotalColumnWidth = 0; this->mSortColumn = 0; this->mSortColumnPrev = 0; this->mListHeader = 0;
        this->mColumnBorderWidth = 5; this->mDynamicSizeColumn = -1; this->mMaxTotalColumnWidth = 0;
        this->mMouseColItem = 0; this->mSortColFunc = 0; this->mAutoGrowColumns = false; this->mColumnPaintContext = 0;
        this->mMouseColNum = 0;
    }
    ~MultiListArea()
    {
        for (size_t i = 0; i < this->mColumnInfoVector.size(); i++)
            if (this->mColumnInfoVector[i].mContainer) HL(Op::ChildRelease, this->mColumnInfoVector[i].mContainer);
    }
    int GetColumnAt(int x)
    {
        std::vector<int> aXs, aWidths;
        for (size_t i = 0; i < this->mColumnInfoVector.size(); ++i) {
            aXs.push_back(this->mColumnInfoVector[i].mx);
            aWidths.push_back(this->mColumnInfoVector[i].mWidth);
        }
        return ee_won_gui_ml_column_at(aXs.empty() ? NULL : &aXs[0], aWidths.empty() ? NULL : &aWidths[0], aXs.size(), x);
    }
    int GetColumnWidth(int column)
    {
        if (column < 0 || column >= (int)this->mColumnInfoVector.size()) return 0;
        return this->mColumnInfoVector[column].mWidth;
    }
    int GetColumnPos(int column)
    {
        if (column < 0 || column >= (int)this->mColumnInfoVector.size()) return -1;
        return this->mColumnInfoVector[column].mx;
    }
    void NotifyListHeader() { if (this->mListHeader != 0) HL(Op::HeaderNotify, this->mListHeader); }
    void SetColumnWidth(int theCol, int theWidth, bool fromSizeChanged)
    {
        size_t aCount = this->mColumnInfoVector.size();
        std::vector<int> aWidths(aCount), aPartners(aCount), aXs(aCount);
        std::vector<unsigned char> aModes(aCount);
        for (size_t i = 0; i < aCount; i++) {
            aWidths[i] = this->mColumnInfoVector[i].mWidth;
            aPartners[i] = this->mColumnInfoVector[i].mPartnerColumn;
            aModes[i] = this->mColumnInfoVector[i].mSizeMode;
        }
        if (!ee_won_gui_ml_set_width(aCount ? &aWidths[0] : NULL, aCount ? &aModes[0] : NULL, aCount ? &aPartners[0] : NULL,
                                     aCount ? &aXs[0] : NULL, aCount, &this->mTotalColumnWidth, this->mMaxTotalColumnWidth,
                                     theCol, theWidth, fromSizeChanged))
            return;
        for (size_t i = 0; i < aCount; i++) {
            this->mColumnInfoVector[i].mWidth = aWidths[i];
            intptr_t aContainer = this->mColumnInfoVector[i].mContainer;
            if (aContainer != 0) HL(Op::ChildSetPos, aContainer, aXs[i], 0);
            this->mColumnInfoVector[i].mx = aXs[i];
        }
        this->NotifyListHeader();
        HL(Op::VirtAdjustPosition, 0);
    }
    void SetSortColumn(int theCol, int theType)
    {
        if (theCol < 0 || theCol >= (int)this->mColumnInfoVector.size()) return;
        this->mSortColumnPrev = this->mSortColumn;
        this->mSortColumn = theCol;
        Base(LC::SetSortTypeOnly, theType);
        this->mSortColFunc = this->mColumnInfoVector[theCol].mSortFunc;
        if (this->mListHeader != 0) HL(Op::HeaderSetSortColumn, this->mListHeader, theCol, theType);
        HL(Op::VirtSort);
    }
    void SetSortColumnFunc(int theCol, intptr_t theFunc)
    {
        if (theCol < 0 || theCol >= (int)this->mColumnInfoVector.size()) return;
        this->mColumnInfoVector[theCol].mSortFunc = theFunc;
        if (this->mSortColumn == theCol) this->mSortColFunc = theFunc;
    }
    void AdjustPosition(bool becauseOfScrollbar)
    {
        HL(Op::Set, (intptr_t)Fd::TotalWidth, this->mTotalColumnWidth);
        Base(LC::AdjustPosition, becauseOfScrollbar);
        this->NotifyListHeader();
    }
    void Virtual08()
    {
        int oldWidth = (int)HL(Op::Get, (intptr_t)Fd::ScrollWidth);
        Base(LC::SizeChanged);
        int aSizeDelta = ee_won_gui_ml_dynamic_delta(this->mDynamicSizeColumn, this->mColumnInfoVector.size(),
                                                     (int)HL(Op::Get, (intptr_t)Fd::ScrollWidth), oldWidth, this->mTotalColumnWidth);
        if (aSizeDelta != 0)
            this->SetColumnWidth(this->mDynamicSizeColumn, this->mColumnInfoVector[this->mDynamicSizeColumn].mWidth + aSizeDelta, true);
    }
    void ScrollPaint()
    {
        std::vector<int> aWidths;
        for (size_t i = 0; i < this->mColumnInfoVector.size(); i++) aWidths.push_back(this->mColumnInfoVector[i].mWidth);
        EeWonDrawOp ops[400];
        size_t n = ee_won_gui_draw_multi_list(aWidths.empty() ? NULL : &aWidths[0], aWidths.size(), this->mColumnBorderWidth,
                                              this->mTotalColumnWidth, (int)HL(Op::Get, (intptr_t)Fd::VertOffset),
                                              (int)HL(Op::Get, (intptr_t)Fd::ScrollHeight), ops, 400);
        for (size_t i = 0; i < n && i < 400; i++) {
            const EeWonDrawOp& o = ops[i];
            if (o.kind == EE_WON_DRAW_CALL) {
                if (o.a == 0) {
                    this->mColumnPaintContext = o.b;
                    Base(LC::SetPaintWidth, o.c);
                } else Base(LC::ScrollPaint, kG);
            } else HL(Op::Draw, kG, (intptr_t)&o);
        }
    }
    Item UpdateItem(Item theItem)
    {
        Base(LC::RefreshItem, theItem);
        if (this->mAutoGrowColumns) {
            for (int i = 0; i < (int)this->mColumnInfoVector.size(); i++) {
                Item anItem = HL(Op::RowCell, theItem, i);
                if (anItem == 0) continue;
                int aWidth = ee_won_gui_ml_grow((int)HL(Op::ItemWidth, anItem), this->mColumnBorderWidth, this->mColumnInfoVector[i].mWidth);
                if (aWidth >= 0) this->SetColumnWidth(i, aWidth, false);
            }
        }
        return theItem;
    }
    Item SetCell(int theRow, int theCol, Item theCellItem)
    {
        if (theCol >= (int)this->mColumnInfoVector.size()) return 0;
        Item aRow = Base(LC::GetItem, theRow);
        if (aRow == 0) return 0;
        HL(Op::RowSetCell, aRow, theCellItem, theCol);
        this->UpdateItem(aRow);
        return aRow;
    }
    void SetNumColumns(int numColumns)
    {
        if ((size_t)numColumns < this->mColumnInfoVector.size()) {
            for (size_t i = numColumns; i < this->mColumnInfoVector.size(); i++)
                if (this->mColumnInfoVector[i].mContainer) HL(Op::ChildRelease, this->mColumnInfoVector[i].mContainer);
        }
        this->mColumnInfoVector.resize(numColumns);
        this->mTotalColumnWidth = 0;
        for (int i = 0; i < (int)this->mColumnInfoVector.size(); ++i) {
            intptr_t container = this->mColumnInfoVector[i].mContainer;
            if (container == 0) {
                container = HL(Op::NewColumnViewport);
                this->mColumnInfoVector[i].mContainer = container;
            }
            this->mTotalColumnWidth += this->mColumnInfoVector[i].mWidth;
        }
        this->NotifyListHeader();
        HL(Op::VirtAdjustPosition, 0);
    }
    void SetColumnHAlign(int column, int align)
    {
        if (column < 0 || column >= (int)this->mColumnInfoVector.size()) return;
        this->mColumnInfoVector[column].mHAlign = static_cast<unsigned char>(align);
    }
    void SetColumnSizeMode(int theCol, unsigned char theMode)
    {
        if (theCol < 0 || theCol >= (int)this->mColumnInfoVector.size()) return;
        int anOldMode = this->mColumnInfoVector[theCol].mSizeMode;
        this->mColumnInfoVector[theCol].mSizeMode = theMode;
        if (!ee_won_gui_ml_relink((unsigned char)anOldMode, theMode)) return;
        size_t aCount = this->mColumnInfoVector.size();
        std::vector<unsigned char> aModes(aCount);
        std::vector<int> aPartners(aCount);
        for (size_t i = 0; i < aCount; i++) {
            aModes[i] = this->mColumnInfoVector[i].mSizeMode;
            aPartners[i] = this->mColumnInfoVector[i].mPartnerColumn;
        }
        ee_won_gui_ml_link_partners(aCount ? &aModes[0] : NULL, aCount ? &aPartners[0] : NULL, aCount);
        for (size_t i = 0; i < aCount; i++) this->mColumnInfoVector[i].mPartnerColumn = aPartners[i];
    }
    void NotifyMouseItemGone()
    {
        Base(LC::NotifyMouseItemGone);
        this->mMouseColItem = 0;
    }
};

struct MultiListItem {
    Item mMe;
    std::vector<Item> mItems;
    explicit MultiListItem(Item me) : mMe(me) {}
    ~MultiListItem()
    {
        for (size_t i = 0; i < this->mItems.size(); i++)
            if (this->mItems[i]) ScriptedHost((void*)this->mMe, Op::ItemRelease, this->mItems[i], 0, 0, 0);
    }
    Item GetItem(int column) const
    {
        if (column < 0 || column >= (int)this->mItems.size()) return 0;
        return this->mItems[column];
    }
    void SetItem(Item inItem, int inColumn)
    {
        if (inColumn < 0) return; // undefined in the original (the driver never asks)
        if ((int)this->mItems.size() <= inColumn) this->mItems.resize(inColumn + 1);
        Item old = this->mItems[inColumn];
        if (old != inItem) {
            if (old != 0) ScriptedHost((void*)this->mMe, Op::ItemRelease, old, 0, 0, 0);
            this->mItems[inColumn] = inItem;
            if (inItem != 0) ScriptedHost((void*)this->mMe, Op::ItemHold, inItem, 0, 0, 0);
        }
    }
    void InsertItem(Item theItem)
    {
        this->mItems.push_back(theItem);
        if (theItem != 0) ScriptedHost((void*)this->mMe, Op::ItemHold, theItem, 0, 0, 0);
    }
    int Top() { return (int)HL(Op::ItemTop, this->mMe); }
    void CalcDimensions(MultiListArea* anArea)
    {
        HL(Op::ItemSetHeight, this->mMe, 0);
        std::vector<Item> items = this->mItems;
        for (size_t i = 0; i < items.size(); i++) {
            Item anItem = items[i];
            if (anItem == 0) continue;
            HL(Op::ItemCalcDimensions, anItem);
            int aHeight = (int)HL(Op::ItemHeight, anItem);
            if (aHeight > (int)HL(Op::ItemHeight, this->mMe)) HL(Op::ItemSetHeight, this->mMe, aHeight);
        }
        HL(Op::ItemSetWidth, this->mMe, anArea->mTotalColumnWidth);
    }
    int Compare(Item inOther, MultiListArea* anArea) const
    {
        int aColumn = anArea->mSortColumn;
        Item anItem = this->GetItem(aColumn);
        Item anOther = HL(Op::RowCell, inOther, aColumn);
        if (anItem == anOther) return 0;
        if (anItem == 0) return -1;
        if (anOther == 0) return 1;
        if (anArea->mSortColFunc != 0) return (int)HL(Op::CallSortFunc, anArea->mSortColFunc, anItem, anOther);
        return (int)HL(Op::ItemCompare, anItem, anOther);
    }
    void SetPos(int inY)
    {
        HL(Op::ItemSetTop, this->mMe, inY);
        std::vector<Item> items = this->mItems;
        for (size_t i = 0; i < items.size(); i++)
            if (items[i] != 0) HL(Op::ItemSetPos, items[i], inY);
    }
    Item GetMouseItem(MultiListArea* anArea, int& ioX)
    {
        int aColumn = anArea->GetColumnAt(ioX);
        ioX -= anArea->GetColumnPos(aColumn);
        return this->GetItem(aColumn);
    }
    Item GetMouseItem(MultiListArea* anArea, int& ioX, int& ioY)
    {
        int aColumn = anArea->GetColumnAt(ioX);
        ioX -= anArea->GetColumnPos(aColumn);
        Item anItem = this->GetItem(aColumn);
        Item anOld = anArea->mMouseColItem;
        if (anOld != 0) {
            if (anOld != anItem || !HL(Op::ItemContains, anOld, ioX, ioY + this->Top())) {
                HL(Op::ItemMouseExit, anOld);
                HL(Op::SetCursor, (intptr_t)EeWonGuiCursor::None);
                anArea->mMouseColItem = 0;
            }
        } else if (anItem != 0 && HL(Op::ItemContains, anItem, ioX, ioY + this->Top())) {
            anArea->mMouseColItem = anItem;
            anArea->mMouseColNum = aColumn;
            HL(Op::ItemMouseEnter, anItem, ioX, ioY);
        }
        return anArea->mMouseColItem;
    }
    void Paint(MultiListArea* anArea)
    {
        int aColumn = anArea->mColumnPaintContext;
        Item anItem = static_cast<unsigned int>(aColumn) < this->mItems.size() ? this->mItems[aColumn] : 0;
        EeWonMultiRowPaint r;
        r.column = aColumn;
        r.selected = HL(Op::ItemSelected, this->mMe) != 0;
        r.full_row_select = (Base(LC::ListFlags) & 0x10) != 0;
        r.has_cell = anItem != 0;
        r.cell_count = (uint32_t)this->mItems.size();
        r.selection_color = (uint32_t)HL(Op::SelBackground, kG);
        r.total_width = anArea->mTotalColumnWidth;
        r.height = (int)HL(Op::ItemHeight, this->mMe);
        // (a cell past the columns: the original read past the vector)
        r.align = anItem != 0 && aColumn < (int)anArea->mColumnInfoVector.size() ? anArea->mColumnInfoVector[aColumn].mHAlign : 0;
        r.paint_width = (int)Base(LC::PaintWidth);
        r.cell_width = anItem != 0 ? (int)HL(Op::ItemWidth, anItem) : 0;
        EeWonDrawOp ops[64];
        size_t n = ee_won_gui_draw_multi_row(&r, ops, 64);
        for (size_t i = 0; i < n && i < 64; i++) {
            const EeWonDrawOp& o = ops[i];
            if (o.kind == EE_WON_DRAW_CALL) {
                if (o.a == 0) {
                    HL(Op::ItemSetHeight, anItem, o.b);
                    HL(Op::ItemSetSelected, anItem, o.c != 0);
                } else HL(Op::ItemPaint, anItem, kG);
            } else HL(Op::Draw, kG, (intptr_t)&o);
        }
    }
    void SetViewport(MultiListArea* anArea, intptr_t inViewport)
    {
        std::vector<Item> items = this->mItems;
        unsigned int aColumn = 0;
        for (size_t k = 0; k < items.size(); aColumn++, k++) {
            std::vector<MultiListArea::ColumnInfo>& aVec = anArea->mColumnInfoVector;
            if (items[k] != 0 && aColumn < aVec.size()) {
                intptr_t aViewport = inViewport == 0 ? 0 : aVec[aColumn].mContainer;
                HL(Op::ItemSetViewport, items[k], aViewport);
            }
        }
    }
    bool CanSelect(MultiListArea* anArea, int inX, int inY)
    {
        if (!HL(Op::ItemSelectable, this->mMe)) return false;
        Item anItem = this->GetMouseItem(anArea, inX);
        if (anItem != 0) return HL(Op::ItemCanSelect, anItem, inX, inY) != 0;
        return true;
    }
    void MouseEnter(MultiListArea* a, int x, int y) { this->GetMouseItem(a, x, y); }
    void MouseExit(MultiListArea* anArea)
    {
        if (anArea->mMouseColItem != 0) {
            HL(Op::ItemMouseExit, anArea->mMouseColItem);
            Base(LC::ClearMouseItem);
        }
    }
    void MouseMove(MultiListArea* a, int x, int y)
    {
        Item anItem = this->GetMouseItem(a, x, y);
        if (anItem != 0) HL(Op::ItemMouseMove, anItem, x, y);
    }
    void MouseDrag(MultiListArea* anArea, int x, int y)
    {
        Item anItem = anArea->mMouseColItem;
        if (anItem != 0) HL(Op::ItemMouseDrag, anItem, x - anArea->GetColumnPos(anArea->mMouseColNum), y);
    }
    void MouseDown(MultiListArea* a, int x, int y, int b)
    {
        Item anItem = this->GetMouseItem(a, x, y);
        if (anItem != 0) HL(Op::ItemMouseDown, anItem, x, y, b);
    }
    void MouseUp(MultiListArea* a, int x, int y, int b)
    {
        Item anItem = this->GetMouseItem(a, x, y);
        if (anItem != 0) HL(Op::ItemMouseUp, anItem, x, y, b);
    }
};

struct MultiListHeader {
    struct ColumnInfo { intptr_t mComponent; int mSortType; ColumnInfo(intptr_t c = 0) : mComponent(c), mSortType(ListSortType_None) {} };
    std::vector<ColumnInfo> mColumnVector;
    MultiListArea* mListArea;
    int mDragColumn;
    intptr_t mDragComponent;
    int mDragX, mCurSortCol;
    bool mDrawSortArrow;
    MultiListHeader() : mListArea(NULL), mDragComponent(0), mDragX(0)
    {
        this->mDragColumn = -1;
        this->mCurSortCol = -1;
        this->mDrawSortArrow = true;
    }
    void SetMultiListArea(MultiListArea* area)
    {
        this->mListArea = area;
        if (this->mListArea != NULL) this->mListArea->mListHeader = kHead;
    }
    void SetSortColumn(int column, int type)
    {
        if (column < 0 || column >= (int)this->mColumnVector.size()) return;
        this->mColumnVector[column].mSortType = type;
        this->mCurSortCol = column;
    }
    void SetColumnItem(int column, intptr_t item)
    {
        this->mDragColumn = -1;
        if ((int)this->mColumnVector.size() <= column) this->mColumnVector.resize(column + 1);
        intptr_t old = this->mColumnVector[column].mComponent;
        if (old != 0) HH(Op::RemoveChild, old);
        HH(Op::ChildSetControlId, item, column);
        HH(Op::ChildSetFlags, item, 0x2000, 1);
        this->mColumnVector[column].mComponent = item;
        HH(Op::AddChild, item);
        if (this->mListArea != NULL && column < (int)this->mListArea->mColumnInfoVector.size() && this->mListArea->GetColumnWidth(column) <= 0) {
            int wh[2];
            HH(Op::ChildDesiredSize, item, (intptr_t)wh);
            this->mListArea->SetColumnWidth(column, wh[0], false);
            if (this->mListArea->mSortColumn == column) {
                this->mCurSortCol = column;
                this->mColumnVector[column].mSortType = (int)Base(LC::SortType);
            }
        } else this->NotifyColumnChange();
    }
    void NotifyColumnChange()
    {
        if (this->mListArea == NULL) return;
        std::vector<MultiListArea::ColumnInfo>& columns = this->mListArea->mColumnInfoVector;
        int size = (int)this->mColumnVector.size();
        std::vector<char> aHas(size);
        for (int i = 0; i < size; ++i) aHas[i] = this->mColumnVector[i].mComponent != 0;
        std::vector<int> aWidths;
        for (size_t i = 0; i < columns.size(); ++i) aWidths.push_back(columns[i].mWidth);
        std::vector<EeWonHeaderCol> aPlaces(size + 1);
        ee_won_gui_mh_layout((const bool*)(size ? &aHas[0] : NULL), size, aWidths.empty() ? NULL : &aWidths[0], aWidths.size(),
                             (int)HL(Op::Get, (intptr_t)Fd::HorzOffset), &aPlaces[0]);
        for (int i = 0; i < size; ++i) {
            intptr_t component = this->mColumnVector[i].mComponent;
            if (component != 0) {
                HH(Op::ChildSetEnabled, component, aPlaces[i].enabled);
                HH(Op::ChildSetVisible, component, aPlaces[i].width != 0);
                EeWonRect4 q = {aPlaces[i].x, 0, aPlaces[i].width, (int)HH(Op::Get, (intptr_t)Fd::Height)};
                HH(Op::ChildSetPosSize, component, (intptr_t)&q);
            }
        }
        if (this->mColumnVector.size() > 0) {
            intptr_t component = this->mColumnVector[this->mColumnVector.size() - 1].mComponent;
            HH(Op::ChildSetVisible, component, 1);
            int left = (int)HH(Op::ChildLeft, component);
            EeWonRect4 q = {left, 0, (int)HH(Op::Get, (intptr_t)Fd::Width), (int)HH(Op::Get, (intptr_t)Fd::Height)};
            HH(Op::ChildSetPosSize, component, (intptr_t)&q);
        }
        HH(Op::Invalidate);
    }
    void Virtual08() { HH(Op::BaseSizeChanged); this->NotifyColumnChange(); }
    int GetColumnIndex(intptr_t component)
    {
        for (int i = 0; i < (int)this->mColumnVector.size(); ++i)
            if (this->mColumnVector[i].mComponent == component) return i;
        return -1;
    }
    int GetSortType(int column)
    {
        int aType = ListSortType_None;
        if (column >= 0 && column < (int)this->mColumnVector.size()) aType = this->mColumnVector[column].mSortType;
        return ee_won_gui_mh_sort_type(column, this->mColumnVector.size(), this->mCurSortCol, aType);
    }
    void HandleComponentEvent(intptr_t event)
    {
        if (HH(Op::EventIsButtonPress, event) && this->mListArea != NULL) {
            intptr_t child = HH(Op::EventChildUnder, event);
            if (child == 0) return;
            int column = (int)HH(Op::ChildControlId, child);
            if (column >= 0 && column < (int)this->mListArea->mColumnInfoVector.size() && column < (int)this->mColumnVector.size()) {
                int type = ee_won_gui_mh_toggle(this->mColumnVector[column].mSortType, column == this->mCurSortCol);
                this->mListArea->SetSortColumn(column, type);
                return;
            }
        }
        HH(Op::BaseHandleEvent, event);
    }
    intptr_t GetDragComponent(int x)
    {
        this->mDragColumn = -1;
        this->mDragComponent = 0;
        std::vector<MultiListArea::ColumnInfo>& columns = this->mListArea->mColumnInfoVector;
        std::vector<EeWonHeaderButton> aButtons(this->mColumnVector.size());
        for (size_t i = 0; i < this->mColumnVector.size(); ++i) {
            intptr_t component = this->mColumnVector[i].mComponent;
            aButtons[i].present = component != 0;
            if (component != 0) {
                aButtons[i].left = (int)HH(Op::ChildLeft, component);
                aButtons[i].right = (int)HH(Op::ChildRight, component);
                aButtons[i].width = (int)HH(Op::ChildWidth, component);
            } else aButtons[i].left = aButtons[i].right = aButtons[i].width = 0;
        }
        std::vector<int> aWidths, aPartners;
        std::vector<unsigned char> aModes;
        for (size_t i = 0; i < columns.size(); ++i) {
            aWidths.push_back(columns[i].mWidth);
            aModes.push_back(columns[i].mSizeMode);
            aPartners.push_back(columns[i].mPartnerColumn);
        }
        int aDragX;
        int aColumn = ee_won_gui_mh_drag(x, aButtons.empty() ? NULL : &aButtons[0], aButtons.size(), aWidths.empty() ? NULL : &aWidths[0],
                                         aModes.empty() ? NULL : &aModes[0], aPartners.empty() ? NULL : &aPartners[0], columns.size(), &aDragX);
        if (aColumn < 0) return 0;
        this->mDragColumn = aColumn;
        this->mDragX = aDragX;
        this->mDragComponent = this->mColumnVector[this->mDragColumn].mComponent;
        return this->mDragComponent;
    }
    void MouseDown(int x, int y, int button)
    {
        if (button == 0 && this->GetDragComponent(x) != 0) return;
        HH(Op::BaseMouseDown, x, y, button);
    }
    void MouseMove(int x, int y)
    {
        HH(Op::BaseMouseMove, x, y);
        if (this->GetDragComponent(x) != 0) HH(Op::SetCursor, (intptr_t)EeWonGuiCursor::EastWest);
        else if (HH(Op::CursorIs, (intptr_t)EeWonGuiCursor::EastWest)) HH(Op::SetCursor, (intptr_t)EeWonGuiCursor::None);
    }
    void MouseDrag(int x, int y)
    {
        if (this->mDragColumn >= 0) {
            this->mListArea->SetColumnWidth(this->mDragColumn, ee_won_gui_mh_drag_width(x, this->mDragX), false);
            HH(Op::SetCursor, (intptr_t)EeWonGuiCursor::EastWest);
        } else HH(Op::BaseMouseDrag, x, y);
    }
    void MouseUp(int x, int y, int button)
    {
        if (button == 0 && this->mDragColumn != -1) this->mDragColumn = -1;
        else HH(Op::BaseMouseUp, x, y, button);
    }
    void MouseExit()
    {
        this->mDragColumn = -1;
        HH(Op::BaseMouseExit);
    }
};
} // namespace orig

struct OrigSide : Side {
    orig::MultiListArea* mMulti;
    orig::MultiListHeader* mHead;
    std::map<intptr_t, orig::MultiListItem*> mRows;
    OrigSide() : mMulti(new orig::MultiListArea), mHead(new orig::MultiListHeader) {}
    void Adjust(bool b) override { this->mMulti->AdjustPosition(b); }
    void Gone() override { this->mMulti->NotifyMouseItemGone(); }
    intptr_t MultiPtr() override { return (intptr_t)this->mMulti; }
    void NewRow(intptr_t id) override { this->mRows[id] = new orig::MultiListItem(id); }
    void FreeRow(intptr_t id) override { orig::MultiListItem* r = this->mRows[id]; this->mRows.erase(id); delete r; }
    intptr_t Row(RC c, intptr_t row, intptr_t a, intptr_t b, intptr_t cc) override
    {
        orig::MultiListItem* r = this->mRows[row];
        orig::MultiListArea* m = this->mMulti;
        if (r == NULL) return 0;
        switch (c) {
        case RC::CalcDimensions: r->CalcDimensions(m); return 0;
        case RC::Compare: return r->Compare(a, m);
        case RC::SetPos: r->SetPos((int)b); return 0;
        case RC::Paint: r->Paint(m); return 0;
        case RC::SetViewport: r->SetViewport(m, a); return 0;
        case RC::CanSelect: return r->CanSelect(m, (int)a, (int)b);
        case RC::MouseEnter: r->MouseEnter(m, (int)a, (int)b); return 0;
        case RC::MouseExit: r->MouseExit(m); return 0;
        case RC::MouseMove: r->MouseMove(m, (int)a, (int)b); return 0;
        case RC::MouseDrag: r->MouseDrag(m, (int)a, (int)b); return 0;
        case RC::MouseDown: r->MouseDown(m, (int)a, (int)b, (int)cc); return 0;
        case RC::MouseUp: r->MouseUp(m, (int)a, (int)b, (int)cc); return 0;
        case RC::SetItem: r->SetItem(a, (int)b); return 0;
        case RC::GetItem: return r->GetItem((int)a);
        case RC::InsertItem: r->InsertItem(a); return 0;
        }
        return 0;
    }
    intptr_t Multi(MC c, intptr_t a, intptr_t b, intptr_t cc) override
    {
        orig::MultiListArea* m = this->mMulti;
        switch (c) {
        case MC::AdjustPosition: m->AdjustPosition(a != 0); return 0;
        case MC::SizeChanged: m->Virtual08(); return 0;
        case MC::ScrollPaint: m->ScrollPaint(); return 0;
        case MC::NotifyMouseItemGone: m->NotifyMouseItemGone(); return 0;
        case MC::SetNumColumns: m->SetNumColumns((int)a); return 0;
        case MC::SetColumnWidth: m->SetColumnWidth((int)a, (int)b, cc != 0); return 0;
        case MC::SetSortColumn: m->SetSortColumn((int)a, (int)b); return 0;
        case MC::SetSortColumnFunc: m->SetSortColumnFunc((int)a, b); return 0;
        case MC::SetColumnHAlign: m->SetColumnHAlign((int)a, (int)b); return 0;
        case MC::SetColumnSizeMode: m->SetColumnSizeMode((int)a, (unsigned char)b); return 0;
        case MC::UpdateItem: return m->UpdateItem(a);
        case MC::SetCell: return m->SetCell((int)a, (int)b, cc);
        case MC::ColumnAt: return m->GetColumnAt((int)a);
        case MC::ColumnWidth: return m->GetColumnWidth((int)a);
        case MC::ColumnPos: return m->GetColumnPos((int)a);
        case MC::PaintContext: return m->mColumnPaintContext;
        case MC::SortColumn: return m->mSortColumn;
        case MC::SortColumnPrev: return m->mSortColumnPrev;
        case MC::SetDynamicSizeColumn: m->mDynamicSizeColumn = (int)a; return 0;
        case MC::SetAutoGrowColumns: m->mAutoGrowColumns = a != 0; return 0;
        case MC::NumColumns: return (intptr_t)m->mColumnInfoVector.size();
        case MC::TotalColumnWidth: return m->mTotalColumnWidth;
        case MC::MouseColItem: return m->mMouseColItem;
        case MC::Header: return m->mListHeader;
        }
        return 0;
    }
    intptr_t Head(HC c, intptr_t a, intptr_t b, intptr_t cc) override
    {
        orig::MultiListHeader* h = this->mHead;
        switch (c) {
        case HC::SizeChanged: h->Virtual08(); return 0;
        case HC::MouseExit: h->MouseExit(); return 0;
        case HC::MouseDown: h->MouseDown((int)a, (int)b, (int)cc); return 0;
        case HC::MouseUp: h->MouseUp((int)a, (int)b, (int)cc); return 0;
        case HC::MouseMove: h->MouseMove((int)a, (int)b); return 0;
        case HC::MouseDrag: h->MouseDrag((int)a, (int)b); return 0;
        case HC::HandleEvent: h->HandleComponentEvent(a); return 0;
        case HC::SetColumnItem: h->SetColumnItem((int)a, b); return 0;
        case HC::SetMultiListArea: h->SetMultiListArea(a ? this->mMulti : NULL); return 0;
        case HC::NotifyColumnChange: h->NotifyColumnChange(); return 0;
        case HC::SetSortColumn: h->SetSortColumn((int)a, (int)b); return 0;
        case HC::ColumnIndex: return h->GetColumnIndex(a);
        case HC::SortType: return h->GetSortType((int)a);
        case HC::DrawSortArrow: return h->mDrawSortArrow;
        case HC::ListArea: return h->mListArea ? kList : 0;
        case HC::DragColumn: return h->mDragColumn;
        }
        return 0;
    }
    void Free()
    {
        delete this->mHead;
        delete this->mMulti;
    }
};

struct RustSide : Side {
    EeWonMulti* mMulti;
    EeWonMHead* mHead;
    std::map<intptr_t, EeWonMRow*> mRows;
    RustSide()
    {
        this->mMulti = ee_won_multi_new((void*)kList, ScriptedHost, sList);
        this->mHead = ee_won_mhead_new((void*)kHead, ScriptedHost);
    }
    void Adjust(bool b) override { ee_won_multi_call(this->mMulti, MC::AdjustPosition, b, 0, 0); }
    void Gone() override { ee_won_multi_call(this->mMulti, MC::NotifyMouseItemGone, 0, 0, 0); }
    intptr_t MultiPtr() override { return (intptr_t)this->mMulti; }
    void NewRow(intptr_t id) override { this->mRows[id] = ee_won_mrow_new((void*)id, ScriptedHost); }
    void FreeRow(intptr_t id) override { EeWonMRow* r = this->mRows[id]; this->mRows.erase(id); ee_won_mrow_free(r); }
    intptr_t Row(RC c, intptr_t row, intptr_t a, intptr_t b, intptr_t cc) override
    {
        EeWonMRow* r = this->mRows[row];
        if (r == NULL) return 0;
        return ee_won_mrow_call(r, this->mMulti, c, a, b, cc);
    }
    intptr_t Multi(MC c, intptr_t a, intptr_t b, intptr_t cc) override
    {
        if (c == MC::ScrollPaint) a = kG;
        return ee_won_multi_call(this->mMulti, c, a, b, cc);
    }
    intptr_t Head(HC c, intptr_t a, intptr_t b, intptr_t cc) override
    {
        if (c == HC::SetMultiListArea) a = a ? kList : 0;
        return ee_won_mhead_call(this->mHead, c, a, b, cc);
    }
    void Free()
    {
        ee_won_mhead_free(this->mHead);
        ee_won_multi_free(this->mMulti);
    }
};

// ---- the driver

enum { kInsertRow, kSetCell, kAppendCell, kUpdate, kNumCols, kWidth, kSizeMode, kHAlign, kSortCol, kSortFunc, kDynamic,
       kAutoGrow, kSize, kAdjust, kPaint, kEnter, kMove, kDown, kUp, kDrag, kExit, kDelete, kClear, kFlags, kRefresh,
       kColAt, kColWidth, kColPos, kQuery, kHeadItem, kHeadSize, kHeadDown, kHeadMove, kHeadDrag, kHeadUp, kHeadExit,
       kHeadEvent, kHeadSort, kHeadIndex, kHeadNotify, kGone, kCalls };
static const char* kCallNames[kCalls] = {"insert-row", "set-cell", "append-cell", "update", "num-cols", "width", "size-mode",
    "halign", "sort-col", "sort-func", "dynamic", "auto-grow", "size", "adjust", "paint", "enter", "move", "down", "up", "drag",
    "exit", "delete", "clear", "flags", "refresh", "col-at", "col-width", "col-pos", "query", "head-item", "head-size",
    "head-down", "head-move", "head-drag", "head-up", "head-exit", "head-event", "head-sort", "head-index", "head-notify",
    "gone"};
static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }

static intptr_t NewCell()
{
    HostItem h;
    h.top = (int)DR(50); h.height = 0; h.width = 0;
    h.selected = DR(5) == 0; h.selectable = DR(6) != 0; h.row = false;
    h.calcH = DR(10) == 0 ? (int)DR(3) - 1 : 8 + (int)DR(14);
    h.calcW = 10 + (int)DR(120);
    h.key = (int)DR(12);
    h.refs = 1; // the creator's
    intptr_t id = 0x1000 + (++sNextItem);
    sItems[id] = h;
    return id;
}
static intptr_t NewRowItem()
{
    HostItem h;
    h.top = 0; h.height = 0; h.width = 0;
    h.selected = false; h.selectable = DR(6) != 0; h.row = true;
    h.calcH = 0; h.calcW = 0; h.key = 0;
    h.refs = 1;
    intptr_t id = 0x2000 + (++sNextItem);
    sItems[id] = h;
    sSide->NewRow(id);
    return id;
}
static intptr_t SomeRow()
{
    std::vector<intptr_t> in;
    for (std::map<intptr_t, HostItem>::const_iterator i = sItems.begin(); i != sItems.end(); ++i)
        if (i->second.row && i->second.refs > 0) in.push_back(i->first);
    if (in.empty()) return 0;
    return in[DR((unsigned)in.size())];
}
static intptr_t SomeButton()
{
    std::vector<intptr_t> in;
    for (std::map<intptr_t, HostComp>::const_iterator i = sComps.begin(); i != sComps.end(); ++i)
        if (i->first >= 0x3000 && i->first < 0x4000) in.push_back(i->first);
    if (in.empty() || DR(8) == 0) return 0x3fff;
    return in[DR((unsigned)in.size())];
}

static void Run()
{
    static const int kWeights[kCalls] = {8, 6, 2, 2, 2, 4, 3, 2, 3, 2, 1, 2, 2, 1, 3, 2, 4, 3, 2, 2, 1, 2, 1, 2, 1,
                                         1, 1, 1, 1, 4, 1, 3, 3, 3, 2, 1, 3, 1, 1, 1, 1};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r0 = (int)DR(aTotal), k = 0;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    long long a = 0, b = 0, c = 0;
    int nrows = (int)Base(LC::NumItems);
    int ncols = (int)sSide->Multi(MC::NumColumns);
    int w = sFields[(int)Fd::ScrollWidth] + 20;
    switch (k) {
    case kInsertRow: a = NewRowItem(); b = (int)DR(nrows + 3) - 1; break;
    case kSetCell: a = (int)DR(nrows + 2) - 1; b = (int)DR(ncols + 2); c = DR(8) == 0 ? 0 : NewCell(); break;
    case kAppendCell: a = SomeRow(); b = NewCell(); break;
    case kUpdate: case kRefresh: a = SomeRow(); break;
    case kNumCols: a = 1 + (int)DR(5); break;
    case kWidth: a = (int)DR(ncols + 2) - 1; b = (int)DR(160) - 10; c = DR(4) == 0; break;
    case kSizeMode: a = (int)DR(ncols + 1); b = DR(3); break;
    case kHAlign: a = (int)DR(ncols + 1); b = DR(3); break;
    case kSortCol: a = (int)DR(ncols + 2) - 1; b = DR(3); break;
    case kSortFunc: { static const intptr_t kF[] = {0, kFuncAsc, kFuncDesc}; a = (int)DR(ncols + 1); b = kF[DR(3)]; break; }
    case kDynamic: a = (int)DR(ncols + 2) - 1; break;
    case kAutoGrow: a = DR(2); break;
    case kAdjust: a = DR(2); break;
    case kEnter: case kMove: case kDrag: a = (int)DR(w) - 10; b = (int)DR(sFields[(int)Fd::ScrollHeight] + 20) - 10; break;
    case kDown: case kUp: a = (int)DR(w) - 10; b = (int)DR(sFields[(int)Fd::ScrollHeight] + 20) - 10; c = DR(4) == 0 ? 1 : 0; break;
    case kDelete: a = (int)DR(nrows + 2) - 1; break;
    case kClear: a = DR(2); break;
    case kFlags: { static const int kF[] = {0x10, 2, 4, 0x400, 0x200}; a = kF[DR(5)]; b = DR(3) != 0; break; }
    case kColAt: case kColWidth: case kColPos: a = k == kColAt ? (int)DR(400) - 20 : (int)DR(ncols + 2) - 1; break;
    case kQuery: a = DR(8); break;
    case kHeadItem: {
        a = (int)DR(ncols + 2);
        intptr_t id = 0x3000 + (++sNextComp);
        HostComp h = {0, (int)DR(80), -1, DR(3) == 0 ? 0 : 20 + (int)DR(80), 0, true, true};
        sComps[id] = h;
        b = id;
        break;
    }
    case kHeadDown: case kHeadUp: a = (int)DR(sHeadW + 20) - 10; b = (int)DR(20); c = DR(3) == 0 ? 1 : 0; break;
    case kHeadMove: case kHeadDrag: a = (int)DR(sHeadW + 20) - 10; b = (int)DR(20); break;
    case kHeadEvent: {
        a = 0x6000 + 2 * (++sNextEvent) + (DR(4) != 0 ? 1 : 0); // odd: a button press
        sEvents[(intptr_t)a] = DR(6) == 0 ? 0 : SomeButton();
        break;
    }
    case kHeadSort: a = (int)DR(ncols + 2) - 1; break;
    case kHeadIndex: a = SomeButton(); break;
    default: break;
    }
    char aLine[160];
    snprintf(aLine, sizeof(aLine), "call %s %llx %llx %llx", kCallNames[k], a, b, c);
    sTrace.push_back(aLine);
    size_t aCallLine = sTrace.size() - 1;
    intptr_t r = 0;
    switch (k) {
    case kInsertRow: {
        // the lobby fills a row's cells before inserting it (sorted inserts compare them)
        int aCells = DR(3) == 0 ? 0 : 1 + (int)DR(ncols + 1);
        for (int i = 0; i < aCells; i++) {
            intptr_t cell = DR(6) == 0 ? 0 : NewCell();
            snprintf(aLine, sizeof(aLine), "call row-cell %llx %llx 0", (long long)a, (long long)cell);
            sTrace.push_back(aLine);
            sSide->Row(RC::InsertItem, (intptr_t)a, cell);
            if (cell) sItems[cell].refs--;
        }
        r = Base(LC::InsertItem, (intptr_t)a, (intptr_t)b);
        sItems[(intptr_t)a].refs--;
        break;
    }
    case kSetCell:
        r = sSide->Multi(MC::SetCell, (intptr_t)a, (intptr_t)b, (intptr_t)c);
        if (c) { sItems[(intptr_t)c].refs--; }
        break;
    case kAppendCell:
        if (a) sSide->Row(RC::InsertItem, (intptr_t)a, (intptr_t)b);
        sItems[(intptr_t)b].refs--;
        break;
    case kUpdate: if (a) r = sSide->Multi(MC::UpdateItem, (intptr_t)a); break;
    case kNumCols: sSide->Multi(MC::SetNumColumns, (intptr_t)a); break;
    case kWidth: sSide->Multi(MC::SetColumnWidth, (intptr_t)a, (intptr_t)b, (intptr_t)c); break;
    case kSizeMode: sSide->Multi(MC::SetColumnSizeMode, (intptr_t)a, (intptr_t)b); break;
    case kHAlign: sSide->Multi(MC::SetColumnHAlign, (intptr_t)a, (intptr_t)b); break;
    case kSortCol: sSide->Multi(MC::SetSortColumn, (intptr_t)a, (intptr_t)b); r = sSide->Multi(MC::SortColumnPrev); break;
    case kSortFunc: sSide->Multi(MC::SetSortColumnFunc, (intptr_t)a, (intptr_t)b); break;
    case kDynamic: sSide->Multi(MC::SetDynamicSizeColumn, (intptr_t)a); break;
    case kAutoGrow: sSide->Multi(MC::SetAutoGrowColumns, (intptr_t)a); break;
    case kSize: sSide->Multi(MC::SizeChanged); r = sSide->Multi(MC::TotalColumnWidth); break;
    case kAdjust: sSide->Multi(MC::AdjustPosition, (intptr_t)a); break;
    case kPaint: sSide->Multi(MC::ScrollPaint, kG); r = sSide->Multi(MC::PaintContext); break;
    case kEnter: Base(LC::MouseEnter, (intptr_t)a, (intptr_t)b); r = sSide->Multi(MC::MouseColItem); break;
    case kMove: Base(LC::MouseMove, (intptr_t)a, (intptr_t)b); r = sSide->Multi(MC::MouseColItem); break;
    case kDown: Base(LC::MouseDown, (intptr_t)a, (intptr_t)b, (intptr_t)c); r = sSide->Multi(MC::MouseColItem); break;
    case kUp: Base(LC::MouseUp, (intptr_t)a, (intptr_t)b, (intptr_t)c); break;
    case kDrag: Base(LC::MouseDrag, (intptr_t)a, (intptr_t)b); break;
    case kExit: Base(LC::MouseExit); r = sSide->Multi(MC::MouseColItem); break;
    case kDelete: Base(LC::DeleteItem, (intptr_t)a); break;
    case kClear: Base(LC::Clear, (intptr_t)a); break;
    case kFlags: Base(LC::SetListFlags, (intptr_t)a, (intptr_t)b); break;
    case kRefresh: if (a) Base(LC::RefreshItem, (intptr_t)a); break;
    case kColAt: r = sSide->Multi(MC::ColumnAt, (intptr_t)a); break;
    case kColWidth: r = sSide->Multi(MC::ColumnWidth, (intptr_t)a); break;
    case kColPos: r = sSide->Multi(MC::ColumnPos, (intptr_t)a); break;
    case kQuery: {
        static const MC kQ[] = {MC::PaintContext, MC::SortColumn, MC::SortColumnPrev, MC::NumColumns, MC::TotalColumnWidth,
                                MC::MouseColItem, MC::Header, MC::NumColumns};
        r = sSide->Multi(kQ[a]);
        if (kQ[a] == MC::Header) r = r != 0;
        break;
    }
    case kHeadItem: sSide->Head(HC::SetColumnItem, (intptr_t)a, (intptr_t)b); break;
    case kHeadSize: sSide->Head(HC::SizeChanged); break;
    case kHeadDown: sSide->Head(HC::MouseDown, (intptr_t)a, (intptr_t)b, (intptr_t)c); r = sSide->Head(HC::DragColumn); break;
    case kHeadMove: sSide->Head(HC::MouseMove, (intptr_t)a, (intptr_t)b); r = sSide->Head(HC::DragColumn); break;
    case kHeadDrag: sSide->Head(HC::MouseDrag, (intptr_t)a, (intptr_t)b); break;
    case kHeadUp: sSide->Head(HC::MouseUp, (intptr_t)a, (intptr_t)b, (intptr_t)c); r = sSide->Head(HC::DragColumn); break;
    case kHeadExit: sSide->Head(HC::MouseExit); break;
    case kHeadEvent: sSide->Head(HC::HandleEvent, (intptr_t)a); break;
    case kHeadSort: r = sSide->Head(HC::SortType, (intptr_t)a); break;
    case kHeadIndex: r = sSide->Head(HC::ColumnIndex, (intptr_t)a); break;
    case kHeadNotify: sSide->Head(HC::NotifyColumnChange); r = sSide->Head(HC::DrawSortArrow) * 2 + (sSide->Head(HC::ListArea) != 0); break;
    case kGone: Base(LC::ClearMouseItem); sSide->Multi(MC::NotifyMouseItemGone); break;
    }
    sTrace[aCallLine] += " -> " + std::to_string((long long)r);
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sItems.clear();
    sComps.clear();
    sEvents.clear();
    sCursor.clear();
    sNextItem = sNextComp = sNextEvent = 0;
    sSeed = inSeq * 2654435761u + 37u;
    sDriverSeed = inSeq * 40503u + 17u;
    memset(sFields, 0, sizeof(sFields));
    sFields[(int)Fd::ScrollWidth] = 40 + (int)R(260);
    sFields[(int)Fd::ScrollHeight] = 20 + (int)R(180);
    sFields[(int)Fd::Width] = sFields[(int)Fd::ScrollWidth] + 4;
    sFields[(int)Fd::Height] = sFields[(int)Fd::ScrollHeight] + 4;
    sFields[(int)Fd::UpScrollAmount] = sFields[(int)Fd::DownScrollAmount] = 10;
    sHeadW = 100 + (int)R(200);
    sHeadH = 18;
    TraceFields();
    sList = ee_won_list_new((void*)kList, ScriptedHost);
    OrigSide* o = NULL;
    RustSide* rs = NULL;
    if (inRust) sSide = rs = new RustSide;
    else sSide = o = new OrigSide;
    // the constructors: MultiListArea(columns), MultiListHeader(area)
    int aColumns = 1 + (int)DR(4);
    sTrace.push_back("call new " + std::to_string(aColumns));
    sSide->Multi(MC::SetNumColumns, aColumns);
    sSide->Head(HC::SetMultiListArea, 1);
    for (int i = 0; i < inCalls; i++) Run();
    sTrace.push_back("free");
    // the header and the list's own objects first, then the ListArea base (its rows, the rows their cells)
    if (inRust) rs->Free(); else o->Free();
    ee_won_list_free(sList);
    std::vector<intptr_t> left;
    for (std::map<intptr_t, HostItem>::const_iterator i = sItems.begin(); i != sItems.end(); ++i)
        if (i->second.row && i->second.refs == 0) left.push_back(i->first);
    delete sSide;
    sSide = NULL;
    sList = NULL;
    bool aBack = true;
    for (std::map<intptr_t, HostItem>::const_iterator i = sItems.begin(); i != sItems.end(); ++i)
        if (i->second.refs != 0) aBack = false;
    for (std::map<intptr_t, HostComp>::const_iterator i = sComps.begin(); i != sComps.end(); ++i)
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
    if (!aPrint) {
        printf("ops met:");
        for (std::set<std::string>::const_iterator i = aSeen.begin(); i != aSeen.end(); ++i)
            if (i->compare(0, 2, "op") == 0) printf(" %s", i->c_str() + 2);
        printf("\n");
    }
    if (!aPrint)
        printf("won_multiobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
