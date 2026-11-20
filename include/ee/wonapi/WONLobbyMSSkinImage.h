#ifndef EE_WONAPI_WONLOBBYMSSKINIMAGE_H
#define EE_WONAPI_WONLOBBYMSSKINIMAGE_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyImageHelpers.h"
namespace WONAPI {
// WONSkin and WONMSSkin's constructor (R6 WONLobby round 23): the skin, its templates and type maps are the Rust
// skin (rust/crates/ee_won_gui/src/skinobj.rs); this keeps the Windows look's drawn images (MSImage) and their
// types and flags.
class WONMSSkin {
public:
    enum MSImageFlag { MSImageFlag_Down=1,MSImageFlag_Default=2,MSImageFlag_Disabled=4,MSImageFlag_Checked=8 };
    enum MSImageType {
        MSImageType_Frame1,MSImageType_Frame2,MSImageType_Frame3,
        MSImageType_LeftArrow,MSImageType_RightArrow,MSImageType_UpArrow,MSImageType_DownArrow,
        MSImageType_PushButton,MSImageType_Checkbox,MSImageType_RadioButton,
        MSImageType_ScrollThumb,MSImageType_ScrollGutter,MSImageType_ScrollCorner,
        MSImageType_ListHeader,MSImageType_TabButton,MSImageType_TabCtrlFrame,
        MSImageType_ComboPopupFrame,MSImageType_CloseButton
    };
    // MSImage (0x14 bytes): ProgramImage (0x10) + two bytes at +0x10/+0x11 + padding.
    // Implicit destructor (same body as 0x00728770).
    class MSImage : public ProgramImage {
    public:
        unsigned char mImageType,mImageFlags;
        MSImage(unsigned char t,unsigned char f=0,int w=0,int h=0) : ProgramImage(w,h),mImageType(t),mImageFlags(f) {} // inline: 0x14 bytes, ProgramImage constructor 0x00728790, then the bytes at +0x10/+0x11
        virtual SmartPtr<Image> Duplicate(); // at 0x00715a10
        virtual void Paint(Graphics&); // slot 0x14, at 0x007172b0
    };
};
}
#endif
