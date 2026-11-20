#ifndef EE_WONAPI_WONLOBBYSPLITTERCONFIG_H
#define EE_WONAPI_WONLOBBYSPLITTERCONFIG_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/splitobj.h"
namespace WONAPI {
// Splitter (R6 WON round 22): its panes, bars, maximise buttons, split, drag and percentages are the Rust Splitter
// (rust/crates/ee_won_gui/src/splitobj.rs); this is the type the configs build.
class Splitter : public Container {
public:
    Splitter(); // at 0x00707060 in the original
    enum QuadrantPosition { Position_Left=1,Position_Top=2,Position_Right=4,Position_Bottom=8,
        Position_TopLeft=3,Position_TopRight=6,Position_BottomLeft=9,Position_BottomRight=12 };
    enum SplitterType { Splitter_LeftRight=5,Splitter_UpDown=10,Splitter_LeftRight_Top=0x15,
        Splitter_LeftRight_Bottom=0x25,Splitter_UpDown_Left=0x4a,Splitter_UpDown_Right=0x8a,Splitter_Center=0x10f };
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
}
#endif
