#ifndef EE_WONAPI_WONLOBBYMSSKINIMAGE_H
#define EE_WONAPI_WONLOBBYMSSKINIMAGE_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// WONSkin and WONMSSkin's constructor (R6 WONLobby round 23): the skin, its templates and type maps are the Rust
// skin (rust/crates/ee_won_gui/src/skinobj.rs); this keeps the types and flags of the Windows look's drawn images
// (MSImage: an imgobj.rs kind since round 24, WONNewMsImage).
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
};
// new WONMSSkin::MSImage(type, flags, width, height)
inline Image* WONNewMsImage(unsigned char t,unsigned char f,int w,int h) { return new Image(ee_won_image_new_ms(WONImageHost,t,f,w,h)); }
}
#endif
