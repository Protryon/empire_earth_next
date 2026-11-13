// Container as a whole object (R6 WON round 26): the original Container units' methods (Container.cpp,
// ContainerChildLifecycle.cpp, ContainerInteraction.cpp, ContainerLifetime.cpp, ContainerOrdering.cpp,
// ContainerSetup.cpp, ContainerTimerCursor.cpp and MouseAt), the statements copied from the C++ before the port with
// every call on a C++ object (a child, the parent, the Component base, the container's own virtuals, the Graphics,
// the child layout, the cursor) replaced by the host op that makes it (Container.cpp's host runs the same calls in
// the same order), against ee_won's Rust Container (ctobj.rs) over one scripted host.
//
// The host keeps a world of twelve components, the first four of them containers (each side has its own four
// container objects): their bounds, flags, parents, timer flags, invalid states, focus ids and control ids, and the
// root, the window's focus, the mouse and the cursors. A call on a component runs what the C++ runs: a container's
// overrides on that side's container ("do" lines), and Component's own behaviour for the rest (the focus flag, the
// invalid state, RequestFocus, RequestTimer, Invalidate and AddedToParent calling the parent back, TabFocus); a leaf's
// other answers (a key, a timer tick, CanClose, a hit) are drawn from the driver. What changes the world is traced
// (the ops, "set" lines); the pure reads (flags, bounds, parents, the root, the mouse, the sibling walk) are answered
// from it and not traced: their order in the original is the compiler's argument evaluation.
//
// Each of 20,000 sequences builds a tree and makes 40 random calls on random containers, with random changes of the
// world between them, then frees the containers. The traces must be the same, every reference given back. The first
// 40 sequences and every later one that makes an op or a call none before made are
// rust/crates/ee_won/src/golden_ctobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonCtVirt V;
typedef EeWonCtCall C;

