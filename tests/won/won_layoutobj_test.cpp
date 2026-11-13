// The child layouts as whole objects (R6 WON round 31): the original ChildLayoutManager and the eight instruction
// classes (ChildLayouts.cpp, ChildLayoutsAdvanced.cpp, CenterLayoutExecute.cpp, CenterLayoutStatic.cpp, the
// header's inline constructors and Add, Container::AddChildLayout and Fn2F6140's instruction), the statements copied
// from the C++ before the port with every call on a component (its bounds, its parent, SetPos, SetPosSize, SetSize,
// SetSizeAtLeast) and every reference (the manager's SmartPtrs of instructions and layout points) replaced by the
// world below, against ee_won's Rust objects (layoutobj.rs) over the same world through the host ops.
//
// The world: component 1 is the container whose manager runs, 2..9 its children, 10 a child of 2 (so a reference is
// sometimes the child's parent and sometimes not), 11..14 layout points. A size change of the container runs its
// manager again (Container::SizeChanged's ExecuteInstructions), one level deep. Every move and reference is traced
// with its arguments; the reads are answered from the world (not traced).
//
// Each of 20,000 sequences makes 40 random calls: new instructions of every class with random numbers and lists,
// added to the manager in random groups (Container::AddChildLayout), run directly or asked UsesComponent; the
// static Center; the manager's ExecuteInstructions, RemoveComponent, Clear, EnableInstructions and AddLayoutPoint;
// changes of the world. Then the manager is destroyed and every reference must have come back. The traces must be
// the same. The first 40 sequences and every later one that makes an op or a call none before made are
// rust/crates/ee_won/src/golden_layoutobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

static const int kN = 14;
static const int kContainer = 1;
struct Box { int x, y, w, h, parent, refs; };
static Box sW[kN + 1];
static std::vector<std::string> sTrace;
static std::set<std::string> sMet;
static int sDepth;
static void (*sRelayout)();
static int sLiveInstr;

static void Trace(const char* inFormat, int a = 0, int b = 0, int c = 0, int d = 0, int e = 0)
{
    char aLine[160];
    snprintf(aLine, sizeof(aLine), inFormat, a, b, c, d, e);
    sTrace.push_back(aLine);
    std::string aKey(aLine);
    sMet.insert(aKey.substr(0, aKey.find(' ')));
}

// ---- the world's moves (both sides)
static void WSizeChanged(int c, int w, int h)
{
    bool aChanged = sW[c].w != w || sW[c].h != h;
    sW[c].w = w;
    sW[c].h = h;
    if (aChanged && c == kContainer && sDepth < 1) {
        Trace("relayout");
        sDepth++;
        sRelayout();
        sDepth--;
    }
}
static void WSetPos(int c, int x, int y) { Trace("pos %d %d %d", c, x, y); sW[c].x = x; sW[c].y = y; }
static void WSetSize(int c, int w, int h) { Trace("size %d %d %d", c, w, h); WSizeChanged(c, w, h); }
static void WSetPosSize(int c, int x, int y, int w, int h)
{
    Trace("possize %d %d %d %d %d", c, x, y, w, h);
    sW[c].x = x;
    sW[c].y = y;
    WSizeChanged(c, w, h);
}
static void WSetSizeAtLeast(int c, int w, int h)
{
    Trace("atleast %d %d %d", c, w, h);
    WSizeChanged(c, w > sW[c].w ? w : sW[c].w, h > sW[c].h ? h : sW[c].h);
}
static void WHoldPoint(int p) { Trace("phold %d", p); sW[p].refs++; }
static void WReleasePoint(int p) { Trace("prelease %d", p); sW[p].refs--; }

