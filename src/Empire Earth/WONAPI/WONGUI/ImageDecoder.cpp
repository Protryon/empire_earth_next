// Image decoder base class and the extension-dispatching multi-decoder (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
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
ImageAttributes::ImageAttributes()
{
    this->mWidth=this->mHeight=0;
}
ImageDecoder::ImageDecoder()
{
    this->mFileReader=NULL;
}
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

// Reads the image attributes (size) from an open file reader; false if the reader is empty or a read
// error is thrown
bool ImageDecoder::GetImageAttributes(FileReader& reader,ImageAttributes& attributes)
{
    if(!reader.IsOpen()) return false;
    this->mFileReader=&reader;
    try { return this->DoGetAttrib(attributes); }
    catch(FileReaderException&) {}
    return false;
}
MultiImageDecoder::MultiImageDecoder() {}

// Destroys the multi-decoder, deleting every registered decoder
MultiImageDecoder::~MultiImageDecoder()
{
    DecoderSet::iterator i=this->mDecoderSet.begin();
    while(i!=this->mDecoderSet.end())
    {
        delete *i;
        ++i;
    }
}
// Registers a decoder for a file extension, replacing any earlier decoder for that extension
void MultiImageDecoder::AddDecoder(ImageDecoder* decoder,const char* extension)
{
    // A replaced decoder stays in the set and is still deleted by the destructor
    this->mDecoderSet.insert(decoder);
    this->mDecoderMap[extension]=decoder;
}
// Selects the decoder by the path's file extension
void MultiImageDecoder::SetFileName(const char* path)
{
    WONFile file(path);
    this->SetCurExtension(file.GetFileExtension().c_str());
}
// Decodes the file at the path into a native image with the decoder for its extension
SmartPtr<NativeImage> MultiImageDecoder::Decode(DisplayContext* context,const char* path)
{
    this->SetFileName(path);
    return ImageDecoder::Decode(context,path);
}
// Decodes the file at the path into a raw image with the decoder for its extension
SmartPtr<RawImage> MultiImageDecoder::DecodeRaw(const char* path)
{
    this->SetFileName(path);
    return ImageDecoder::DecodeRaw(path);
}
// Reads the attributes of the file at the path with the decoder for its extension
bool MultiImageDecoder::GetImageAttributes(const char* path,ImageAttributes& attributes)
{
    this->SetFileName(path);
    return ImageDecoder::GetImageAttributes(path,attributes);
}
// Decodes the current reader into a native image with the current extension's decoder (the SDK kept the
// decoder's animation; the port has none); NULL when no decoder handles the extension
SmartPtr<NativeImage> MultiImageDecoder::DoDecode(DisplayContext* context)
{
    DecoderMap::iterator i=this->mDecoderMap.find(this->mCurExtension);
    if(i==this->mDecoderMap.end()) return NULL;
    ImageDecoder* decoder=i->second;
    SmartPtr<NativeImage> image=decoder->Decode(context,*this->mFileReader);
    return image;
}
// Decodes the current reader into a raw image with the current extension's decoder; NULL when no decoder
// handles the extension
SmartPtr<RawImage> MultiImageDecoder::DoDecodeRaw()
{
    DecoderMap::iterator i=this->mDecoderMap.find(this->mCurExtension);
    if(i==this->mDecoderMap.end()) return NULL;
    ImageDecoder* decoder=i->second;
    SmartPtr<RawImage> image=decoder->DecodeRaw(*this->mFileReader);
    return image;
}
// Reads the current reader's image attributes with the current extension's decoder; false if none
bool MultiImageDecoder::DoGetAttrib(ImageAttributes& attributes)
{
    DecoderMap::iterator i=this->mDecoderMap.find(this->mCurExtension);
    if(i==this->mDecoderMap.end()) return false;
    ImageDecoder* aDecoder=i->second;
    return aDecoder->GetImageAttributes(*this->mFileReader,attributes);
}
// Creates a copy of the multi-decoder with a duplicate of every per-extension decoder
ImageDecoder* MultiImageDecoder::Duplicate()
{
    MultiImageDecoder* decoder=new MultiImageDecoder;
    decoder->mDecoderMap=this->mDecoderMap;
    DecoderMap::iterator i=decoder->mDecoderMap.begin();
    while(i!=decoder->mDecoderMap.end())
    {
        i->second=i->second->Duplicate();
        decoder->mDecoderSet.insert(i->second);
        ++i;
    }
    return decoder;
}
}
