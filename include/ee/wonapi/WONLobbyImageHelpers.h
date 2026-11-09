// ProgramImage (0x10 bytes): Image (0x0c) plus the paint callback at +0x0c.
#ifndef EE_WONAPI_WONLOBBYIMAGEHELPERS_H
#define EE_WONAPI_WONLOBBYIMAGEHELPERS_H
namespace WONAPI {
// ImagePiece (0x18 bytes): Image (0x0c) + owned image at +0x0c + left at +0x10 + top at +0x14.
class ImagePiece : public Image {
private:
    SmartPtr<Image> mImage;
    int mLeft,mTop;
protected:
    virtual ~ImagePiece();
public:
    ImagePiece(Image*,int,int,int,int);
    virtual void Draw(DisplayContext*,int,int,int,int,int,int);
};
class ProgramImage : public Image {
protected:
    typedef void (*PaintFunc)(Graphics&,int,int);
    PaintFunc mPaintFunc;
    virtual void Paint(Graphics&); //3287c0, slot14
public:
    ProgramImage(int = 0,int = 0,PaintFunc = NULL); //328790
    ProgramImage(PaintFunc); // callback-only constructor
    virtual SmartPtr<Image> Duplicate(); //3286d0
    virtual void Draw(DisplayContext*,int,int,int,int,int,int); //3287f0
    virtual void SetSize(int w,int h) { this->mWidth=(unsigned short)w; this->mHeight=(unsigned short)h; }
};
}
#endif
