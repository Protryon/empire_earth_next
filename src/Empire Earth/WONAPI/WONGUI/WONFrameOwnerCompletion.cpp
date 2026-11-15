// Frame and frame component constructors and frame assignment (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
namespace WONAPI {
// Creates a frame with the same border width on all four sides (the original's body at 0x007175f0)
WONFrame::WONFrame(Image* image,int size)
{
    this->mImage=image;
    this->mLeft=this->mTop=this->mRight=this->mBottom=size;
}
// Creates a frame with the given image and four border widths (the original's body at 0x00717690)
WONFrame::WONFrame(Image* image,int left,int top,int right,int bottom)
{
    this->mImage=image;
    this->mLeft=left;
    this->mTop=top;
    this->mRight=right;
    this->mBottom=bottom;
}
// Makes this frame a deep copy of another one (the original's body at 0x007177a0)
WONFrame& WONFrame::operator=(const WONFrame& frame)
{
    this->Copy(frame);
    return *this;
}
// Creates a frame component with the given frame image and borders (the original's body at 0x0071a5b0)
WONFrameComponent::WONFrameComponent(Image* image,int left,int top,int right,int bottom)
{
    this->SetFrameImageAndBorders(image,left,top,right,bottom);
}
// The frame component's destructor is implicit (compiler-generated member and base cleanup)
}

#include "ee/wonapi/WONLobbyStretchImage.h"
namespace WONAPI {
// Creates a frame from a stretch image, taking the image's four borders (the original's body at 0x00717550)
WONFrame::WONFrame(StretchImage* image)
{
    this->mImage=image;
    this->mLeft=image->GetLeft();
    this->mTop=image->GetTop();
    this->mRight=image->GetRight();
    this->mBottom=image->GetBottom();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00717550
// (public: __thiscall WONAPI::WONFrame::WONFrame(class WONAPI::StretchImage *)    [??0WONFrame@WONAPI@@QAE@PAVStretchImage@1@@Z])
// TEMPLATE: WON 0x007175f0
// (public: __thiscall WONAPI::WONFrame::WONFrame(class WONAPI::Image *,int)    [??0WONFrame@WONAPI@@QAE@PAVImage@1@H@Z])
// TEMPLATE: WON 0x00717690
// (public: __thiscall WONAPI::WONFrame::WONFrame(class WONAPI::Image *,int,int,int,int)    [??0WONFrame@WONAPI@@QAE@PAVImage@1@HHHH@Z])
// TEMPLATE: WON 0x007177a0
// (public: class WONAPI::WONFrame & __thiscall WONAPI::WONFrame::operator=(class WONAPI::WONFrame const &)    [??4WONFrame@WONAPI@@QAEAAV01@ABV01@@Z])
// TEMPLATE: WON 0x0071a5b0
// (public: __thiscall WONAPI::WONFrameComponent::WONFrameComponent(class WONAPI::Image *,int,int,int,int)    [??0WONFrameComponent@WONAPI@@QAE@PAVImage@1)
// TEMPLATE: WON 0x0071a6b0
// (public: virtual void * __thiscall WONAPI::WONFrameComponent::`scalar deleting destructor'(unsigned int)    [??_GWONFrameComponent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0071a6d0
// (public: virtual __thiscall WONAPI::WONFrameComponent::~WONFrameComponent(void)    [??1WONFrameComponent@WONAPI@@UAE@XZ])
