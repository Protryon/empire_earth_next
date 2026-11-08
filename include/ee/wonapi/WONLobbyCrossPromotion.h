// Image buttons with a caption; CrossPromotionButton shows the cross-promotion image and opens its link
#ifndef EE_WONAPI_WONLOBBYCROSSPROMOTION_H
#define EE_WONAPI_WONLOBBYCROSSPROMOTION_H
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyImageButton.h"
namespace WONAPI {
class SetCrossPromotionEvent;
class ImageTextButton : public ImageButton {
public:
    ImageTextButton(const GUIString& = "");
    virtual void Paint(Graphics&);
    virtual bool HandleActivate();
    SmartPtr<Font> mFont; // 0x5c
    GUIString mText; // 0x60
    GUIString mBrowse; // 0x64
};
class CrossPromotionButton : public ImageTextButton {
public:
    CrossPromotionButton(); // 0x7c96a0; object size 0x70
    void HandleSetCrossPromotion(SetCrossPromotionEvent*);
    void SetDefaultImage(Image*);
    virtual void HandleComponentEvent(ComponentEvent*);
    SmartPtr<Image> mDefaultImage; // 0x68
    void SetDefaultBrowse(const GUIString& inBrowse) { this->mDefaultBrowse=inBrowse; }
    GUIString mDefaultBrowse; // 0x6c
};
}
#endif