static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c);
static intptr_t CtObjectOf(intptr_t ct);
static bool ExtraRead(intptr_t, Op, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t*) { return false; }
static void ExtraOp(intptr_t, Op, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t*) {}
static bool ExtraLine(intptr_t, Op, intptr_t, intptr_t, intptr_t, intptr_t) { return false; }
static intptr_t Host(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
#include "won_ct_world.h"

// ---- the original (the Container units; every C++ call through the host)

namespace orig { struct Container; }
static orig::Container* sOrig[kCt + 1];
static EeWonContainer* sRust[kCt + 1];
namespace orig {
struct Container {
    intptr_t mSelf;
    intptr_t mMouseChild, mFocusChild;
    std::list<intptr_t> mChildList;
    std::map<intptr_t, std::list<intptr_t>::iterator> mItr; // each child's mChildListItr
    std::set<uintptr_t> mChildTimerSet;
    unsigned char mNextFocusId;
    intptr_t mCurrentCursor;
    bool mMouseWheelGoesToFocusChild, mIsolateTabFocus, mHasUsedDialogKeys, mOnlyChildInput, mRemovingAllChildren;
    int mModalCount;

    intptr_t H(Op op, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return Host((void*)mSelf, op, a, b, c, d); }
    // the calls
    intptr_t Base(V v, intptr_t a = 0, intptr_t b = 0) { return H(Op::CtBase, (intptr_t)v, a, b); }
    intptr_t Child(intptr_t ch, V v, intptr_t a = 0, intptr_t b = 0) { return H(Op::CtChild, ch, (intptr_t)v, a, b); }
    intptr_t Parent(V v, intptr_t a = 0) { return H(Op::CtParentCall, (intptr_t)v, a); }
    intptr_t Virt(V v, intptr_t a = 0, intptr_t b = 0) { return H(Op::CtVirt, (intptr_t)v, a, b); }
    void Translate(intptr_t g, int x, int y)
    {
        EeWonDrawOp o; memset(&o, 0, sizeof(o)); o.kind = EE_WON_DRAW_TRANSLATE; o.a = x; o.b = y;
        H(Op::Draw, g, (intptr_t)&o);
    }
    void PushClip(intptr_t g, int x, int y, int w, int h)
    {
        EeWonDrawOp o; memset(&o, 0, sizeof(o)); o.kind = EE_WON_DRAW_PUSH_CLIP; o.a = x; o.b = y; o.c = w; o.d = h;
        H(Op::Draw, g, (intptr_t)&o);
    }
    void PopClip(intptr_t g)
    {
        EeWonDrawOp o; memset(&o, 0, sizeof(o)); o.kind = EE_WON_DRAW_POP_CLIP;
        H(Op::Draw, g, (intptr_t)&o);
    }
    // the pure reads
    uint32_t Bits(intptr_t ch) { return (uint32_t)H(Op::CtBits, ch); }
    bool FlagSet(intptr_t ch, uint32_t f) { return (Bits(ch) & f) != 0; }
    bool IsVisible(intptr_t ch) { return !FlagSet(ch, 1); }
    bool IsDisabled(intptr_t ch) { return FlagSet(ch, 0x80); }
    intptr_t ParentOf(intptr_t ch) { return H(Op::CtParentOf, ch); }
    int Left(intptr_t ch) { return (int)H(Op::ChildLeft, ch); }
    int Top(intptr_t ch) { return (int)H(Op::ChildTop, ch); }
    int Width(intptr_t ch) { return (int)H(Op::ChildWidth, ch); }
    int Height(intptr_t ch) { return (int)H(Op::ChildHeight, ch); }
    bool Contains(intptr_t ch, int x, int y) { return H(Op::CtContains, ch, x, y) != 0; }
    void Hold(intptr_t ch) { H(Op::ChildHold, ch); }
    void Release(intptr_t ch) { H(Op::ChildRelease, ch); }

    Container(intptr_t self, bool wheelDef)
    {
        mSelf = self;
        this->mMouseChild = 0;
        this->mFocusChild = 0;
        this->mCurrentCursor = 0;
        this->mMouseWheelGoesToFocusChild = wheelDef;
        this->mOnlyChildInput = false;
        this->mIsolateTabFocus = false;
        this->mRemovingAllChildren = false;
        this->mHasUsedDialogKeys = false;
        this->mModalCount = 0;
        this->mNextFocusId = 0;
    }
    void SetCursorField(intptr_t c)
    {
        // SmartPtr<Cursor>::operator=: the old one released, then the new one held
        if (this->mCurrentCursor != c) {
            if (this->mCurrentCursor != 0) H(Op::CtCursorRelease, this->mCurrentCursor);
            this->mCurrentCursor = c;
            if (c != 0) H(Op::CtCursorHold, c);
        }
    }
    // ~Container: the children's parents cleared; then the members: the cursor, the child layout, the timer set
    // (its tree's order: here the set's order) and the list (front to back)
    void Destroy()
    {
        for (std::list<intptr_t>::iterator child = this->mChildList.begin(); child != this->mChildList.end(); ++child)
            H(Op::CtSetParent, *child, 0);
        SetCursorField(0);
        H(Op::CtLayout, 3);
        for (std::set<uintptr_t>::iterator i = mChildTimerSet.begin(); i != mChildTimerSet.end(); ++i) Release((intptr_t)*i);
        mChildTimerSet.clear();
        for (std::list<intptr_t>::iterator i = mChildList.begin(); i != mChildList.end(); ++i) Release(*i);
        mChildList.clear();
    }

    // Container.cpp
    void Virtual08()
    {
        Base(V::SizeChanged);
        H(Op::CtLayout, 0);
    }
    void GetDesiredSize(int& width, int& height)
    {
        std::vector<int32_t> aRects;
        for (std::list<intptr_t>::iterator i = this->mChildList.begin(); i != this->mChildList.end(); ++i) {
            intptr_t child = *i;
            aRects.push_back(Left(child));
            aRects.push_back(Top(child));
            aRects.push_back(Width(child));
            aRects.push_back(Height(child));
        }
        width = ee_won_gui_ct_desired_size(aRects.empty() ? NULL : &aRects[0], aRects.size() / 4, &height);
    }
    void PrePaint(intptr_t g)
    {
        if (!this->mChildList.empty()) {
            std::list<intptr_t>::iterator i = this->mChildList.end();
            do {
                --i;
                intptr_t child = *i;
                if (Child(child, V::NeedsPaint)) {
                    Translate(g, Left(child), Top(child));
                    Child(child, V::PrePaint, g);
                    Translate(g, -Left(child), -Top(child));
                }
            } while (i != this->mChildList.begin());
        }
        Base(V::PrePaint, g);
    }
    void EnableControlsById(intptr_t ids, bool enabled)
    {
        Base(V::EnableControlsById, ids, enabled);
        for (std::list<intptr_t>::iterator i = this->mChildList.begin(); i != this->mChildList.end(); ++i)
            Child(*i, V::EnableControlsById, ids, enabled);
    }
    void NotifyRootVisibility(bool visible)
    {
        Base(V::NotifyRootVisibility, visible);
        this->mHasUsedDialogKeys = false;
        for (std::list<intptr_t>::iterator i = this->mChildList.begin(); i != this->mChildList.end(); ++i) {
            intptr_t child = *i;
            if (IsVisible(child)) Child(child, V::NotifyRootVisibility, visible);
        }
    }
    bool HitTest(int x, int y)
    {
        if (!Base(V::HitTest, x, y)) return false;
        if (this->mOnlyChildInput) {
            if (this->mChildList.empty()) return false; // (the original read the list's head: not driven)
            std::list<intptr_t>::iterator i = this->mChildList.end();
            do {
                --i;
                intptr_t child = *i;
                if (Contains(child, x, y) && Child(child, V::HitTest, x - Left(child), y - Top(child))) return true;
            } while (i != this->mChildList.begin());
            return false;
        }
        return true;
    }
    intptr_t FindChildAt(int x, int y)
    {
        if (this->mChildList.empty())
            return 0;
        std::list<intptr_t>::iterator i = this->mChildList.end();
        do {
            --i;
            intptr_t child = *i;
            if (Contains(child, x, y) && Child(child, V::HitTest, x - Left(child), y - Top(child))) {
                return ee_won_gui_hit_child(IsDisabled(child), this->mModalCount > 0, Bits(child)) ? child : 0;
            }
        } while (i != this->mChildList.begin());
        return 0;
    }
    void UpdateMouseChild(int x, int y)
    {
        intptr_t aChild = Virt(V::FindChildAt, x, y);
        uint8_t aSteps = ee_won_gui_ct_mouse_child(aChild == this->mMouseChild, this->mMouseChild != 0, aChild != 0);
        if (aChild == this->mMouseChild)
            return;
        if (aSteps & 1) {
            Child(this->mMouseChild, V::MouseExit);
            this->mMouseChild = aChild;
            Virt(V::SetCursor, H(Op::CtDefaultCursor));
        } else this->mMouseChild = aChild;
        if (aSteps & 2) Child(aChild, V::MouseEnter, x - Left(aChild), y - Top(aChild));
    }
    void MouseEnter(int x, int y)
    {
        Base(V::MouseEnter, x, y);
        this->UpdateMouseChild(x, y);
    }
    void MouseExit()
    {
        Base(V::MouseExit);
        if (this->mMouseChild != 0) {
            Child(this->mMouseChild, V::MouseExit);
            this->mMouseChild = 0;
        }
        SetCursorField(0);
    }
    void MouseMove(int x, int y)
    {
        this->UpdateMouseChild(x, y);
        intptr_t aChild = this->mMouseChild;
        if (aChild != 0) Child(aChild, V::MouseMove, x - Left(aChild), y - Top(aChild));
    }
    void MouseDrag(int x, int y)
    {
        intptr_t aChild = this->mMouseChild;
        if (aChild != 0) Child(aChild, V::MouseDrag, x - Left(aChild), y - Top(aChild));
    }
    void MouseUp(int x, int y, int button)
    {
        intptr_t anOld = this->mMouseChild;
        this->UpdateMouseChild(x, y);
        intptr_t aChild = this->mMouseChild;
        if (ee_won_gui_ct_release_to_child(aChild != 0, aChild == anOld))
            H(Op::CtChildMouseUp, aChild, x - Left(aChild), y - Top(aChild), button);
    }
    void MouseDown(int x, int y, int button)
    {
        this->UpdateMouseChild(x, y);
        intptr_t aChild = this->mMouseChild;
        if (aChild != 0) H(Op::CtChildMouseDown, aChild, x - Left(aChild), y - Top(aChild), button);
    }
    std::list<intptr_t>::iterator FindTop(std::list<intptr_t>::iterator i)
    {
        std::vector<uint8_t> anOnTop;
        for (std::list<intptr_t>::iterator j = this->mChildList.begin(); j != i; ++j)
            anOnTop.push_back(FlagSet(*j, 0x1000) ? 1 : 0);
        size_t aPlace = ee_won_gui_ct_find_top(anOnTop.empty() ? NULL : &anOnTop[0], anOnTop.size(), anOnTop.size());
        std::list<intptr_t>::iterator r = this->mChildList.begin();
        std::advance(r, aPlace);
        return r;
    }
    std::list<intptr_t>::iterator FindTop() { return this->FindTop(this->mChildList.end()); }
    void AddChild(intptr_t child)
    {
        if (ParentOf(child) != 0) H(Op::CtDetach, child);
        H(Op::CtSetParent, child, 1);
        Hold(child); // SmartPtr<Component>(child)
        this->mItr[child] = this->mChildList.insert(this->FindTop(), child);
        Child(child, V::AddedToParent);
    }
    void AddChildEx(intptr_t child, int position)
    {
        if (ParentOf(child) != 0) H(Op::CtDetach, child);
        H(Op::CtSetParent, child, 1);
        std::list<intptr_t>::iterator i = this->mChildList.begin();
        for (int n = 0; n < position && i != this->mChildList.end(); ++n) ++i;
        Hold(child);
        this->mItr[child] = this->mChildList.insert(this->FindTop(i), child);
        Child(child, V::AddedToParent);
    }
    void HandleBroadcastEvent(intptr_t event)
    {
        Base(V::HandleBroadcastEvent, event);
        for (std::list<intptr_t>::iterator i = this->mChildList.begin(); i != this->mChildList.end(); ++i)
            Child(*i, V::HandleBroadcastEvent, event);
    }
    intptr_t FindChildById(int inControlId)
    {
        std::list<intptr_t> pending;
        pending.push_back(mSelf);
        for (std::list<intptr_t>::iterator i = pending.begin(); i != pending.end(); ++i) {
            intptr_t component = *i;
            if ((int)H(Op::ChildControlId, component) == inControlId)
                return component;
            Container* container = (Container*)H(Op::CtContainerOf, component);
            if (container == NULL)
                continue;
            std::list<intptr_t>::iterator child = container->mChildList.begin();
            while (child != container->mChildList.end()) {
                pending.push_back(*child);
                ++child;
            }
        }
        return 0;
    }
    // ContainerChildLifecycle.cpp
    void EnterModal()
    {
        ++this->mModalCount;
        if (this->mMouseChild != 0 && !FlagSet(this->mMouseChild, 0x8000)) this->MouseAt();
        if (this->mFocusChild != 0 && !FlagSet(this->mFocusChild, 0x8000)) this->SetFocusToLastFocusChild();
    }
    void LeaveModal()
    {
        if (!(this->mModalCount > 0))
            return;
        this->mModalCount = 0;
        this->MouseAt();
        if (this->mFocusChild == 0) this->SetFocusToLastFocusChild();
    }
    void Erase(intptr_t child)
    {
        // a child a reset dropped from the list keeps its parent: the original erased a dead iterator (not driven
        // to a crash: skipped, as the Rust)
        std::map<intptr_t, std::list<intptr_t>::iterator>::iterator f = mItr.find(child);
        if (f == mItr.end()) return;
        this->mChildList.erase(f->second);
        mItr.erase(f);
        Release(child);
    }
    void RemoveChild(intptr_t child)
    {
        if (ParentOf(child) != mSelf) return;
        if (!this->mRemovingAllChildren && FlagSet(child, 0x100)) {
            H(Op::ChildInvalidate, child);
            intptr_t root = H(Op::CtRoot);
            if (root != 0) H(Op::CtAddBG, H(Op::CtGrabBG, child));
        } else H(Op::Invalidate);
        Hold(child); // keep
        Erase(child);
        if (!this->mRemovingAllChildren) H(Op::CtLayout, 1, child);
        if (child == this->mFocusChild) this->SetFocusToLastFocusChild();
        if (child == this->mMouseChild) this->MouseAt();
        H(Op::CtSetParent, child, 0);
        Release(child); // keep
    }
    void RemoveAllChildren()
    {
        this->mRemovingAllChildren = true;
        while (!this->mChildList.empty()) Virt(V::RemoveChild, this->mChildList.front());
        H(Op::CtLayout, 2);
        this->mRemovingAllChildren = false;
        Virt(V::SetCursor, 0);
    }
    void BringToTop(intptr_t child)
    {
        if (ParentOf(child) != mSelf) return;
        Hold(child); // keep
        intptr_t grabber = H(Op::CtGrabBG, child);
        bool needInvalidate = true;
        if (grabber != 0) {
            H(Op::ChildInvalidate, child);
            H(Op::CtAddBG, grabber);
            needInvalidate = false;
        }
        Erase(child);
        Hold(child);
        this->mItr[child] = this->mChildList.insert(this->FindTop(), child);
        if (needInvalidate) H(Op::Invalidate);
        Release(child); // keep
    }
    void SetFocusChild(intptr_t child)
    {
        if (!IsVisible(mSelf) || IsDisabled(mSelf)) return;
        if (this->mFocusChild == child && FlagSet(mSelf, 0x10)) return;
        if (this->mModalCount > 0 && child != 0 && !FlagSet(child, 0x8000)) return;
        if (this->mFocusChild != 0 && FlagSet(mSelf, 0x10)) Child(this->mFocusChild, V::LostFocus);
        this->mFocusChild = child;
        unsigned char id = this->GetNextFocusId();
        if (child == 0) return; // (the original wrote the id through NULL: not driven)
        H(Op::CtSetFocusId, child, id);
        if (!FlagSet(mSelf, 0x10) && ParentOf(mSelf) != 0) Parent(V::SetFocusChild);
        else {
            H(Op::ChildSetFlags, mSelf, 0x10, 1);
            if (ParentOf(mSelf) == 0 && H(Op::CtRoot) == mSelf) {
                if (!H(Op::CtRootFocused)) return;
            }
            Child(child, V::GotFocus);
        }
    }
    void ChildStateChanged(intptr_t child)
    {
        bool bad = IsDisabled(child) || !IsVisible(child);
        if (this->mFocusChild == child && bad) {
            Child(this->mFocusChild, V::LostFocus);
            this->mFocusChild = 0;
        }
        if (H(Op::CtRoot) == 0) return;
        int x, y;
        H(Op::GetMousePos, (intptr_t)&x, (intptr_t)&y);
        if (Contains(child, x, y) || (this->mMouseChild == child && bad)) this->UpdateMouseChild(x, y);
    }
    // ContainerInteraction.cpp
    void Paint(intptr_t g)
    {
        intptr_t scheme = H(Op::CtScheme, g);
        Base(V::Paint, g);
        for (std::list<intptr_t>::iterator i = this->mChildList.begin(); i != this->mChildList.end(); ++i) {
            intptr_t child = *i;
            if (H(Op::CtInvalidState, child) == 0 || !IsVisible(child))
                continue;
            Translate(g, Left(child), Top(child));
            if (FlagSet(child, 0x2000)) PushClip(g, 0, 0, Width(child), Height(child));
            if (H(Op::CtOwnScheme, child) != 0) H(Op::CtSetScheme, g, H(Op::CtOwnScheme, child));
            else H(Op::CtSetScheme, g, scheme);
            Child(child, V::Paint, g);
            if (FlagSet(child, 0x2000)) PopClip(g);
            Translate(g, -Left(child), -Top(child));
        }
    }
    bool ChildInvalidated(intptr_t changed)
    {
        if (H(Op::CtInvalidState, mSelf) == 2) return false;
        H(Op::CtSetInvalidState, mSelf, 1);
        bool higherInvalid = false;
        std::list<intptr_t>::iterator i = this->mChildList.end();
        while (i != this->mChildList.begin()) {
            --i;
            intptr_t child = *i;
            if (child == changed) break;
            if (H(Op::CtIntersects, child, changed) && IsVisible(child)) {
                H(Op::ChildInvalidate, child);
                higherInvalid = true;
            }
        }
        if (ParentOf(mSelf) != 0) Parent(V::ChildInvalidated);
        return higherInvalid;
    }
    void MarkInvalid()
    {
        if (H(Op::CtInvalidState, mSelf) == 2) return;
        Base(V::MarkInvalid);
        H(Op::CtSetInvalidState, mSelf, 2);
        for (std::list<intptr_t>::iterator i = this->mChildList.begin(); i != this->mChildList.end(); ++i)
            Child(*i, V::MarkInvalid);
    }
    void ClearGrabBG()
    {
        Base(V::ClearGrabBG);
        for (std::list<intptr_t>::iterator i = this->mChildList.begin(); i != this->mChildList.end(); ++i)
            Child(*i, V::ClearGrabBG);
    }
    intptr_t GetNextSibling(intptr_t child, bool forward, bool wrap) { return H(Op::CtNextSibling, child, forward, wrap); }
    bool TabFocus(bool forward, bool init)
    {
        switch (ee_won_gui_tab_focus_start(H(Op::CtWantFocus) != 0, init, this->mIsolateTabFocus, Bits(mSelf),
                                           !this->mChildList.empty())) {
        case 0: return false;
        case 1:
            this->mHasUsedDialogKeys = true;
            H(Op::RequestFocus);
            return true;
        }
        bool wrap = this->mIsolateTabFocus;
        intptr_t child = init ? 0 : this->mFocusChild;
        if (child == 0) { intptr_t first = forward ? this->mChildList.front() : this->mChildList.back(); child = first; }
        else child = GetNextSibling(child, forward, wrap);
        unsigned long count = 0;
        while (child != 0) {
            if (Child(child, V::TabFocus, forward, true)) {
                this->mHasUsedDialogKeys = true;
                return true;
            }
            child = GetNextSibling(child, forward, wrap);
            if (ee_won_gui_tab_focus_done(count++, this->mChildList.size())) break;
        }
        return false;
    }
    bool KeyDown(int key)
    {
        if (this->mFocusChild != 0 && Child(this->mFocusChild, V::KeyDown, key)) return true;
        if (key != 9) return false;
        if (this->mChildList.empty()) return false;
        bool forward = (H(Op::KeyMod) & 1) == 0;
        return Virt(V::TabFocus, forward, false) != 0;
    }
    bool CanClose()
    {
        for (std::list<intptr_t>::iterator i = this->mChildList.begin(); i != this->mChildList.end(); ++i)
            if (!Child(*i, V::CanClose)) return false;
        return true;
    }
    void GotFocus() { Base(V::GotFocus); if (this->mFocusChild != 0) Child(this->mFocusChild, V::GotFocus); }
    void LostFocus() { Base(V::LostFocus); if (this->mFocusChild != 0) Child(this->mFocusChild, V::LostFocus); }
    bool KeyChar(int key) { return this->mFocusChild != 0 ? Child(this->mFocusChild, V::KeyChar, key) != 0 : false; }
    bool KeyUp(int key)
    {
        if (this->mFocusChild != 0) {
            Child(this->mFocusChild, V::KeyUp, key);
            return true;
        }
        return false;
    }
    void AddedToParent() { Base(V::AddedToParent); this->mHasUsedDialogKeys = false; }
    bool MouseWheel(int amount)
    {
        intptr_t child = this->mMouseChild;
        if (this->mMouseWheelGoesToFocusChild) child = this->mFocusChild;
        if (child != 0) return Child(child, V::MouseWheel, amount) != 0;
        return false;
    }
    void MouseHover(unsigned long elapsed) { Base(V::MouseHover, elapsed); if (this->mMouseChild != 0) Child(this->mMouseChild, V::MouseHover, elapsed); }
    // ControlSetupCompletion.cpp
    void MouseAt()
    {
        if (!H(Op::CtMouseInClient))
            return;
        int x, y;
        H(Op::GetMousePos, (intptr_t)&x, (intptr_t)&y);
        Virt(V::MouseMove, x, y);
    }
    // ContainerTimerCursor.cpp
    void AddTimerChild(intptr_t child)
    {
        if (H(Op::CtTimerFlags, child) != 0) {
            if (this->mChildTimerSet.insert((uintptr_t)child).second) Hold(child);
            int flags = (int)H(Op::CtTimerFlags, mSelf);
            bool requestParent = flags == 0;
            H(Op::CtSetTimerFlags, flags | 1);
            if (requestParent && ParentOf(mSelf) != 0) Parent(V::AddTimerChild);
        }
    }
    bool TimerEvent(int elapsed)
    {
        bool childNeeds = false;
        std::set<uintptr_t>::iterator i = this->mChildTimerSet.begin();
        while (i != this->mChildTimerSet.end()) {
            intptr_t child = (intptr_t)*i;
            if (H(Op::CtTimerFlags, child) != 0 && ParentOf(child) == mSelf && Child(child, V::TimerEvent, elapsed)) {
                childNeeds = true;
                ++i;
            } else {
                Child(child, V::RequestTimer, 0);
                this->mChildTimerSet.erase(i++);
                Release(child);
            }
        }
        if (!childNeeds) H(Op::CtSetTimerFlags, H(Op::CtTimerFlags, mSelf) & ~1);
        return H(Op::CtTimerFlags, mSelf) != 0;
    }
    void SetCursor(intptr_t cursor)
    {
        if (this->mMouseChild != 0 && FlagSet(this->mMouseChild, 0x40)) {
            intptr_t fallback = H(Op::CtDefaultCursor);
            if (this->mCurrentCursor != fallback) Parent(V::SetCursor, fallback);
            return;
        }
        if (cursor == 0) cursor = H(Op::CtDefaultCursor);
        if (cursor != this->mCurrentCursor) {
            SetCursorField(cursor);
            if (ParentOf(mSelf) != 0) Parent(V::SetCursor, cursor);
        }
    }
    // ContainerOrdering.cpp
    void SetFocusToLastFocusChild()
    {
        unsigned char current = this->mNextFocusId;
        bool skipCurrent = false;
        if (this->mFocusChild != 0) {
            current = (unsigned char)H(Op::CtFocusId, this->mFocusChild);
            Child(this->mFocusChild, V::LostFocus);
            this->mFocusChild = 0;
            skipCurrent = true;
        }
        std::vector<intptr_t> aChildren;
        std::vector<unsigned char> anIds;
        for (std::list<intptr_t>::iterator i = this->mChildList.begin(); i != this->mChildList.end(); ++i) {
            aChildren.push_back(*i);
            anIds.push_back((unsigned char)H(Op::CtFocusId, *i));
        }
        long long aPick = ee_won_gui_last_focus_child(current, skipCurrent, anIds.empty() ? NULL : &anIds[0], anIds.size());
        if (aPick >= 0) Child(aChildren[(size_t)aPick], V::RequestFocus);
    }
    void EnsureOnTopOf(intptr_t bottom, intptr_t top)
    {
        if (ParentOf(bottom) != mSelf || ParentOf(top) != mSelf) return;
        if (!mItr.count(bottom) || !mItr.count(top)) return; // (reset children: not driven)
        std::list<intptr_t>::iterator i = this->mItr[bottom];
        while (i != this->mItr[top] && i != this->mChildList.end()) ++i;
        if (i != this->mChildList.end()) return;
        Hold(bottom); // keep
        this->mChildList.erase(this->mItr[bottom]);
        Release(bottom);
        Hold(bottom);
        this->mItr[bottom] = this->mChildList.insert(this->mItr[top], bottom);
        Release(bottom); // keep
    }
    unsigned char GetNextFocusId()
    {
        this->mNextFocusId = ee_won_gui_next_focus_id(this->mNextFocusId);
        return this->mNextFocusId;
    }
    // ComponentTraversal.cpp's GetNextSibling, on the parent's list
    intptr_t NextChild(intptr_t child, bool forward, bool wrap)
    {
        if (!mItr.count(child)) return 0;
        std::list<intptr_t>::iterator i = this->mItr[child];
        if (forward) {
            ++i;
            if (i == this->mChildList.end()) {
                if (!wrap) return 0;
                return this->mChildList.front();
            }
        } else {
            if (i == this->mChildList.begin()) {
                if (!wrap) return 0;
                return this->mChildList.back();
            }
            --i;
        }
        return *i;
    }
    // ScrollAreaViewport::Reset's clear
    void ResetChildren()
    {
        this->mMouseChild = this->mFocusChild = 0;
        for (std::set<uintptr_t>::iterator i = mChildTimerSet.begin(); i != mChildTimerSet.end(); ++i) Release((intptr_t)*i);
        this->mChildTimerSet.clear();
        for (std::list<intptr_t>::iterator i = mChildList.begin(); i != mChildList.end(); ++i) Release(*i);
        this->mChildList.clear();
        mItr.clear();
    }
    void DropFocusChild() { if (this->mFocusChild != 0) { Child(this->mFocusChild, V::LostFocus); this->mFocusChild = 0; } }
    intptr_t GetField(int f)
    {
        switch ((EeWonCtField)f) {
        case EeWonCtField::MouseChild: return mMouseChild;
        case EeWonCtField::FocusChild: return mFocusChild;
        case EeWonCtField::CurrentCursor: return mCurrentCursor;
        case EeWonCtField::MouseWheelGoesToFocusChild: return mMouseWheelGoesToFocusChild;
        case EeWonCtField::IsolateTabFocus: return mIsolateTabFocus;
        case EeWonCtField::HasUsedDialogKeys: return mHasUsedDialogKeys;
        case EeWonCtField::OnlyChildInput: return mOnlyChildInput;
        case EeWonCtField::ModalCount: return mModalCount;
        case EeWonCtField::NextFocusId: return mNextFocusId;
        case EeWonCtField::FrontChild: return mChildList.empty() ? 0 : mChildList.front();
        case EeWonCtField::BackChild: return mChildList.empty() ? 0 : mChildList.back();
        case EeWonCtField::NumChildren: return (intptr_t)mChildList.size();
        }
        return 0;
    }
    void SetField(int f, intptr_t v)
    {
        switch ((EeWonCtField)f) {
        case EeWonCtField::MouseWheelGoesToFocusChild: mMouseWheelGoesToFocusChild = v != 0; break;
        case EeWonCtField::IsolateTabFocus: mIsolateTabFocus = v != 0; break;
        case EeWonCtField::HasUsedDialogKeys: mHasUsedDialogKeys = v != 0; break;
        case EeWonCtField::OnlyChildInput: mOnlyChildInput = v != 0; break;
        default: break;
        }
    }
    intptr_t Call(C call, intptr_t a, intptr_t b, intptr_t c)
    {
        switch (call) {
        case C::SizeChanged: Virtual08(); return 0;
        case C::DesiredSize: { int w, h; GetDesiredSize(w, h); *(int*)a = w; *(int*)b = h; return 0; }
        case C::PrePaint: PrePaint(a); return 0;
        case C::Paint: Paint(a); return 0;
        case C::EnableControlsById: EnableControlsById(a, b != 0); return 0;
        case C::NotifyRootVisibility: NotifyRootVisibility(a != 0); return 0;
        case C::HitTest: return HitTest((int)a, (int)b);
        case C::FindChildAt: return FindChildAt((int)a, (int)b);
        case C::UpdateMouseChild: UpdateMouseChild((int)a, (int)b); return 0;
        case C::MouseAt: MouseAt(); return 0;
        case C::MouseEnter: MouseEnter((int)a, (int)b); return 0;
        case C::MouseExit: MouseExit(); return 0;
        case C::MouseMove: MouseMove((int)a, (int)b); return 0;
        case C::MouseDrag: MouseDrag((int)a, (int)b); return 0;
        case C::MouseUp: MouseUp((int)a, (int)b, (int)c); return 0;
        case C::MouseDown: MouseDown((int)a, (int)b, (int)c); return 0;
        case C::MouseWheel: return MouseWheel((int)a);
        case C::MouseHover: MouseHover((unsigned long)a); return 0;
        case C::AddChild: AddChild(a); return 0;
        case C::AddChildEx: AddChildEx(a, (int)b); return 0;
        case C::HandleBroadcastEvent: HandleBroadcastEvent(a); return 0;
        case C::FindChildById: return FindChildById((int)a);
        case C::EnterModal: EnterModal(); return 0;
        case C::LeaveModal: LeaveModal(); return 0;
        case C::RemoveChild: RemoveChild(a); return 0;
        case C::RemoveAllChildren: RemoveAllChildren(); return 0;
        case C::BringToTop: BringToTop(a); return 0;
        case C::SetFocusChild: SetFocusChild(a); return 0;
        case C::ChildStateChanged: ChildStateChanged(a); return 0;
        case C::ChildInvalidated: return ChildInvalidated(a);
        case C::MarkInvalid: MarkInvalid(); return 0;
        case C::ClearGrabBG: ClearGrabBG(); return 0;
        case C::TabFocus: return TabFocus(a != 0, b != 0);
        case C::KeyDown: return KeyDown((int)a);
        case C::KeyUp: return KeyUp((int)a);
        case C::KeyChar: return KeyChar((int)a);
        case C::CanClose: return CanClose();
        case C::GotFocus: GotFocus(); return 0;
        case C::LostFocus: LostFocus(); return 0;
        case C::AddedToParent: AddedToParent(); return 0;
        case C::AddTimerChild: AddTimerChild(a); return 0;
        case C::TimerEvent: return TimerEvent((int)a);
        case C::SetCursor: SetCursor(a); return 0;
        case C::SetFocusToLastFocusChild: SetFocusToLastFocusChild(); return 0;
        case C::EnsureOnTopOf: EnsureOnTopOf(a, b); return 0;
        case C::NextFocusId: return GetNextFocusId();
        case C::NextChild: return NextChild(a, b != 0, c != 0);
        case C::ResetChildren: ResetChildren(); return 0;
        case C::DropFocusChild: DropFocusChild(); return 0;
        case C::GetField: return GetField((int)a);
        case C::SetField: SetField((int)a, b); return 0;
        }
        return 0;
    }
};
}

static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c)
{
    if (sRustSide) return ee_won_ct_call(sRust[ct], call, a, b, c);
    return sOrig[ct]->Call(call, a, b, c);
}
static intptr_t CtObjectOf(intptr_t ct) { return sRustSide ? (intptr_t)sRust[ct] : (intptr_t)sOrig[ct]; }

