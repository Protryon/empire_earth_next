#ifndef EE_WONLOBBYIMAGEBUTTON_H
#define EE_WONLOBBYIMAGEBUTTON_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
enum ImageButtonType { ImageButtonType_Normal, ImageButtonType_Over, ImageButtonType_Down,
    ImageButtonType_Disabled, ImageButtonType_Checked, ImageButtonType_None };
// ImageButton (0x5c bytes, with RTTI): five owned images and a stretch flag.
class ImageButton : public Button {
protected:
    virtual ~ImageButton(); // explicit destructor (stores its own vptr), at 0x006f7e20
    SmartPtr<Image> mImages[5]; //44..54
    bool mStretch; //58
public:
    ImageButton(); //2f7d70
    void SetImage(ImageButtonType type,Image* image) { this->mImages[type]=image; } // inline (used at 0x0078ad5d)
    virtual void GetDesiredSize(int&,int&); //2f7e80, GetDesiredSize
    virtual void Paint(Graphics&); //2f7ed0
};

class ImageButtonConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0079ad10
    static Component* CompFactory(); // at 0x0079acb0
    ImageButtonConfig();
    virtual bool Virtual24(const std::string&); //38acb0
    virtual void SetComponent(Component*); // at 0x0078f8d0 (shared body)
    ImageButton* mImageButton; //78 borrowed
};
bool Browse(const std::string&); // MiscUtil, at 0x006a30e0
}
#endif
