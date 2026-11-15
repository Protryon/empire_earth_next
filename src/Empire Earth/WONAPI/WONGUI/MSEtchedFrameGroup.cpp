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
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONSkinLayout.h"
namespace WONAPI {
// FUNCTION: WON 0x00700a60
// Returns the contained component's size plus a 20-pixel margin and the title height (20 x 20 when empty;
// skinlay.rs)
void MSEtchedFrameGroup::GetDesiredSize(int& width,int& height) {
    WONSkinLayout(this,EeWonSkinLayKind::EtchedSize,0,0,0,&width,&height);
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
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::EtchedGroup,this->Width(),this->Height(),0,
                              this->mLabel->Height(),this->mLabel->Left(),this->mLabel->Right()));
}
}