// ---- the driver

static const char* kCallNames[] = {"", "size", "desired", "prepaint", "paint", "enable", "rootvis", "hit", "find-at",
    "update-mouse", "mouse-at", "enter", "exit", "move", "drag", "up", "down", "wheel", "hover", "add", "add-ex",
    "broadcast", "find-id", "enter-modal", "leave-modal", "remove", "remove-all", "to-top", "set-focus", "state",
    "invalidated", "mark", "clear-bg", "tab", "key-down", "key-up", "key-char", "can-close", "got-focus", "lost-focus",
    "added", "add-timer", "timer", "cursor", "last-focus", "on-top", "focus-id", "next-child", "reset", "drop-focus",
    "get", "set"};
static const int kCallCount = (int)C::SetField;

static void RandomCall()
{
    int ct = 1 + (int)DR(kCt);
    static const int kWeights[kCallCount + 1] = {0,
        2, 1, 2, 3, 1, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2, 3, 2, 2, 6, 3,
        1, 2, 2, 2, 4, 1, 3, 4, 3, 3, 2, 1, 3, 3, 2, 2, 2, 2, 2, 2,
        3, 4, 3, 2, 2, 1, 2, 1, 1, 2, 2};
    int total = 0;
    for (int i = 1; i <= kCallCount; i++) total += kWeights[i];
    int r0 = (int)DR(total), k = 1;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    C call = (C)k;
    intptr_t a = 0, b = 0, c = 0;
    int x = (int)DR(260) - 30, y = (int)DR(260) - 30;
    switch (call) {
    case C::PrePaint: case C::Paint: a = kGraphics; break;
    case C::EnableControlsById: a = kIds; b = DR(2); break;
    case C::NotifyRootVisibility: a = DR(2); break;
    case C::HitTest: case C::FindChildAt: case C::UpdateMouseChild: case C::MouseEnter: case C::MouseMove:
    case C::MouseDrag: a = x; b = y; break;
    case C::MouseUp: case C::MouseDown: a = x; b = y; c = DR(3); break;
    case C::MouseWheel: a = (int)DR(7) - 3; break;
    case C::MouseHover: a = DR(8000); break;
    case C::AddChild: case C::RemoveChild: case C::BringToTop: case C::ChildStateChanged: case C::ChildInvalidated:
    case C::AddTimerChild: {
        // a component that is not this container nor one of its ancestors (no cycle)
        a = AnyComp();
        for (intptr_t p = ct; p != 0; p = sW[p].parent)
            if (p == a) { a = 5 + DR(kN - 4); break; }
        break;
    }
    case C::AddChildEx: {
        a = AnyComp();
        for (intptr_t p = ct; p != 0; p = sW[p].parent)
            if (p == a) { a = 5 + DR(kN - 4); break; }
        b = (int)DR(6) - 1;
        break;
    }
    case C::SetFocusChild: {
        // one of its children (the focus child is always a child: a focus cycle would never end)
        intptr_t n = SideCall(ct, C::GetField, (intptr_t)EeWonCtField::NumChildren, 0, 0);
        if (n == 0) { call = C::GotFocus; break; }
        int k = (int)DR((unsigned)n);
        a = SideCall(ct, C::GetField, (intptr_t)EeWonCtField::FrontChild, 0, 0);
        while (k-- > 0) a = SideCall(ct, C::NextChild, a, 1, 0);
        break;
    }
    case C::HandleBroadcastEvent: a = kEvent; break;
    case C::FindChildById: a = DR(8); break;
    case C::TabFocus: a = DR(2); b = DR(2); break;
    case C::KeyDown: a = DR(2) ? 9 : 40; break;
    case C::KeyUp: case C::KeyChar: a = 65 + DR(3); break;
    case C::TimerEvent: a = DR(100); break;
    case C::SetCursor: a = DR(3) ? 0x5000 + DR(4) : 0; break;
    case C::EnsureOnTopOf: a = AnyComp(); b = AnyComp(); break;
    case C::NextChild: a = AnyComp(); b = DR(2); c = DR(2); break;
    case C::GetField: a = 1 + DR(12); break;
    case C::SetField: a = 4 + DR(4); b = DR(2); break;
    case C::RemoveAllChildren: case C::ResetChildren: if (DR(3)) call = C::LeaveModal; break;
    default: break;
    }
    TopCall(ct, call, a, b, c);
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sCursorRefs.clear();
    sRustSide = inRust;
    sDriverSeed = inSeq * 40503u + 13u;
    sAnswerSeed = inSeq * 2654435761u + 7u;
    InitWorld();
    for (int ct = 1; ct <= kCt; ct++) {
        Line("new %lld", ct);
        if (inRust) sRust[ct] = ee_won_ct_new((void*)(intptr_t)ct, Host, false);
        else sOrig[ct] = new orig::Container(ct, false);
    }
    // a tree: 2..4 under 1 or each other, the leaves anywhere
    for (int id = 2; id <= kN; id++) {
        if (DR(5) == 0) continue;
        int p = id <= kCt ? 1 + (int)DR(id - 1) : 1 + (int)DR(kCt);
        TopCall(p, C::AddChild, id, 0, 0);
    }
    for (int i = 0; i < inCalls; i++) {
        if (DR(3) == 0) RandomChange();
        RandomCall();
    }
    for (int ct = kCt; ct >= 1; ct--) {
        Line("free %lld", ct);
        if (inRust) { ee_won_ct_free(sRust[ct]); sRust[ct] = NULL; }
        else { sOrig[ct]->Destroy(); delete sOrig[ct]; sOrig[ct] = NULL; }
    }
    for (int id = 1; id <= kN; id++) CHECK(sW[id].refs == 0);
    for (std::map<intptr_t, int>::iterator i = sCursorRefs.begin(); i != sCursorRefs.end(); ++i) CHECK(i->second == 0);
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
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
            if (a[i].compare(0, 5, "call ") == 0) {
                size_t sp = a[i].find(' ', 5);
                std::string key = "call" + a[i].substr(sp, a[i].find(' ', sp + 1) - sp);
                if (aSeen.insert(key).second) aNew = true;
            }
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    (void)kCallNames;
    if (!aPrint)
        printf("won_ctobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
