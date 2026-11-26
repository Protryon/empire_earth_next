// The image decoders (R6 WONLobby round 24): MultiImageDecoder's decoders by extension, its decodes and the PNG and
// JPEG decoders are ee_won_gui's imgobj.rs (the decodes imgdec.rs); a FileReader status comes back to be thrown here,
// where the original's reads threw.
#ifndef EE_WONAPI_WONLOBBYIMAGEDECODER_H
#define EE_WONAPI_WONLOBBYIMAGEDECODER_H
#include "rust/won_gui/imgobj.h"
// (the platform window and manager, which WONLobbyGUI.h's Window and WindowManager are since R6 WON GUI round 35)
#include "platform/PlatWindow.h"
#include "rust/won_gui/platwin.h"
#include "rust/won_gui/platwm.h"
#include "rust/won_gui/platloop.h"
#include "rust/won_gui/cursorobj.h"
#include "rust/won_gui/platfont.h"
#include "rust/won_gui/platgfx.h"
#include "rust/won_gui/gfxplat.h"
namespace WONAPI {
class DisplayContext;
class Image;
typedef Image NativeImage;
typedef Image RawImage;
class ImageAttributes {
public:
    int mWidth,mHeight;
    ImageAttributes() : mWidth(0),mHeight(0) {}
};
void WONImageThrow(int inStatus); // throws the FileReaderException of a FileReader status (PlatNativeImage.cpp)
intptr_t WONImageHost(EeWonImageHost op, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
class MultiImageDecoder {
    EeWonImgDecoder* mRust;
    intptr_t Run(EeWonImgDecoderRun r,DisplayContext* c,const char* p,int* wh=NULL)
    {
        int32_t t=0, w[2];
        intptr_t i=ee_won_img_decoder_run(this->mRust,r,(intptr_t)c,p,wh!=NULL?wh:w,&t);
        if(t!=0) WONImageThrow(t);
        return i;
    }
    MultiImageDecoder(const MultiImageDecoder&);
    MultiImageDecoder& operator=(const MultiImageDecoder&);
public:
    MultiImageDecoder() : mRust(ee_won_img_decoder_new(WONImageHost)) {}
    ~MultiImageDecoder() { ee_won_img_decoder_free(this->mRust); }
    void AddDecoder(EeWonImgKind k,const char* ext) { ee_won_img_decoder_add(this->mRust,k,ext); } // replaces one there
    SmartPtr<NativeImage> Decode(DisplayContext*,const char*); // with the texture pack's copy when there is one
    SmartPtr<RawImage> DecodeRaw(const char*);
    bool GetImageAttributes(const char* p,ImageAttributes& a)
    { int wh[2]; if(!this->Run(EeWonImgDecoderRun::Attributes,NULL,p,wh)) return false; a.mWidth=wh[0]; a.mHeight=wh[1]; return true; }
};
struct PNGDecoder { static void Add(MultiImageDecoder* d) { d->AddDecoder(EeWonImgKind::Png,"png"); } };
struct JPGDecoder {
    static void Add(MultiImageDecoder* d)
    { static const char* const kExt[4]={"jpg","jpe","jpeg","pjpeg"}; for(int i=0;i<4;i++) d->AddDecoder(EeWonImgKind::Jpg,kExt[i]); }
};
}
#endif
