// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x00700a60
// Returns the contained component's size plus a 20-pixel margin and the title height (20 x 20 when empty)
void MSEtchedFrameGroup::GetDesiredSize(int& width,int& height) {
    if(this->mComponent.get()!=NULL) {
        width=this->mComponent->Width()+20;
        height=this->mComponent->Height()+this->mLabel->Height()+20;
    } else width=height=20;
}
// FUNCTION: WON 0x00700ab0
// On first joining a parent, adds the title label 5 pixels in at the top and the contained component
// below it, 10 pixels in from the frame
void MSEtchedFrameGroup::AddedToParent() {
    if(this->mLabel->mParent!=this) {
        this->AddChild(this->mLabel.get());
        this->Fn2F6140(this->mLabel.get(),0x1000,this,5,0,0,0,0);
        if(this->mComponent.get()!=NULL) {
            this->AddChild(this->mComponent.get());
            this->Fn2F6140(this->mComponent.get(),0x1001,this,10,0,-20,0,0);
            this->Fn2F6140(this->mComponent.get(),0x200,this->mLabel.get(),0,10,0,0,0);
            this->Fn2F6140(this->mComponent.get(),0x80000,this,0,0,0,-10,0);
        }
    }
    Container::AddedToParent();
}
// FUNCTION: WON 0x00700b70
// Sets the group's title and sizes the label to it with 10 pixels to spare
void MSEtchedFrameGroup::SetLabelText(const GUIString& text) {
    this->mLabel->SetText(text);
    this->mLabel->SetDesiredSize(10,0);
}
// FUNCTION: WON 0x00700ba0
// Sets the component the group frames
void MSEtchedFrameGroup::AddContainedComponent(Component* c) { this->mComponent=c; }
// FUNCTION: WON 0x00700c00
// Paints the etched group frame from the middle of the title down, leaving a gap for the title
void MSEtchedFrameGroup::Paint(Graphics& g) {
    Container::Paint(g);
    int dy=this->mLabel->Height()/2;
    g.Translate(0,dy);
    int right=this->Width()-1;
    int bottom=this->Height()-1-dy;
    int labelLeft=this->mLabel->Left();
    int labelRight=this->mLabel->Right();
    g.SetSysColor(StandardColor_3DHilight);
    g.DrawLine(0,1,labelLeft,1);
    g.DrawLine(labelRight,1,right,1);
    g.DrawLine(right,0,right,bottom);
    g.DrawLine(0,bottom,right,bottom);
    g.DrawLine(1,1,1,bottom);
    g.SetSysColor(StandardColor_3DShadow);
    g.DrawLine(0,0,labelLeft,0);
    g.DrawLine(labelRight,0,right-1,0);
    g.DrawLine(right-1,0,right-1,bottom-1);
    g.DrawLine(0,bottom-1,right-1,bottom-1);
    g.DrawLine(0,0,0,bottom-1);
    g.Translate(0,-dy);
}
}
