// Image decoders: the base class, the extension-dispatching multi-decoder, the PNG and JPEG decoders (file name
// inferred). R6 WON round 41: the multi-decoder's map, set and current extension and both decodes are ee_won's
// (rust/crates/ee_won_gui/src/imgdec.rs; the JPEG decode is libjpeg 6b's in ee_base's jpeg.rs, the PNG one ee_openformats'); these
// classes are their shells: the objects, the virtual calls, the new RawImage32 and the FileReader's throws.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "ee/wonapi/WONLobbyPlatform.h"
namespace WONAPI {
ImageAttributes::ImageAttributes() { this->mWidth=this->mHeight=0; }
ImageDecoder::ImageDecoder() { this->mFileReader=NULL; }
ImageDecoder::~ImageDecoder() {}

// Decodes a raw image and converts it to a native image for the display context; NULL on failure
SmartPtr<NativeImage> ImageDecoder::DoDecode(DisplayContext* context)
{
    SmartPtr<RawImage> image=this->DoDecodeRaw();
    if(image.get()!=NULL) return image->GetNative(context);
    return NULL;
}
// port: with the texture pack's larger copy of the file when it has one (WONLobbyPlatform.h PlatGUIPackImage)
SmartPtr<NativeImage> ImageDecoder::Decode(DisplayContext* context,const char* path)
{
    FileReader reader(path);
    SmartPtr<NativeImage> image=this->Decode(context,reader);
    if(image.get()!=NULL) image=PlatGUIPackImage(context,image.get(),path);
    return image;
}
SmartPtr<NativeImage> ImageDecoder::Decode(DisplayContext* context,FileReader& reader)
{
    if(!reader.IsOpen()) return NULL;
    this->mFileReader=&reader;
    return this->DoDecode(context);
}
SmartPtr<RawImage> ImageDecoder::DecodeRaw(const char* path)
{
    FileReader reader(path);
    return this->DecodeRaw(reader);
}
SmartPtr<RawImage> ImageDecoder::DecodeRaw(FileReader& reader)
{
    if(!reader.IsOpen()) return NULL;
    this->mFileReader=&reader;
    return this->DoDecodeRaw(); // the original has no FileReaderException catch in this overload
}
bool ImageDecoder::GetImageAttributes(const char* path,ImageAttributes& attributes)
{
    FileReader reader(path);
    return this->GetImageAttributes(reader,attributes);
}
// false if the reader is empty or a read error is thrown
bool ImageDecoder::GetImageAttributes(FileReader& reader,ImageAttributes& attributes)
{
    if(!reader.IsOpen()) return false;
    this->mFileReader=&reader;
    try { return this->DoGetAttrib(attributes); }
    catch(FileReaderException&) {}
    return false;
}

// The decoder inKind's attributes and raw image (imgdec.rs) over the reader; a reader status is thrown where the
// original's read threw it
bool ImageDecoder::RustAttrib(int inKind,ImageAttributes& outAttributes)
{
    int32_t wh[2], thrown=0;
    bool ok=ee_won_img_attrib((EeWonImgKind)inKind,&this->mFileReader->mCore,&wh,&thrown);
    this->mFileReader->Check(thrown);
    if(ok) { outAttributes.mWidth=wh[0]; outAttributes.mHeight=wh[1]; }
    return ok;
}
SmartPtr<RawImage> ImageDecoder::RustDecodeRaw(int inKind)
{
    EeWonImgRaw raw={0,0,false,NULL};
    int32_t thrown=0;
    bool ok=ee_won_img_decode((EeWonImgKind)inKind,&this->mFileReader->mCore,&raw,&thrown);
    this->mFileReader->Check(thrown);
    if(!ok) return NULL;
    // Allocates the RawImage32 directly (not through CreateRawImage32)
    SmartPtr<RawImage32> image=new RawImage32(raw.width,raw.height);
    image->SetDoTransparency(raw.transparent);
    ee_won_img_take(&raw,(uint32_t*)image->GetImageData());
    return image.get();
}

// ---- the multi-decoder: the map, the set and the extension are imgdec.rs's EeWonImgDec

uintptr_t MultiImageDecoder::Call(EeWonImgDecCall inCall,uintptr_t a,const char* b,size_t c)
{
    return ee_won_img_dec(this->mRust,inCall,a,(const uint8_t*)b,c);
}
MultiImageDecoder::MultiImageDecoder() : mRust(ee_won_img_dec_new()) {}
// Destroys the multi-decoder, deleting every registered decoder (a replaced decoder stays in the set)
MultiImageDecoder::~MultiImageDecoder()
{
    for(uintptr_t i=0,n=this->Call(EeWonImgDecCall::OwnedCount);i<n;i++)
        delete (ImageDecoder*)this->Call(EeWonImgDecCall::Owned,i);
    ee_won_img_dec_free(this->mRust);
}
// Registers a decoder for a file extension, replacing any earlier decoder for that extension; takes ownership
void MultiImageDecoder::AddDecoder(ImageDecoder* decoder,const char* extension)
{
    this->Call(EeWonImgDecCall::Add,(uintptr_t)decoder,extension,strlen(extension));
}
void MultiImageDecoder::SetCurExtension(const char* s) { this->Call(EeWonImgDecCall::SetExtension,0,s,strlen(s)); }
// Selects the decoder by the path's file extension
void MultiImageDecoder::SetFileName(const char* path)
{
    WONFile file(path);
    this->SetCurExtension(file.GetFileExtension().c_str());
}
SmartPtr<NativeImage> MultiImageDecoder::Decode(DisplayContext* context,const char* path)
{
    this->SetFileName(path);
    return ImageDecoder::Decode(context,path);
}
SmartPtr<RawImage> MultiImageDecoder::DecodeRaw(const char* path)
{
    this->SetFileName(path);
    return ImageDecoder::DecodeRaw(path);
}
bool MultiImageDecoder::GetImageAttributes(const char* path,ImageAttributes& attributes)
{
    this->SetFileName(path);
    return ImageDecoder::GetImageAttributes(path,attributes);
}
// The current extension's decoder on the current reader (the SDK kept the decoder's animation; the port has none);
// NULL or false when no decoder handles the extension
SmartPtr<NativeImage> MultiImageDecoder::DoDecode(DisplayContext* context)
{
    ImageDecoder* decoder=(ImageDecoder*)this->Call(EeWonImgDecCall::Find);
    return decoder!=NULL ? decoder->Decode(context,*this->mFileReader) : NULL;
}
SmartPtr<RawImage> MultiImageDecoder::DoDecodeRaw()
{
    ImageDecoder* decoder=(ImageDecoder*)this->Call(EeWonImgDecCall::Find);
    return decoder!=NULL ? decoder->DecodeRaw(*this->mFileReader) : NULL;
}
bool MultiImageDecoder::DoGetAttrib(ImageAttributes& attributes)
{
    ImageDecoder* decoder=(ImageDecoder*)this->Call(EeWonImgDecCall::Find);
    return decoder!=NULL && decoder->GetImageAttributes(*this->mFileReader,attributes);
}
// A copy with a duplicate of every per-extension decoder (one per map entry)
ImageDecoder* MultiImageDecoder::Duplicate()
{
    MultiImageDecoder* decoder=new MultiImageDecoder;
    ee_won_img_dec_free(decoder->mRust);
    decoder->mRust=ee_won_img_dec_copy(this->mRust);
    for(uintptr_t i=0,n=decoder->Call(EeWonImgDecCall::Count);i<n;i++) {
        ImageDecoder* copy=((ImageDecoder*)decoder->Call(EeWonImgDecCall::Entry,i))->Duplicate();
        decoder->Call(EeWonImgDecCall::SetEntry,i,(const char*)copy,0);
    }
    return decoder;
}

// ---- the PNG decoder ("png") and the JPEG decoder (jpg, jpe, jpeg and pjpeg: one decoder)

void PNGDecoder::Add(MultiImageDecoder* decoder) { decoder->AddDecoder(new PNGDecoder,"png"); }
ImageDecoder* PNGDecoder::Duplicate() { return new PNGDecoder; }
bool PNGDecoder::DoGetAttrib(ImageAttributes& attributes) { return this->RustAttrib((int)EeWonImgKind::Png,attributes); }
SmartPtr<RawImage> PNGDecoder::DoDecodeRaw() { return this->RustDecodeRaw((int)EeWonImgKind::Png); }
void JPGDecoder::Add(MultiImageDecoder* decoder)
{
    JPGDecoder* jpg=new JPGDecoder;
    decoder->AddDecoder(jpg,"jpg");
    decoder->AddDecoder(jpg,"jpe");
    decoder->AddDecoder(jpg,"jpeg");
    decoder->AddDecoder(jpg,"pjpeg");
}
ImageDecoder* JPGDecoder::Duplicate() { return new JPGDecoder; }
bool JPGDecoder::DoGetAttrib(ImageAttributes& attributes) { return this->RustAttrib((int)EeWonImgKind::Jpg,attributes); }
SmartPtr<RawImage> JPGDecoder::DoDecodeRaw() { return this->RustDecodeRaw((int)EeWonImgKind::Jpg); }
}
