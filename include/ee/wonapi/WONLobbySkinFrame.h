#ifndef EE_WONAPI_WONLOBBYSKINFRAME_H
#define EE_WONAPI_WONLOBBYSKINFRAME_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
class StretchImage;
// WONFrame (0x14) is declared in WONLobbyGUI.h.
// WONSimpleButton (0x58 bytes), vtable 0x00850614; the images are retained Image pointers.
class WONSimpleButton : public Button {
public:
    WONSimpleButton(); // at 0x00717990 (0x58 bytes)
    WONSimpleButton* Duplicate(); // at 0x00717ae0
    virtual void GetDesiredSize(int&,int&); // GetDesiredSize (slot 0x0c), at 0x00717a90
    virtual void Paint(Graphics&); // slot 0x18, at 0x00717bd0
    void SetImage(int state,Image* image) { this->mImages[state]=image; }
    void SetScaleImage(bool v) { this->mScaleImage=v; }
    void SetCheckMouseButtonDown(bool v) { this->mCheckMouseButtonDown=v; }
    SmartPtr<Image> mImages[4]; //44..53
    bool mScaleImage,mCheckMouseButtonDown; //54,55
};
// WONScrollThumb: fieldless WONSimpleButton (0x58), vtable 0x0085085c (47 slots), constructed at 0x007194a0.
class WONScrollThumb : public WONSimpleButton {
public:
    WONScrollThumb(); // at 0x007194a0
};
}
#endif
