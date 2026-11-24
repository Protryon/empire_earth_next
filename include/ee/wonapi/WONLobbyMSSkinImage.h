#ifndef EE_WONAPI_WONLOBBYMSSKINIMAGE_H
#define EE_WONAPI_WONLOBBYMSSKINIMAGE_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// WONSkin and WONMSSkin's constructor (R6 WONLobby round 23): the skin, its templates and type maps are the Rust
// skin (rust/crates/ee_won_gui/src/skinobj.rs); the Windows look's drawn images are an imgobj.rs kind (MSImage,
// round 24: WONNewMsImage), their types and flags mspaint.rs's (the class's unused enums went in GUI round 35).
// new WONMSSkin::MSImage(type, flags, width, height)
inline Image* WONNewMsImage(unsigned char t,unsigned char f,int w,int h) { return new Image(ee_won_image_new_ms(WONImageHost,t,f,w,h)); }
}
#endif
