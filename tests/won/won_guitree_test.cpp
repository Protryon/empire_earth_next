// WONGUI's tree list (TreeArea.cpp, TreeItem.cpp): the original's statements (copied from the C++ before the port,
// the items replaced by a plain node struct, the list rows by a log of the positions they were inserted at, the
// wrapped items' Contains by recorded answers) against ee_won's guitree module through its C ABI, over generated
// trees and inputs (a fixed LCG). Prints one line per failed check; the exit code is the number of failed checks.
#include <stdio.h>
#include <stddef.h>
#include <vector>
#include <utility>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 7311;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
static int Any()
{
    switch (Next() % 8) {
    case 0: return (int)Next() - (int)(Next() << 8);
    case 1: return 0x7fffffff - (int)(Next() % 3);
    case 2: return (int)0x80000000 + (int)(Next() % 3);
    default: return (int)(Next() % 300) - 30;
    }
}

// WONLobbyGUI.h's event numbers up to the button press
enum WindowEventType {
    WindowEventType_None, InputEvent_SizeChange, InputEvent_MouseMove, InputEvent_MouseDown, InputEvent_MouseDoubleClick,
    InputEvent_MouseDownCheckDoubleClick, InputEvent_MouseUp, InputEvent_MouseWheel, InputEvent_KeyDown,
    InputEvent_KeyUp, InputEvent_KeyChar, InputEvent_GotFocus, InputEvent_LostFocus, InputEvent_MouseExit,
    InputEvent_WindowClose, ComponentEvent_ButtonPressed
};

struct TreeItem {
    TreeItem* mParent;
    int mIndex;
    bool mOpen;
    std::vector<TreeItem*> mChildren;
    bool IsOpen() { return mOpen; }
    // the original's GetNextNonChild
    TreeItem* GetNextNonChild()
    {
        if(this->mParent==NULL)
            return NULL;
        else if(this->mIndex>=this->mParent->mChildren.size()-1)
            return this->mParent->GetNextNonChild();
        else
            return this->mParent->mChildren[this->mIndex+1];
    }
    // the original's InsertChildren, the rows' inserts logged
    void InsertChildren(std::vector<std::pair<TreeItem*,int> >& rows, int thePos)
    {
        for(int i=this->mChildren.size()-1; i>=0; i--)
        {
            TreeItem *anItem = this->mChildren[i];
            rows.push_back(std::make_pair(anItem,thePos));
            if(anItem->IsOpen())
                anItem->InsertChildren(rows, thePos+1);
        }
    }
    // the original's CloseAll, the order logged
    void CloseAll(std::vector<TreeItem*>& log)
    {
        this->mOpen = false;
        log.push_back(this);
        std::vector<TreeItem*>::iterator anItr = this->mChildren.begin();
        while(anItr!=this->mChildren.end())
        {
            (*anItr)->CloseAll(log);
            ++anItr;
        }
    }
};

static std::vector<TreeItem*> sNodes;
static TreeItem* Make(TreeItem* parent, int depth)
{
    TreeItem* n = new TreeItem;
    sNodes.push_back(n);
    n->mParent = parent;
    n->mIndex = -1;
    n->mOpen = Next() % 3 != 0;
    int kids = depth >= 4 ? 0 : (int)(Next() % 5);
    for (int i = 0; i < kids; i++) {
        TreeItem* c = Make(n, depth + 1);
        c->mIndex = Next() % 23 == 0 ? (int)(Next() % 7) - 3 : i;
        n->mChildren.push_back(c);
    }
    return n;
}

