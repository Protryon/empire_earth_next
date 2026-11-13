#ifndef EE_WONAPI_WONLOBBYSPLITTERCONFIG_H
#define EE_WONAPI_WONLOBBYSPLITTERCONFIG_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// Splitter (R6 WON round 22): its panes, bars, maximise buttons, split, drag and percentages are the Rust Splitter
// (rust/crates/ee_won/src/splitobj.rs); this is the type the configs build.
class Splitter : public Container {
public:
    Splitter(); // at 0x00707060 in the original
    enum QuadrantPosition { Position_Left=1,Position_Top=2,Position_Right=4,Position_Bottom=8,
        Position_TopLeft=3,Position_TopRight=6,Position_BottomLeft=9,Position_BottomRight=12 };
    enum SplitterType { Splitter_LeftRight=5,Splitter_UpDown=10,Splitter_LeftRight_Top=0x15,
        Splitter_LeftRight_Bottom=0x25,Splitter_UpDown_Left=0x4a,Splitter_UpDown_Right=0x8a,Splitter_Center=0x10f };
    void AddQuadrant(Component* c,QuadrantPosition p) { this->Call(EeWonSplitCall::AddQuadrant,(intptr_t)c,p); }
    void AddSplitter(Component* c,SplitterType t) { this->Call(EeWonSplitCall::AddSplitter,(intptr_t)c,t); }
    void AddInvisibleSplitter(SplitterType t,int size=10) { this->Call(EeWonSplitCall::AddInvisibleSplitter,t,size); }
    void AddComplexSplitter(Container* c,SplitterType t,Component* drag=NULL) { this->Call(EeWonSplitCall::AddComplexSplitter,(intptr_t)static_cast<Component*>(c),t,(intptr_t)drag); }
    void SetMinLeftRight(int l,int r) { this->Call(EeWonSplitCall::SetMinLeftRight,l,r); }
    void SetMinTopBottom(int t,int b) { this->Call(EeWonSplitCall::SetMinTopBottom,t,b); }
    void SetPercentages(int l,int t) { this->Call(EeWonSplitCall::SetPercentages,l,t); }
    void MaxQuadrant(QuadrantPosition p) { this->Call(EeWonSplitCall::MaxQuadrant,p); }
    void UnMaxQuadrant() { this->Call(EeWonSplitCall::UnMaxQuadrant); }
    void SetSplitterPos(int x,int y,bool remember=true) { this->Call(EeWonSplitCall::SetSplitterPos,x,y,remember); }
    void RegisterMaxButton(Button* b,QuadrantPosition p) { this->Call(EeWonSplitCall::RegisterMaxButton,(intptr_t)b,p); }
    bool CheckMaxButton(ComponentEvent* e) { return this->Call(EeWonSplitCall::CheckMaxButton,(intptr_t)e)!=0; }
    EeWonSplitter* mRust;
protected:
    virtual ~Splitter();
    virtual void Virtual08() { this->Call(EeWonSplitCall::SizeChanged); } // SizeChanged
    virtual void MouseDown(int x,int y,int b) { this->Call(EeWonSplitCall::MouseDown,x,y,b); }
    virtual void MouseUp(int x,int y,int b) { this->Call(EeWonSplitCall::MouseUp,x,y,b); }
    virtual void MouseDrag(int x,int y) { this->Call(EeWonSplitCall::MouseDrag,x,y); }
    virtual void HandleComponentEvent(ComponentEvent* e) { this->Call(EeWonSplitCall::HandleEvent,(intptr_t)e); }
    friend intptr_t SplitterHostCall(Splitter*,EeWonGuiOp,intptr_t,intptr_t,intptr_t);
private:
    intptr_t Call(EeWonSplitCall c,intptr_t a=0,intptr_t b=0,intptr_t cc=0) { return ee_won_split_call(this->mRust,c,a,b,cc); }
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
