// Complete native Background value owner:20decimal/0x14 bytes; no vtable/base.
// Requires established WONAPI SmartPtr; Image/Component objects remain native-owned.
#ifndef EE_WONAPI_WONLOBBYBACKGROUND_H
#define EE_WONAPI_WONLOBBYBACKGROUND_H
namespace WONAPI {
class Image;
class Component;
class Graphics;
class Background {
public:
    SmartPtr<Image> mImage; //00
    // the defaults (Init, 0x335cb0): no colour, no border, a tiled image with offsets, no grab
    int mColor = -1; //04
    int mBorderColor = -1; //08
    bool mUseOffsets = true; //0c
    bool mStretchImage = false; //0d
    bool mWantGrabBG = false; //0e, natural padding0f
    SmartPtr<Component> mComponent; //10, end14
    Background(); //335cd0: the standard colour 13
    Background(Image*); //335cf0: a tiled image
    Background(int,int=-1); //335d80: a fill and border colour
    Background(Component*); //335db0: a painted component
    void SetColor(int); //335e50
    void Paint(Graphics&,int,int,int,int,int=0,int=0); //335e60
    bool GetWantGrabBG() const { return this->mWantGrabBG; }
    // Actual implicit copy/assignment/destruction retain/release bothSmartPtrs.
};
}
#endif