static std::vector<std::pair<TreeItem*,int> > sRows;
static std::vector<TreeItem*> sClosed;
static void* OpParent(void*, void* n) { return static_cast<TreeItem*>(n)->mParent; }
static int OpIndex(void*, void* n) { return static_cast<TreeItem*>(n)->mIndex; }
static size_t OpCount(void*, void* n) { return static_cast<TreeItem*>(n)->mChildren.size(); }
static void* OpChild(void*, void* n, size_t i) { return static_cast<TreeItem*>(n)->mChildren[i]; }
static bool OpIsOpen(void*, void* n) { return static_cast<TreeItem*>(n)->mOpen; }
static void OpInsert(void*, void* n, int pos) { sRows.push_back(std::make_pair(static_cast<TreeItem*>(n), pos)); }
static void OpClose(void*, void* n) { static_cast<TreeItem*>(n)->mOpen = false; sClosed.push_back(static_cast<TreeItem*>(n)); }

static bool sAnswers[2];
static std::vector<int> sAsked;
static bool Contains(void*, int which) { sAsked.push_back(which); return sAnswers[which]; }

static void Walks()
{
    EeWonTreeOps ops = { NULL, OpParent, OpIndex, OpCount, OpChild, OpIsOpen, OpInsert, OpClose };
    for (int n = 0; n < 3000; n++) {
        sNodes.clear();
        TreeItem* root = Make(NULL, Next() % 3);
        for (size_t i = 0; i < sNodes.size(); i++) {
            TreeItem* t = sNodes[i];
            // any index: one that is not the last is below the count, so the walk stays inside the vectors
            CHECK(t->GetNextNonChild() == ee_won_gui_tree_next_non_child(&ops, t));
            std::vector<std::pair<TreeItem*,int> > o;
            int pos = Next() % 9 == 0 ? Any() : (int)(Next() % 40);
            t->InsertChildren(o, pos);
            sRows.clear();
            ee_won_gui_tree_insert_children(&ops, t, pos);
            CHECK(o == sRows);
        }
        // CloseAll, on a copy's open flags
        TreeItem* t = sNodes[Next() % sNodes.size()];
        std::vector<bool> before;
        for (size_t i = 0; i < sNodes.size(); i++) before.push_back(sNodes[i]->mOpen);
        std::vector<TreeItem*> o;
        t->CloseAll(o);
        std::vector<bool> after;
        for (size_t i = 0; i < sNodes.size(); i++) { after.push_back(sNodes[i]->mOpen); sNodes[i]->mOpen = before[i]; }
        sClosed.clear();
        ee_won_gui_tree_close_all(&ops, t);
        CHECK(o == sClosed);
        for (size_t i = 0; i < sNodes.size(); i++) CHECK(after[i] == sNodes[i]->mOpen);
        for (size_t i = 0; i < sNodes.size(); i++) delete sNodes[i];
        (void)root;
    }
}

