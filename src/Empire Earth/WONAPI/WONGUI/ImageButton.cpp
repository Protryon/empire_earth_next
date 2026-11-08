// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONComponentFlags.h"
using namespace WONAPI;
// FUNCTION: WON 0x006f7d70
// Creates an image button that takes focus but not tab focus, with stretching off
ImageButton::ImageButton() {
    this->SetButtonFlags(1,true);
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
    this->SetComponentFlags(ComponentFlag_WantTabFocus,false);
    this->mStretch=false;
}
// FUNCTION: WON 0x006f7e20
// Destroys the image button (members only)
ImageButton::~ImageButton() {}
// FUNCTION: WON 0x006f7e80
// Returns the largest width and height among the button's five state images
void ImageButton::GetDesiredSize(int& outWidth,int& outHeight) {
    int width=0,height=0;
    for(int i=0;i<5;++i) {
        Image* image=this->mImages[i].get();
        if(image) {
            if(image->mWidth>width) width=image->mWidth;
            if(image->mHeight>height) height=image->mHeight;
        }
    }
    outWidth=width;
    outHeight=height;
}
// FUNCTION: WON 0x006f7ed0
// Draws the image for the current state (disabled, checked, down, over), falling back to the normal image,
// resizing it to the button first when stretching
void ImageButton::Paint(Graphics& g) {
    Component::Paint(g);
    Image* image=NULL;
    if(this->IsDisabled()) image=this->mImages[ImageButtonType_Disabled].get();
    else if(this->IsChecked() && this->mImages[ImageButtonType_Checked].get())
        image=this->mImages[ImageButtonType_Checked].get();
    else if(this->IsPressed()) image=this->mImages[ImageButtonType_Down].get();
    else if(this->IsMouseOver()) image=this->mImages[ImageButtonType_Over].get();
    if(!image) image=this->mImages[ImageButtonType_Normal].get();
    if(!image) return;
    if(this->mStretch && (image->mWidth!=this->Width() || image->mHeight!=this->Height()))
        image->SetSize(this->Width(),this->Height());
    g.DrawImage(image,0,0);
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f7e00
// (protected: virtual void * __thiscall WONAPI::ImageButton::`scalar deleting destructor'(unsigned int)    [??_GImageButton@WONAPI@@MAEPAXI@Z])
