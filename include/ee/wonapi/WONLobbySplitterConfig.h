#ifndef EE_WONAPI_WONLOBBYSPLITTERCONFIG_H
#define EE_WONAPI_WONLOBBYSPLITTERCONFIG_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// Splitter: member order as at 0x00707060. This version predates the SDK's move of the doubles
// to the top.
class Splitter : public Container {
public:
    Splitter(); // at 0x00707060 (0x118 bytes)
    enum QuadrantPosition { Position_Left=1,Position_Top=2,Position_Right=4,Position_Bottom=8,
        Position_TopLeft=3,Position_TopRight=6,Position_BottomLeft=9,Position_BottomRight=12 };
    enum SplitterType { Splitter_LeftRight=5,Splitter_UpDown=10,Splitter_LeftRight_Top=0x15,
        Splitter_LeftRight_Bottom=0x25,Splitter_UpDown_Left=0x4a,Splitter_UpDown_Right=0x8a,Splitter_Center=0x10f };
    void AddQuadrant(Component*,QuadrantPosition); //307440
    void AddSplitter(Component*,SplitterType); //307670
    void AddInvisibleSplitter(SplitterType,int=10); //307760
    void AddComplexSplitter(Container*,SplitterType,Component* =NULL); //307690
    void SetMinLeftRight(int,int); // at 0x00707c30
    void SetMinTopBottom(int,int); // at 0x00707c50
    void SetPercentages(int,int); // at 0x00707c70
    void MaxQuadrant(QuadrantPosition); //307810
    void UnMaxQuadrant(); //307990
    void SetSplitterPos(int,int,bool=true); //307c90
    void RegisterMaxButton(Button*,QuadrantPosition); //307300
    bool CheckMaxButton(ComponentEvent*); //3080b0
protected:
    virtual ~Splitter(); // at 0x007071e0 (out of line)
    virtual void Virtual08(); //307de0 SizeChanged
    virtual void MouseDown(int,int,int); // at 0x00707f30 (int button argument)
    virtual void MouseUp(int,int,int);   // at 0x00707ff0
    virtual void MouseDrag(int,int); //308030 MouseDrag
    virtual void HandleComponentEvent(ComponentEvent*); // slot 0x7c override, at 0x00708140
    struct Quadrant {
        Component* mComponent; // borrowed (node +0x08); the Container owns the child
        QuadrantPosition mPosition; //node+0c
        Quadrant(Component* c,QuadrantPosition p):mComponent(c),mPosition(p) {}
    };
    typedef std::list<Quadrant> QuadrantList;
    QuadrantList mQuadrantList; // allocator +0x84, head +0x88, count +0x8c; 0x10-byte nodes
    struct SplitterParam {
        Component* mComponent; // borrowed (node +0x08); the Container owns the child
        SplitterType mType; //node+0c
        SmartPtr<Component> mDragComponent; // node +0x10, retained; 0x14-byte nodes
        SplitterParam(Component* c,SplitterType t):mComponent(c),mType(t) {}
    };
    typedef std::list<SplitterParam> SplitterList;
    SplitterList mSplitterList; //90 allocator,94 header,98 count
    typedef std::list<SmartPtr<Button> > ButtonList;
    ButtonList mButtonList; // allocator +0x9c, head +0xa0, count +0xa4; 0x0c-byte nodes
    WONRectangle mOldBounds; //a8..b7 real POD rectangle
    bool mLeftRightDrag,mUpDownDrag; //b8,b9; naturalba/bb alignment
    int mDragX,mDragY; //bc,c0
    int mMinLeft,mMinRight,mMinTop,mMinBottom; //c4,c8,cc,d0
    bool mUpDownMax,mLeftRightMax; //d4,d5; naturald6/d7 alignment
    double mLeftPercent,mTopPercent; // offsets 0xd8, 0xe0 (older member order)
    bool mSplitCoordsValid; //e8, naturale9..eb alignment
    int mSplitX,mSplitY,mSplitLeft,mSplitRight,mSplitTop,mSplitBottom; //ec..100
    int mLeftRightSplitterWidth,mUpDownSplitterHeight; //104,108
    bool mHaveMaxQuadrant; //10c, natural10d..10f alignment
    QuadrantPosition mCurMaxQuadrant; //110, natural alignment, no size claim
    void CalcQuadrants(); //307a50
    void CalcSplitterPositions(); //307ad0
    void AddSplitterPrv(Component*,SplitterType,Component*); //3074b0
};
// SplitterConfig (0x9c bytes): ContainerConfig (0x98) plus a borrowed pointer at +0x98.
class SplitterConfig : public ContainerConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0079b410
    static Component* CompFactory(); // at 0x0079b3b0
    virtual void SetComponent(Component*); // at 0x0078f960
protected:
    virtual bool Virtual24(const std::string&); // at 0x00795450
    Splitter* mSplitter; //98
    Splitter::QuadrantPosition ReadQuadrant(); //394ed0
    Splitter::SplitterType ReadSplitterType(); //3951b0
};
}
#endif