static void Rules()
{
    for (int n = 0; n < 400000; n++) {
        int a = Any(), b = Any(), c = Any(), d = Any(), e = Any();
        size_t count = Next() % 4 == 0 ? (size_t)Any() : Next() % 6;
        // IsLastChild / GetNextNonChild's test, GetChild's range
        {
            int mIndex = Next() % 3 ? (int)(Next() % 8) - 2 : a;
            std::vector<int> v(count < 64 ? count : 0);
            size_t sz = count < 64 ? count : 0;
            CHECK((mIndex>=sz-1) == ee_won_gui_tree_is_last(mIndex, sz));
            int thePos = Next() % 3 ? (int)(Next() % 8) - 2 : b;
            CHECK(!(thePos<0 || thePos>=sz) == ee_won_gui_tree_child_in_range(thePos, sz));
        }
        // SetOpen's steps
        {
            bool mOpen = Next() & 1, isOpen = Next() & 1, empty = Next() & 1;
            int o = -1;
            do {
                if(mOpen==isOpen) { o = 0; break; }
                if(empty) { o = 1; break; }
                if(!isOpen) o = 2; else o = 3;
            } while (0);
            CHECK(o == ee_won_gui_tree_set_open_action(mOpen, isOpen, empty));
            bool hasSibling = Next() & 1;
            int mTotalHeight = a, siblingTop = b, mVertOffset = c, height = d, mTop = e;
            int aSiblingYPos = mTotalHeight;
            if(hasSibling) aSiblingYPos = siblingTop;
            int aVertOffset = mVertOffset;
            bool scrolled = false;
            if(aSiblingYPos > aVertOffset + height)
            {
                aVertOffset = aSiblingYPos - height;
                if(mTop < aVertOffset)
                    aVertOffset = mTop;
                scrolled = true;
            }
            int r = 12345;
            bool rs = ee_won_gui_tree_open_scroll(mTotalHeight, hasSibling, siblingTop, mVertOffset, height, mTop, &r);
            CHECK(rs == scrolled);
            if (scrolled) CHECK(r == aVertOffset);
        }
        // InsertChild's row, RemoveChild's steps, Sort's plan
        {
            bool open = Next() & 1, onEnd = Next() & 1;
            int o = !open ? 0 : onEnd ? 1 : 2;
            CHECK(o == ee_won_gui_tree_insert_row(open, onEnd));
            bool childOpen = Next() & 1;
            size_t sz = 1 + Next() % 4;
            int steps = 0;
            if(childOpen) steps |= 1;
            if(open) steps |= 2;
            std::vector<int> v(sz);
            v.erase(v.begin());
            if(v.empty()) steps |= 4;
            CHECK(steps == ee_won_gui_tree_remove_plan(childOpen, open, sz));
            bool none = Next() % 3 == 0, isNull = Next() % 3 == 0;
            int row = (int)(Next() % 5) - 2;
            int plan;
            do {
                if(none) { plan = 0; break; }
                if(isNull) { plan = 1; break; }
                bool isVisible = open && row>=0;
                plan = isVisible ? 3 : 2;
            } while (0);
            CHECK(plan == ee_won_gui_tree_sort_plan(none, isNull, open, open ? row : -1));
        }
        // GetMouseItem
        {
            static int items[3];
            int* col = Next() % 3 ? &items[Next() % 2] : NULL;
            int* nw = Next() % 4 ? &items[Next() % 2] : NULL;
            sAnswers[0] = Next() & 1;
            sAnswers[1] = Next() & 1;
            std::vector<int> asked;
            int steps = 0;
            int* aMouseColItem = col;
            if(aMouseColItem!=NULL && (aMouseColItem!=nw || !(asked.push_back(0), sAnswers[0])))
                steps |= 1;
            if(aMouseColItem==NULL)
            {
                if(nw!=NULL && (asked.push_back(1), sAnswers[1]))
                    steps |= 2;
            }
            sAsked.clear();
            CHECK(steps == ee_won_gui_tree_mouse_item(col!=NULL, col==nw, nw!=NULL, Contains, NULL));
            CHECK(asked == sAsked);
        }
        // CanSelect, ContainsPoint's x, CalcDimensions' width, the expand press, the flags
        {
            bool mSelectable = Next() & 1;
            int x = a, mX = Next() % 3 ? (int)(Next() % 60) : b;
            CHECK((mSelectable && x>=mX) == ee_won_gui_tree_can_select(mSelectable, x, mX));
            int lx = 777;
            bool in = ee_won_gui_tree_local_x(x, mX, &lx);
            CHECK(in == !(x<mX));
            if (in) CHECK(lx == x-mX);
            CHECK(mX+c == ee_won_gui_tree_row_width(mX, c));
            int type = Next() % 3 ? ComponentEvent_ButtonPressed : (int)(Next() % 40);
            int id = Next() % 3 ? 100003 : (Next() % 2 ? 100002 : d);
            CHECK((type==ComponentEvent_ButtonPressed && id==100003) == ee_won_gui_tree_is_expand_press(type, id));
            int mTreeFlags = e, theFlags = (int)(Next() % 16);
            bool on = Next() & 1;
            int o = mTreeFlags;
            if(on) o |= theFlags; else o &= ~theFlags;
            CHECK(o == ee_won_gui_tree_flags(mTreeFlags, theFlags, on));
        }
    }
}

int main()
{
    Walks();
    Rules();
    printf("won_guitree_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
