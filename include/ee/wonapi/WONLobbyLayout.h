// Layout parameters and layout classes.
#ifndef EE_WONAPI_WONLOBBYLAYOUT_H
#define EE_WONAPI_WONLOBBYLAYOUT_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// PercentLayoutParam (0x40 bytes): explicit constructor, borrowed component pointers, plain copy.
class PercentLayoutParam {
public:
    PercentLayoutParam(Component *left, Component *top, Component *right, Component *bottom, int px, int py, int pw = -1, int ph = -1, int lp = -1, int tp = -1, int rp = -1, int bp = -1); // at 0x00737320
    PercentLayoutParam(Component *ref,int px,int py,int pw=-1,int ph=-1,int lp=-1,int tp=-1,int rp=-1,int bp=-1);
    PercentLayoutParam(Component *ref1,Component *ref2,int px,int py,int pw=-1,int ph=-1,int lp=-1,int tp=-1,int rp=-1,int bp=-1);
    void Init(Component*,Component*,Component*,Component*,int,int,int,int,int,int,int,int); // at 0x007372c0
    void SetPercentages(int,int,int,int); // at 0x007373f0
    void SetPadding(int,int,int,int); // at 0x00737410
    void SetBorders(Component*, Component*, Component*, Component*);
    void DoCalc(Component*, Component*);
    Component *left, *top, *right, *bottom;
    int px, py, pw, ph;
    int lp, tp, rp, bp;
    int dx, dy, dw, dh; // offsets 0x30..0x3c, copied by the factory; DoCalc does not read them
};
// VerticalFlowLayout (not identified in the original exe).
class VerticalFlowLayout : public ChildLayoutInstructionBase {
protected:
    struct ItemInfo {
        int mTopPad;
        int mLeftPad;
        Component* mComponent;
        ItemInfo(Component* component,int topPad,int leftPad)
            : mTopPad(topPad),mLeftPad(leftPad),mComponent(component) {}
    };
    Component* mTopComp;
    typedef std::list<ItemInfo> ComponentList;
    ComponentList mComponentList;
public:
    VerticalFlowLayout(Component*);
    VerticalFlowLayout* Add(Component*,int=0,int=0);
    virtual void Execute(Component*);
};
class SameSizeLayout : public ChildLayoutInstructionBase {
public:
    SameSizeLayout(bool horz=true,bool vert=true) : mHorz(horz),mVert(vert) {} // inline (as at 0x00788e56)
    SameSizeLayout(Component* first,bool horz=true,bool vert=true) : mHorz(horz),mVert(vert) { this->Add(first); } // inline (as at 0x00701964): the first insertion is part of construction
    SameSizeLayout* Add(Component*); // at 0x007377a0
    virtual void Execute(Component*);
    std::list<Component*> mComponentList; //0c
    bool mHorz, mVert; // offsets 0x18, 0x19; class size 0x1c
};
class ClearRightLayout : public ChildLayoutInstructionBase {
public:
    ClearRightLayout(Component* component,int pad=0) : mComponent(component),mPad(pad) {} //3890d8..116
    ClearRightLayout* Add(Component* component) { this->mComponentList.push_back(component); return this; }
    virtual void Execute(Component*);
    Component* mComponent; //0c borrowed
    int mPad; //10
    std::list<Component*> mComponentList; // offset 0x14; class size 0x20
};
class SpaceEvenlyLayout : public ChildLayoutInstructionBase {
public:
    SpaceEvenlyLayout(Component* box,int left=0,int right=0,bool horz=true) : mSpaceBox(box),mLeftPad(left),mRightPad(right),mIsHorz(horz) {} //3892ea..337
    SpaceEvenlyLayout* Add(Component* component) { this->mComponentList.push_back(component); return this; }
    virtual void Execute(Component*);
    Component* mSpaceBox; //0c borrowed
    int mLeftPad, mRightPad; //10,14
    bool mIsHorz; //18
    std::list<Component*> mComponentList; // offset 0x1c; class size 0x28
};
class NoOverlapLayout : public ChildLayoutInstructionBase {
public:
    NoOverlapLayout(Component* target,int type,Component* ref,int horz=0,int vert=0) : mRef(ref),mTarget(target),mType(type),mHorzPad(horz),mVertPad(vert) {} //389608..64d
    NoOverlapLayout* Add(Component* component) { this->mComponentList.push_back(component); return this; }
    virtual void Execute(Component*);
    Component *mRef, *mTarget; //0c,10 borrowed
    int mType, mHorzPad, mVertPad; //14,18,1c
    std::list<Component*> mComponentList; // offset 0x20; class size 0x2c
};
class SizeAtLeastLayout : public ChildLayoutInstructionBase {
public:
    SizeAtLeastLayout(Component* target,int horz=0,int vert=0) : mTarget(target),mHorzPad(horz),mVertPad(vert) {} //3898c3..909
    SizeAtLeastLayout* Add(Component* component) { this->mComponentList.push_back(component); return this; }
    virtual void Execute(Component*);
    Component* mTarget; //0c borrowed
    int mHorzPad, mVertPad; //10,14
    std::list<Component*> mComponentList; // offset 0x18; class size 0x24
};
class ChildLayoutInstruction : public ChildLayoutInstructionBase {
public:
    virtual void Execute(Component*);
    virtual bool UsesComponent(Component*);
    int mFlags;
    Component* mChild;
    Component* mReference;
    int mX, mY, mWidth, mHeight;
};
// Complete ChildLayoutManager20 declaration is in WONLobbyGUI.h before Container.
}
#endif