// ---- the original
namespace orig {
struct Component {
    int id;
    Component* Parent();
    int Left() { return sW[id].x; }
    int Top() { return sW[id].y; }
    int Width() { return sW[id].w; }
    int Height() { return sW[id].h; }
    int Right() { return sW[id].x + sW[id].w; }
    struct Bounds { int x, y, width, height; } mB;
    const Bounds& GetBounds() { mB.x = sW[id].x; mB.y = sW[id].y; mB.width = sW[id].w; mB.height = sW[id].h; return mB; }
    void SetPos(int x, int y) { WSetPos(id, x, y); }
    void SetSize(int w, int h) { WSetSize(id, w, h); }
    void SetPosSize(int x, int y, int w, int h) { WSetPosSize(id, x, y, w, h); }
    void SetSizeAtLeast(int w, int h) { WSetSizeAtLeast(id, w, h); }
    void AddRef() { WHoldPoint(id); }
    void Release() { WReleasePoint(id); }
};
static Component sComp[kN + 1];
static Component* C(int c) { return c ? &sComp[c] : NULL; }
Component* Component::Parent() { return C(sW[id].parent); }
typedef Component LayoutPoint;

struct RefCount {
    int mId;
    int mRefs;
    RefCount() : mId(0), mRefs(0) {}
    virtual ~RefCount() {}
    // (id 0: Center's temporary, which the Rust makes without a C++ object: not traced)
    void AddRef() { mRefs++; if (mId) Trace("hold %d", mId); }
    void Release()
    {
        mRefs--;
        if (mId) Trace("release %d", mId);
        if (mRefs == 0) { if (mId) { Trace("delete %d", mId); sLiveInstr--; } delete this; }
    }
};
template <class T> struct SmartPtr {
    T* p;
    SmartPtr(T* q = NULL) : p(q) { if (p) p->AddRef(); }
    SmartPtr(const SmartPtr& o) : p(o.p) { if (p) p->AddRef(); }
    ~SmartPtr() { if (p) p->Release(); }
    SmartPtr& operator=(const SmartPtr& o) { if (o.p) o.p->AddRef(); if (p) p->Release(); p = o.p; return *this; }
    T* operator->() const { return p; }
    T* get() const { return p; }
};

// the header's classes
struct ChildLayoutInstructionBase : public RefCount {
    ChildLayoutInstructionBase() : mGroupId(0), mEnabled(true) {}
    virtual void Execute(Component*) {}
    virtual bool UsesComponent(Component*) { return false; }
    unsigned char mGroupId;
    bool mEnabled;
};
struct ChildLayoutInstruction : public ChildLayoutInstructionBase {
    virtual void Execute(Component*);
    virtual bool UsesComponent(Component*);
    int mFlags;
    Component* mChild;
    Component* mReference;
    int mX, mY, mWidth, mHeight;
};
struct CenterLayout : public ChildLayoutInstructionBase {
    CenterLayout(Component* box, bool horz, bool vert) : mHorz(horz), mVert(vert), mCenterBox(box) {}
    CenterLayout(Component* box, Component* first, bool horz, bool vert) : mHorz(horz), mVert(vert), mCenterBox(box) { this->Add(first); }
    CenterLayout* Add(Component* component) { this->mComponentList.push_back(component); return this; }
    static void Center(Component* inParent, Component* inChild, bool inHorz, bool inVert);
    virtual void Execute(Component*);
    bool mHorz;
    bool mVert;
    Component* mCenterBox;
    std::list<Component*> mComponentList;
};
struct SameSizeLayout : public ChildLayoutInstructionBase {
    SameSizeLayout(bool horz = true, bool vert = true) : mHorz(horz), mVert(vert) {}
    SameSizeLayout* Add(Component* c) { this->mComponentList.push_back(c); return this; }
    virtual void Execute(Component*);
    std::list<Component*> mComponentList;
    bool mHorz, mVert;
};
struct ClearRightLayout : public ChildLayoutInstructionBase {
    ClearRightLayout(Component* component, int pad = 0) : mComponent(component), mPad(pad) {}
    ClearRightLayout* Add(Component* component) { this->mComponentList.push_back(component); return this; }
    virtual void Execute(Component*);
    Component* mComponent;
    int mPad;
    std::list<Component*> mComponentList;
};
struct SpaceEvenlyLayout : public ChildLayoutInstructionBase {
    SpaceEvenlyLayout(Component* box, int left = 0, int right = 0, bool horz = true) : mSpaceBox(box), mLeftPad(left), mRightPad(right), mIsHorz(horz) {}
    SpaceEvenlyLayout* Add(Component* component) { this->mComponentList.push_back(component); return this; }
    virtual void Execute(Component*);
    Component* mSpaceBox;
    int mLeftPad, mRightPad;
    bool mIsHorz;
    std::list<Component*> mComponentList;
};
struct NoOverlapLayout : public ChildLayoutInstructionBase {
    NoOverlapLayout(Component* target, int type, Component* ref, int horz = 0, int vert = 0) : mRef(ref), mTarget(target), mType(type), mHorzPad(horz), mVertPad(vert) {}
    NoOverlapLayout* Add(Component* component) { this->mComponentList.push_back(component); return this; }
    virtual void Execute(Component*);
    Component *mRef, *mTarget;
    int mType, mHorzPad, mVertPad;
    std::list<Component*> mComponentList;
};
struct SizeAtLeastLayout : public ChildLayoutInstructionBase {
    SizeAtLeastLayout(Component* target, int horz = 0, int vert = 0) : mTarget(target), mHorzPad(horz), mVertPad(vert) {}
    SizeAtLeastLayout* Add(Component* component) { this->mComponentList.push_back(component); return this; }
    virtual void Execute(Component*);
    Component* mTarget;
    int mHorzPad, mVertPad;
    std::list<Component*> mComponentList;
};
struct PercentLayoutParam {
    PercentLayoutParam(Component* left, Component* top, Component* right, Component* bottom, int px, int py, int pw, int ph, int lp, int tp, int rp, int bp)
    { this->Init(left, top, right, bottom, px, py, pw, ph, lp, tp, rp, bp); }
    void Init(Component* left, Component* top, Component* right, Component* bottom, int px, int py, int pw, int ph, int lp, int tp, int rp, int bp)
    {
        this->SetBorders(left, top, right, bottom);
        this->px = px; this->py = py; this->pw = pw; this->ph = ph;
        this->lp = lp; this->tp = tp; this->rp = rp; this->bp = bp;
    }
    void SetBorders(Component*, Component*, Component*, Component*);
    void DoCalc(Component*, Component*);
    Component *left, *top, *right, *bottom;
    int px, py, pw, ph;
    int lp, tp, rp, bp;
};
struct PercentLayout : public ChildLayoutInstructionBase {
    PercentLayout(Component* target, const PercentLayoutParam& param) : mTarget(target), mParam(param) {}
    virtual void Execute(Component*);
    Component* mTarget;
    PercentLayoutParam mParam;
};
struct ChildLayoutManager {
    typedef std::list<SmartPtr<ChildLayoutInstructionBase> > InstructionList;
    void ExecuteInstructions(Component*);
    void AddInstruction(ChildLayoutInstructionBase*);
    void AddLayoutPoint(LayoutPoint*);
    void RemoveComponent(Component*);
    void Clear();
    void EnableInstructions(int, bool);
    InstructionList mInstructions;
    std::list<SmartPtr<LayoutPoint> > mLayoutPoints;
};
// Container::AddChildLayout (ChildLayoutRegistration.cpp), on the manager
static void AddChildLayout(ChildLayoutManager* mgr, Component* t, ChildLayoutInstructionBase* theInstr, int theId)
{
    theInstr->mGroupId = theId;
    if(theId == 0) {
        theInstr->mEnabled = true;
        theInstr->Execute(t);
    }
    else
        theInstr->mEnabled = false;
    mgr->AddInstruction(theInstr);
}

// ChildLayouts.cpp
void ChildLayoutInstruction::Execute(Component*)
{
    Component* child = this->mChild;
    Component* reference = this->mReference;
    EeWonRect aChild = { child->Left(), child->Top(), child->Width(), child->Height() };
    EeWonRect aRef = { reference->Left(), reference->Top(), reference->Width(), reference->Height() };
    EeWonChildLayout aLayout = { this->mFlags, this->mX, this->mY, this->mWidth, this->mHeight };
    EeWonRect r = ee_won_gui_child_layout(aChild, aRef, reference == child->Parent(), aLayout);
    child->SetPosSize(r.x, r.y, r.width, r.height);
}
bool ChildLayoutInstruction::UsesComponent(Component* component)
{
    return this->mChild == component || this->mReference == component;
}
void ChildLayoutManager::ExecuteInstructions(Component* container)
{
    for(InstructionList::iterator itr = this->mInstructions.begin(); itr != this->mInstructions.end(); ++itr)
        if((*itr)->mEnabled)
            (*itr)->Execute(container);
}
void ChildLayoutManager::AddInstruction(ChildLayoutInstructionBase* instruction)
{
    this->mInstructions.emplace_back(instruction); // (push_back's temporary SmartPtr would add a hold and a release)
}
void ChildLayoutManager::AddLayoutPoint(LayoutPoint* point)
{
    this->mLayoutPoints.emplace_back(point); // (the same)
}
void ChildLayoutManager::RemoveComponent(Component* component)
{
    InstructionList::iterator itr = this->mInstructions.begin();
    while(itr != this->mInstructions.end())
    {
        if((*itr)->UsesComponent(component))
            this->mInstructions.erase(itr++);
        else
            ++itr;
    }
}
void ChildLayoutManager::Clear()
{
    this->mInstructions.clear();
    this->mLayoutPoints.clear();
}
void ChildLayoutManager::EnableInstructions(int group, bool enabled)
{
    for(InstructionList::iterator itr = this->mInstructions.begin(); itr != this->mInstructions.end(); ++itr)
        if((*itr)->mGroupId == group)
            (*itr)->mEnabled = enabled;
}

// CenterLayoutExecute.cpp, CenterLayoutStatic.cpp
void CenterLayout::Execute(Component* parent)
{
    if(this->mComponentList.empty())
        return;
    std::list<Component*>::iterator it;
    std::vector<EeWonRect> aBounds;
    for(it=this->mComponentList.begin();it!=this->mComponentList.end();++it)
    {
        const Component::Bounds& b=(*it)->GetBounds();
        EeWonRect r = { b.x, b.y, b.width, b.height };
        aBounds.push_back(r);
    }
    bool aBox = this->mCenterBox!=NULL && this->mCenterBox!=parent;
    Component* center = aBox ? this->mCenterBox : parent;
    EeWonRect aCenter = { center->Left(), center->Top(), center->Width(), center->Height() };
    int dx=0,dy=0;
    if(!ee_won_gui_center_shift(&aBounds[0],aBounds.size(),aCenter,aBox,this->mHorz,this->mVert,&dx,&dy))
        return;
    for(it=this->mComponentList.begin();it!=this->mComponentList.end();++it)
    {
        Component* child=*it;
        child->SetPos(child->Left()+dx,child->Top()+dy);
    }
}
void CenterLayout::Center(Component* parent,Component* child,bool horz,bool vert)
{
    SmartPtr<CenterLayout> layout=new CenterLayout(parent,child,horz,vert);
    layout->Execute(parent);
}

// ChildLayoutsAdvanced.cpp
void PercentLayoutParam::SetBorders(Component* inLeft, Component* inTop, Component* inRight, Component* inBottom)
{
    Component* in[4] = { inLeft, inTop, inRight, inBottom };
    int anIndex[4];
    ee_won_gui_percent_borders((inLeft!=NULL?1u:0u)|(inTop!=NULL?2u:0u)|(inRight!=NULL?4u:0u)|(inBottom!=NULL?8u:0u),
                               anIndex);
    this->left=anIndex[0]<0?NULL:in[anIndex[0]];
    this->top=anIndex[1]<0?NULL:in[anIndex[1]];
    this->right=anIndex[2]<0?NULL:in[anIndex[2]];
    this->bottom=anIndex[3]<0?NULL:in[anIndex[3]];
}
void PercentLayoutParam::DoCalc(Component* target, Component* parent)
{
    int anEdges[4] = { 0, 0, 0, 0 };
    if(this->px>=0 || this->pw>=0)
    {
        anEdges[0]=this->left==parent?0:this->left->Left();
        anEdges[1]=this->right==parent?parent->Width():this->right->Left()+this->right->Width();
    }
    if(this->py>=0 || this->ph>=0)
    {
        anEdges[2]=this->top==parent?0:this->top->Top();
        anEdges[3]=this->bottom==parent?parent->Height():this->bottom->Top()+this->bottom->Height();
    }
    EeWonRect aTarget = { target->Left(), target->Top(), target->Width(), target->Height() };
    EeWonPercent p = { this->px, this->py, this->pw, this->ph, this->lp, this->tp, this->rp, this->bp };
    EeWonRect r = ee_won_gui_percent_layout(aTarget, anEdges, p);
    target->SetPosSize(r.x,r.y,r.width,r.height);
}
void SameSizeLayout::Execute(Component*)
{
    std::vector<int> aSizes;
    std::list<Component*>::iterator i;
    for(i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        aSizes.push_back((*i)->Width());
        aSizes.push_back((*i)->Height());
    }
    int width=0,height=0;
    ee_won_gui_max_size(0,aSizes.empty()?NULL:&aSizes[0],this->mComponentList.size(),this->mHorz,this->mVert,0,0,
                        &width,&height);
    for(i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        Component* child=*i;
        if(this->mHorz && this->mVert) child->SetSize(width,height);
        else if(this->mHorz) child->SetSize(width,child->Height());
        else if(this->mVert) child->SetSize(child->Width(),height);
    }
}
void ClearRightLayout::Execute(Component*)
{
    std::vector<int> aRights;
    for(std::list<Component*>::iterator i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
        aRights.push_back((*i)->Right());
    this->mComponent->SetPos(ee_won_gui_clear_right(aRights.empty()?NULL:&aRights[0],aRights.size(),this->mPad),
                             this->mComponent->Top());
}
void SpaceEvenlyLayout::Execute(Component* parent)
{
    if(this->mComponentList.empty()) return;
    std::vector<int> anExtents;
    std::list<Component*>::iterator i;
    for(i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
        anExtents.push_back(this->mIsHorz?(*i)->Width():(*i)->Height());
    Component* box=this->mSpaceBox;
    std::vector<int> aPositions(anExtents.size());
    ee_won_gui_space_evenly(&anExtents[0],anExtents.size(),this->mIsHorz?box->Left():box->Top(),
                            this->mIsHorz?box->Width():box->Height(),box==parent,this->mLeftPad,this->mRightPad,
                            &aPositions[0]);
    size_t n=0;
    for(i=this->mComponentList.begin();i!=this->mComponentList.end();++i,++n)
    {
        Component* child=*i;
        if(this->mIsHorz) child->SetPos(aPositions[n],child->Top());
        else child->SetPos(child->Left(),aPositions[n]);
    }
}
void NoOverlapLayout::Execute(Component* parent)
{
    const Component::Bounds& b=this->mTarget->GetBounds();
    EeWonRect aTarget = { b.x, b.y, b.width, b.height };
    EeWonRect aRef = { this->mRef->Left(), this->mRef->Top(), this->mRef->Width(), this->mRef->Height() };
    int dx=0,dy=0;
    if(!ee_won_gui_no_overlap(aTarget,aRef,this->mRef==parent,this->mType,this->mHorzPad,this->mVertPad,&dx,&dy))
        return;
    this->mTarget->SetPos(this->mTarget->Left()+dx,this->mTarget->Top()+dy);
    for(std::list<Component*>::iterator i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        Component* child=*i;
        child->SetPos(child->Left()+dx,child->Top()+dy);
    }
}
void SizeAtLeastLayout::Execute(Component*)
{
    std::vector<int> aSizes;
    for(std::list<Component*>::iterator i=this->mComponentList.begin();i!=this->mComponentList.end();++i)
    {
        aSizes.push_back((*i)->Width());
        aSizes.push_back((*i)->Height());
    }
    int width=0,height=0;
    ee_won_gui_max_size(1,aSizes.empty()?NULL:&aSizes[0],this->mComponentList.size(),false,false,this->mHorzPad,
                        this->mVertPad,&width,&height);
    this->mTarget->SetSizeAtLeast(width,height);
}
void PercentLayout::Execute(Component* theParent)
{
    this->mParam.DoCalc(this->mTarget,theParent);
}

static ChildLayoutManager* sMgr;
static void Relayout() { sMgr->ExecuteInstructions(C(kContainer)); }
} // namespace orig

// ---- the Rust side: an instruction shell (the C++ class's reference count) over its Rust object
struct RInstr {
    int id;
    int refs;
    EeWonLayout* lay;
};
static EeWonLayoutMgr* sRMgr;
static void RHold(RInstr* t) { t->refs++; Trace("hold %d", t->id); }
static void RRelease(RInstr* t)
{
    t->refs--;
    Trace("release %d", t->id);
    if (t->refs == 0) { Trace("delete %d", t->id); sLiveInstr--; ee_won_layout_free(t->lay); delete t; }
}
static intptr_t Host(void*, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t)
{
    int k = (int)a;
    switch (op) {
    case Op::ChildLeft: return sW[k].x;
    case Op::ChildTop: return sW[k].y;
    case Op::ChildWidth: return sW[k].w;
    case Op::ChildHeight: return sW[k].h;
    case Op::LayParentOf: return sW[k].parent;
    case Op::ChildSetPos: WSetPos(k, (int)b, (int)c); return 0;
    case Op::ChildSetSize: WSetSize(k, (int)b, (int)c); return 0;
    case Op::ChildSetPosSize: {
        const EeWonRect4* r = (const EeWonRect4*)b;
        WSetPosSize(k, r->x, r->y, r->w, r->h);
        return 0;
    }
    case Op::LaySetSizeAtLeast: WSetSizeAtLeast(k, (int)b, (int)c); return 0;
    case Op::LayHold: RHold((RInstr*)a); return 0;
    case Op::LayRelease: RRelease((RInstr*)a); return 0;
    case Op::ChildHold: WHoldPoint(k); return 0;
    case Op::ChildRelease: WReleasePoint(k); return 0;
    default:
        Trace("unexpected-op %d", (int)op);
        return 0;
    }
}
static void RRelayout() { ee_won_lmgr_call(sRMgr, EeWonLayoutMgrCall::Execute, kContainer, 0, 0, 0); }

// ---- the driver
static unsigned sDriverSeed;
static unsigned D() { sDriverSeed = sDriverSeed * 1103515245u + 12345u; return (sDriverSeed >> 16) & 0x7fff; }
static int DR(int n) { return n ? (int)(D() % (unsigned)n) : 0; }
static int Num() { int k = DR(10); if (k < 2) return DR(3) - 1; if (k < 4) return DR(200) - 100; return DR(60) - 10; }
static int AnyComp() { return 1 + DR(10); } // 1..10: the container or a child
static int ChildFlags()
{
    static const int kFlags[] = { 0x1, 0x2, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
                                  0x10000, 0x20000, 0x40000, 0x80000, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80 };
    int f = 0, n = 1 + DR(3);
    for (int i = 0; i < n; i++) f |= kFlags[DR(20)];
    return f;
}
static int sNextId;

// a new instruction: its class and numbers drawn the same on both sides, then its list, then what is done with it
struct NewSpec {
    int kind; // EeWonLayoutKind
    int refs[5];
    int ints[8];
    bool horz, vert;
    std::vector<int> list;
    int use; // 0 add to the manager, 1 execute directly, 2 UsesComponent
    int group, parent, asked;
};
static NewSpec DrawSpec()
{
    NewSpec s;
    memset(s.refs, 0, sizeof(s.refs));
    memset(s.ints, 0, sizeof(s.ints));
    s.kind = 1 + DR(8);
    s.horz = DR(2) != 0;
    s.vert = DR(2) != 0;
    switch (s.kind) {
    case 1: s.refs[0] = 2 + DR(9); s.refs[1] = AnyComp(); s.ints[0] = ChildFlags(); for (int i = 1; i < 5; i++) s.ints[i] = Num(); break;
    case 2: s.refs[0] = DR(4) == 0 ? 0 : AnyComp(); break;
    case 3: break;
    case 4: s.refs[0] = 2 + DR(9); s.ints[0] = Num(); break;
    case 5: s.refs[0] = AnyComp(); s.ints[0] = Num(); s.ints[1] = Num(); break;
    case 6: {
        s.refs[0] = AnyComp();
        s.refs[1] = 2 + DR(9);
        static const int kH[] = { 0, 0x800, 0x400, 0xc00 }, kV[] = { 0, 0x100, 0x200, 0x300 };
        s.ints[0] = kH[DR(4)] | kV[DR(4)];
        s.ints[1] = Num();
        s.ints[2] = Num();
        break;
    }
    case 7: s.refs[0] = AnyComp(); s.ints[0] = Num(); s.ints[1] = Num(); break;
    case 8: {
        s.refs[0] = 2 + DR(9);
        int given = 1 + DR(15);
        for (int i = 0; i < 4; i++) s.refs[1 + i] = (given & (1 << i)) ? AnyComp() : 0;
        for (int i = 0; i < 4; i++) s.ints[i] = DR(3) == 0 ? -1 : DR(101);
        for (int i = 4; i < 8; i++) s.ints[i] = DR(3) == 0 ? -1 : DR(12);
        break;
    }
    }
    if (s.kind >= 2 && s.kind <= 7) {
        int n = (s.kind == 3 || s.kind == 5) ? 1 + DR(4) : DR(5);
        for (int i = 0; i < n; i++) s.list.push_back(2 + DR(9));
    }
    s.use = DR(10) < 7 ? 0 : (DR(2) ? 1 : 2);
    static const int kGroups[] = { 0, 0, 0, 1, 2, 256 };
    s.group = kGroups[DR(6)];
    s.parent = DR(4) == 0 ? AnyComp() : kContainer;
    s.asked = AnyComp();
    return s;
}

static orig::ChildLayoutInstructionBase* OrigNew(const NewSpec& s)
{
    using namespace orig;
    ChildLayoutInstructionBase* r = NULL;
    switch (s.kind) {
    case 1: {
        ChildLayoutInstruction* i = new ChildLayoutInstruction;
        i->mFlags = s.ints[0]; i->mChild = C(s.refs[0]); i->mReference = C(s.refs[1]);
        i->mX = s.ints[1]; i->mY = s.ints[2]; i->mWidth = s.ints[3]; i->mHeight = s.ints[4];
        r = i;
        break;
    }
    case 2: { CenterLayout* l = new CenterLayout(C(s.refs[0]), s.horz, s.vert); for (size_t k = 0; k < s.list.size(); k++) l->Add(C(s.list[k])); r = l; break; }
    case 3: { SameSizeLayout* l = new SameSizeLayout(s.horz, s.vert); for (size_t k = 0; k < s.list.size(); k++) l->Add(C(s.list[k])); r = l; break; }
    case 4: { ClearRightLayout* l = new ClearRightLayout(C(s.refs[0]), s.ints[0]); for (size_t k = 0; k < s.list.size(); k++) l->Add(C(s.list[k])); r = l; break; }
    case 5: { SpaceEvenlyLayout* l = new SpaceEvenlyLayout(C(s.refs[0]), s.ints[0], s.ints[1], s.horz); for (size_t k = 0; k < s.list.size(); k++) l->Add(C(s.list[k])); r = l; break; }
    case 6: { NoOverlapLayout* l = new NoOverlapLayout(C(s.refs[1]), s.ints[0], C(s.refs[0]), s.ints[1], s.ints[2]); for (size_t k = 0; k < s.list.size(); k++) l->Add(C(s.list[k])); r = l; break; }
    case 7: { SizeAtLeastLayout* l = new SizeAtLeastLayout(C(s.refs[0]), s.ints[0], s.ints[1]); for (size_t k = 0; k < s.list.size(); k++) l->Add(C(s.list[k])); r = l; break; }
    case 8: {
        PercentLayoutParam p(C(s.refs[1]), C(s.refs[2]), C(s.refs[3]), C(s.refs[4]), s.ints[0], s.ints[1], s.ints[2], s.ints[3], s.ints[4], s.ints[5], s.ints[6], s.ints[7]);
        r = new PercentLayout(C(s.refs[0]), p);
        break;
    }
    }
    r->mId = ++sNextId;
    return r;
}
static RInstr* RustNew(const NewSpec& s)
{
    EeWonLayoutInit init;
    memset(&init, 0, sizeof(init));
    init.kind = (EeWonLayoutKind)s.kind;
    for (int i = 0; i < 5; i++) init.refs[i] = s.refs[i];
    for (int i = 0; i < 8; i++) init.ints[i] = s.ints[i];
    init.horz = s.horz;
    init.vert = s.vert;
    if (s.kind == 8) {
        // PercentLayoutParam's SetBorders (the C++ shell's statement before the Rust object is made)
        int idx[4];
        ee_won_gui_percent_borders((s.refs[1] ? 1u : 0u) | (s.refs[2] ? 2u : 0u) | (s.refs[3] ? 4u : 0u) | (s.refs[4] ? 8u : 0u), idx);
        for (int i = 0; i < 4; i++) init.refs[1 + i] = idx[i] < 0 ? 0 : s.refs[1 + idx[i]];
    }
    RInstr* t = new RInstr;
    t->id = ++sNextId;
    t->refs = 0;
    t->lay = ee_won_layout_new(t, Host, &init);
    for (size_t k = 0; k < s.list.size(); k++) ee_won_layout_call(t->lay, EeWonLayoutCall::Add, s.list[k], 0);
    return t;
}

static void Run(bool inRust)
{
    int k = DR(100);
    if (k < 40) {
        NewSpec s = DrawSpec();
        {
            char aLine[400];
            int n = snprintf(aLine, sizeof(aLine), "call new %d %d %d %d %d %d %d", s.kind, s.use, s.group, s.parent,
                             s.asked, s.horz ? 1 : 0, s.vert ? 1 : 0);
            for (int i = 0; i < 5; i++) n += snprintf(aLine + n, sizeof(aLine) - n, " %d", s.refs[i]);
            for (int i = 0; i < 8; i++) n += snprintf(aLine + n, sizeof(aLine) - n, " %d", s.ints[i]);
            for (size_t i = 0; i < s.list.size(); i++) n += snprintf(aLine + n, sizeof(aLine) - n, " %d", s.list[i]);
            sTrace.push_back(aLine);
            sMet.insert("call");
        }
        sLiveInstr++;
        if (inRust) {
            RInstr* t = RustNew(s);
            RHold(t); // the creator's SmartPtr
            if (s.use == 0) ee_won_lmgr_call(sRMgr, EeWonLayoutMgrCall::AddChildLayout, (intptr_t)t, (intptr_t)t->lay, s.group, kContainer);
            else if (s.use == 1) ee_won_layout_call(t->lay, EeWonLayoutCall::Execute, s.parent, 0);
            else Trace("uses %d", (int)ee_won_layout_call(t->lay, EeWonLayoutCall::UsesComponent, s.asked, 0));
            RRelease(t);
        } else {
            orig::SmartPtr<orig::ChildLayoutInstructionBase> t = OrigNew(s);
            if (s.use == 0) orig::AddChildLayout(orig::sMgr, orig::C(kContainer), t.get(), s.group);
            else if (s.use == 1) t->Execute(orig::C(s.parent));
            else Trace("uses %d", t->UsesComponent(orig::C(s.asked)) ? 1 : 0);
        }
    } else if (k < 45) {
        int parent = DR(3) == 0 ? AnyComp() : kContainer, child = 2 + DR(9);
        bool h = DR(2) != 0, v = DR(2) != 0;
        Trace("call center %d %d %d", parent, child, h * 2 + v);
        if (inRust) ee_won_layout_center(Host, parent, child, h, v);
        else orig::CenterLayout::Center(orig::C(parent), orig::C(child), h, v);
    } else if (k < 60) {
        Trace("call run");
        if (inRust) RRelayout(); else orig::Relayout();
    } else if (k < 67) {
        int c = AnyComp();
        Trace("call remove %d", c);
        if (inRust) ee_won_lmgr_call(sRMgr, EeWonLayoutMgrCall::RemoveComponent, c, 0, 0, 0);
        else orig::sMgr->RemoveComponent(orig::C(c));
    } else if (k < 69) {
        Trace("call clear");
        if (inRust) ee_won_lmgr_call(sRMgr, EeWonLayoutMgrCall::Clear, 0, 0, 0, 0);
        else orig::sMgr->Clear();
    } else if (k < 77) {
        static const int kGroups[] = { 0, 1, 2, 256 };
        int g = kGroups[DR(4)], on = DR(2);
        Trace("call enable %d %d", g, on);
        if (inRust) ee_won_lmgr_call(sRMgr, EeWonLayoutMgrCall::EnableInstructions, g, on, 0, 0);
        else orig::sMgr->EnableInstructions(g, on != 0);
    } else if (k < 82) {
        int p = 11 + DR(4);
        Trace("call point %d", p);
        if (inRust) ee_won_lmgr_call(sRMgr, EeWonLayoutMgrCall::AddLayoutPoint, p, 0, 0, 0);
        else orig::sMgr->AddLayoutPoint(orig::C(p));
    } else {
        int c = 1 + DR(10);
        int x = Num(), y = Num(), w = DR(5) == 0 ? Num() : DR(300), h = DR(5) == 0 ? Num() : DR(200);
        Trace("set %d %d %d %d %d", c, x, y, w, h);
        sW[c].x = x; sW[c].y = y; sW[c].w = w; sW[c].h = h;
    }
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sSeed = inSeq * 2654435761u + 31u;
    sDriverSeed = inSeq * 40503u + 17u;
    sNextId = 0;
    sDepth = 0;
    sLiveInstr = 0;
    for (int c = 1; c <= kN; c++) {
        sW[c].x = (int)R(400) - 50;
        sW[c].y = (int)R(300) - 50;
        sW[c].w = (int)R(300);
        sW[c].h = (int)R(200);
        sW[c].parent = c == 1 ? 0 : c == 10 ? 2 : kContainer;
        sW[c].refs = 0;
        orig::sComp[c].id = c;
    }
    std::string aStart = "start";
    for (int c = 1; c <= kN; c++) {
        char aBuf[64];
        snprintf(aBuf, sizeof(aBuf), " %d %d %d %d %d", sW[c].x, sW[c].y, sW[c].w, sW[c].h, sW[c].parent);
        aStart += aBuf;
    }
    sTrace.push_back(aStart);
    if (inRust) {
        sRelayout = RRelayout;
        sRMgr = ee_won_lmgr_new((void*)0x77, Host);
        for (int i = 0; i < inCalls; i++) Run(true);
        Trace("free");
        ee_won_lmgr_free(sRMgr);
        sRMgr = NULL;
    } else {
        sRelayout = orig::Relayout;
        orig::sMgr = new orig::ChildLayoutManager;
        for (int i = 0; i < inCalls; i++) Run(false);
        Trace("free");
        delete orig::sMgr;
        orig::sMgr = NULL;
    }
    for (int c = 1; c <= kN; c++) CHECK(sW[c].refs == 0);
    CHECK(sLiveInstr == 0);
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCalls = 40;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCalls);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCalls);
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
                    if (strcmp(x, y) != 0) { printf("  line %u\n  orig: %s\n  rust: %s\n", (unsigned)i, x, y); break; }
                }
            }
        }
        bool aNew = seq < 40;
        for (std::set<std::string>::const_iterator i = sMet.begin(); i != sMet.end(); ++i)
            if (aSeen.insert(*i).second) aNew = true;
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].compare(0, 9, "call new ") == 0)
                if (aSeen.insert(a[i].substr(0, 13)).second) aNew = true;
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_layoutobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
