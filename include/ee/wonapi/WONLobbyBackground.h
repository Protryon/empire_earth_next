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
    int mColor; //04
    int mBorderColor; //08
    bool mUseOffsets; //0c
    bool mStretchImage; //0d
    bool mWantGrabBG; //0e, natural padding0f
    SmartPtr<Component> mComponent; //10, end14
    Background(); //335cd0
    Background(Image*); //335cf0
    Background(int,int=-1); //335d80
    Background(Component*); //335db0
    void Init(); //335cb0
    void SetColor(int); //335e50
    void Paint(Graphics&,int,int,int,int,int=0,int=0); //335e60
    bool GetWantGrabBG() const { return this->mWantGrabBG; }
    // Actual implicit copy/assignment/destruction retain/release bothSmartPtrs.
};
}
#endif
