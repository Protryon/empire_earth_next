// Complete24B native delayed-image owner. Image0c/string0c..1b/bool1c/SP20.
#ifndef EE_WONAPI_WONLOBBYDELAYLOADIMAGE_H
#define EE_WONAPI_WONLOBBYDELAYLOADIMAGE_H
namespace WONAPI {
class DelayLoadImage : public Image {
protected:
    std::string mImagePath;
    bool mHaveLoaded;
    SmartPtr<NativeImage> mImage;
public:
    DelayLoadImage(const std::string&,int,int); //337c00
    virtual void Draw(DisplayContext*,int,int,int,int,int,int); //337d50
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*); //337e50
};
}
#endif
