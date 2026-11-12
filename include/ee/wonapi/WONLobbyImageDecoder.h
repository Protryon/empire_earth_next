// ImageDecoder (8 bytes) and MultiImageDecoder (0x34 bytes).
#ifndef EE_WONAPI_WONLOBBYIMAGEDECODER_H
#define EE_WONAPI_WONLOBBYIMAGEDECODER_H
#include <set>
#include <map>
#include <string>
#include "ee/wonapi/WONStringCompare.h"
namespace WONAPI {
class FileReader;
class DisplayContext;
class NativeImage;
class RawImage;
class ImageAttributes {
public:
    int mWidth,mHeight;
    ImageAttributes(); // at 0x007478f0
};
class ImageDecoder {
protected:
    FileReader* mFileReader; // offset 0x04; class size 0x08, no RefCount base
    virtual SmartPtr<NativeImage> DoDecode(DisplayContext*); //slot00 347940
    virtual SmartPtr<RawImage> DoDecodeRaw()=0; //04
    virtual bool DoGetAttrib(ImageAttributes&)=0; //08
public:
    ImageDecoder(); //347900
    virtual ~ImageDecoder(); // slot 0x0c; complete 0x00747930, deleting 0x00747910
    SmartPtr<NativeImage> Decode(DisplayContext*,const char*); //3479f0
    SmartPtr<NativeImage> Decode(DisplayContext*,FileReader&); //347a70
    SmartPtr<RawImage> DecodeRaw(const char*); //347ab0
    SmartPtr<RawImage> DecodeRaw(FileReader&); //347b30
    bool GetImageAttributes(const char*,ImageAttributes&); //347b70
    bool GetImageAttributes(FileReader&,ImageAttributes&); //347be0
    virtual ImageDecoder* Duplicate()=0; //slot14
};
class MultiImageDecoder : public ImageDecoder {
protected:
    typedef std::set<ImageDecoder*> DecoderSet;
    typedef std::map<std::string,ImageDecoder*,StringLessNoCase> DecoderMap;
    DecoderSet mDecoderSet; //08..13, sole ownership set
    DecoderMap mDecoderMap; //14..1f, borrowed decoder pointers
    std::string mCurExtension; // the original's 0x24..0x33, after its animation pointer (+0x20; no animation now)
    virtual SmartPtr<NativeImage> DoDecode(DisplayContext*); //00 3484e0
    virtual SmartPtr<RawImage> DoDecodeRaw(); //04 348670
    virtual bool DoGetAttrib(ImageAttributes&); //08 3487f0
public:
    MultiImageDecoder(); //347c50
    virtual ~MultiImageDecoder(); // slot 0x0c; complete 0x00747eb0, deleting 0x00747d20
    void AddDecoder(ImageDecoder*,const char*); // at 0x00748080; takes ownership
    void SetCurExtension(const char* s) { this->mCurExtension=s; } // inline
    void SetFileName(const char*); //348270
    SmartPtr<NativeImage> Decode(DisplayContext*,const char*); //348450
    SmartPtr<RawImage> DecodeRaw(const char*); //348490
    bool GetImageAttributes(const char*,ImageAttributes&); //3484c0
    virtual ImageDecoder* Duplicate(); //14 3488e0
};
// Fieldless PNG/JPG decoders: ImageDecoder (8 bytes), six slots; vtables 0x008547c8 and 0x008547e4.
// The inherited slots 0x00/0x0c/0x10 are unchanged.
class PNGDecoder : public ImageDecoder {
protected:
    virtual SmartPtr<RawImage> DoDecodeRaw(); // slot 0x04, 0x007d2660
    virtual bool DoGetAttrib(ImageAttributes&); // slot 0x08, 0x007d2550
public:
    static void Add(MultiImageDecoder*); // at 0x007d24e0
    virtual ImageDecoder* Duplicate(); // slot 0x14, 0x007d29d0
};
class JPGDecoder : public ImageDecoder {
protected:
    virtual SmartPtr<RawImage> DoDecodeRaw(); // slot 0x04, 0x007d2cd0
    virtual bool DoGetAttrib(ImageAttributes&); // slot 0x08, 0x007d2bf0
public:
    static void Add(MultiImageDecoder*); //3d2a20
    virtual ImageDecoder* Duplicate(); // slot 0x14, 0x007d2f50
};
}
#endif
