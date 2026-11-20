// StretchImage and StretchCenterImage (R6 WONLobby round 24): imgobj.rs kinds (the borders, tiling, skip-centre and
// rebuild flag of round 38's stretchobj.rs, the source and cached images, the paints, StretchImageFilter); these are
// their handles (WONLobbyGUI.h "The images").
#ifndef EE_WONAPI_WONLOBBYSTRETCHIMAGE_H
#define EE_WONAPI_WONLOBBYSTRETCHIMAGE_H
#include "rust/won_gui/imgobj.h"
namespace WONAPI {
class StretchImage : public Image {
public:
    explicit StretchImage(EeWonImage* r) : Image(r) {}
    StretchImage(RawImage* i,int l,int t,int r,int b) : Image(ee_won_image_new_stretch(WONImageHost,(intptr_t)i,l,t,r,b)) {}
    int GetLeft() { return (int)this->Get(EeWonImageField::Left); }
    int GetRight() { return (int)this->Get(EeWonImageField::Right); }
    int GetTop() { return (int)this->Get(EeWonImageField::Top); }
    int GetBottom() { return (int)this->Get(EeWonImageField::Bottom); }
    void SetImage(RawImage* i) { this->Call(EeWonImageCall::StretchSetImage,(intptr_t)i); } // the cache not marked
    void SetBorders(int l,int t,int r,int b) { this->Call(EeWonImageCall::StretchSetBorders,l,t,r,b); }
    void CopyAttributes(StretchImage* f) { this->Call(EeWonImageCall::StretchCopy,(intptr_t)f); } // and the size
    void SetTile(bool h,bool v) { this->Call(EeWonImageCall::StretchSetTile,h,v); }
    void SetSkipCenter(bool b) { this->Call(EeWonImageCall::StretchSetSkip,b); }
};
// a frame of a stretch image: its own borders
inline WONFrame::WONFrame(StretchImage* i) : mLeft(i->GetLeft()),mTop(i->GetTop()),mRight(i->GetRight()),mBottom(i->GetBottom()) { this->mImage=i; }
class StretchCenterImage : public Image {
public:
    explicit StretchCenterImage(EeWonImage* r) : Image(r) {}
    StretchCenterImage() : Image(ee_won_image_new_stretch_center(WONImageHost,0,0)) {}
    void SetStretchImage(Image* i) { this->Call(EeWonImageCall::SetStretchImage,(intptr_t)i); }
    void SetCenterImage(Image* i) { this->Call(EeWonImageCall::SetCenterImage,(intptr_t)i); }
};
}
#endif
