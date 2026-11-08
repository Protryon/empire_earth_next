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
#include "ee/wonapi/WONLobbyAnimation.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
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
SmartPtr<Animation> ImageDecoder::GetAnimation() { return NULL; }
// FUNCTION: WON 0x00747910
// (??_GImageDecoder@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x00747930
// (??1ImageDecoder@WONAPI@@UAE@XZ)

// FUNCTION: WON 0x00747940
// Decodes a raw image and converts it to a native image for the display context; NULL on failure
SmartPtr<NativeImage> ImageDecoder::DoDecode(DisplayContext* context)
{
    SmartPtr<RawImage> image=this->DoDecodeRaw();
    if(image.get()!=NULL) return image->GetNative(context);
    return NULL;
}
SmartPtr<NativeImage> ImageDecoder::Decode(DisplayContext* context,const char* path)
{
    FileReader reader(path);
    return this->Decode(context,reader);
}
SmartPtr<NativeImage> ImageDecoder::Decode(DisplayContext* context,FileReader& reader)
{
    if(reader.mFile==NULL && reader.mBuffer==NULL) return NULL;
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
    if(reader.mFile==NULL && reader.mBuffer==NULL) return NULL;
    this->mFileReader=&reader;
    return this->DoDecodeRaw(); // the original has no FileReaderException catch in this overload
}
bool ImageDecoder::GetImageAttributes(const char* path,ImageAttributes& attributes)
{
    FileReader reader(path);
    return this->GetImageAttributes(reader,attributes);
}
// FUNCTION: WON 0x007479f0
// (?Decode@ImageDecoder@WONAPI@@QAE?AV?$SmartPtr@VNativeImage@WONAPI@@@2@PAVDisplayContext@2@PBD@Z)

// FUNCTION: WON 0x00747a70
// (?Decode@ImageDecoder@WONAPI@@QAE?AV?$SmartPtr@VNativeImage@WONAPI@@@2@PAVDisplayContext@2@AAVFileReader@2@@Z)

// FUNCTION: WON 0x00747ab0
// (?DecodeRaw@ImageDecoder@WONAPI@@QAE?AV?$SmartPtr@VRawImage@WONAPI@@@2@PBD@Z)

// FUNCTION: WON 0x00747b30
// (?DecodeRaw@ImageDecoder@WONAPI@@QAE?AV?$SmartPtr@VRawImage@WONAPI@@@2@AAVFileReader@2@@Z)

// FUNCTION: WON 0x00747b70
// (?GetImageAttributes@ImageDecoder@WONAPI@@QAE_NPBDAAVImageAttributes@2@@Z)

// FUNCTION: WON 0x00747be0
// Reads the image attributes (size) from an open file reader; false if the reader is empty or a read
// error is thrown
bool ImageDecoder::GetImageAttributes(FileReader& reader,ImageAttributes& attributes)
{
    if(reader.mFile==NULL && reader.mBuffer==NULL) return false;
    this->mFileReader=&reader;
    try { return this->DoGetAttrib(attributes); }
    catch(FileReaderException&) {}
    return false;
}
MultiImageDecoder::MultiImageDecoder() {}
// FUNCTION: WON 0x00747c50
// (??0MultiImageDecoder@WONAPI@@QAE@XZ)

// FUNCTION: WON 0x00747d20
// (??_GMultiImageDecoder@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x00747d40
// (??1?$set@PAVImageDecoder@WONAPI@@U?$less@PAVImageDecoder@WONAPI@@@std@@V?$allocator@PAVImageDecoder@WONAPI@@@4@@std@@QAE@XZ)

// FUNCTION: WON 0x00747eb0
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
// FUNCTION: WON 0x00748080
// Registers a decoder for a file extension, replacing any earlier decoder for that extension
void MultiImageDecoder::AddDecoder(ImageDecoder* decoder,const char* extension)
{
    // A replaced decoder stays in the set and is still deleted by the destructor
    this->mDecoderSet.insert(decoder);
    this->mDecoderMap[extension]=decoder;
}
// FUNCTION: WON 0x00748270
// Selects the decoder by the path's file extension
void MultiImageDecoder::SetFileName(const char* path)
{
    WONFile file(path);
    this->SetCurExtension(file.GetFileExtension().c_str());
}
// FUNCTION: WON 0x00748450
// Decodes the file at the path into a native image with the decoder for its extension
SmartPtr<NativeImage> MultiImageDecoder::Decode(DisplayContext* context,const char* path)
{
    this->SetFileName(path);
    return ImageDecoder::Decode(context,path);
}
// FUNCTION: WON 0x00748490
// Decodes the file at the path into a raw image with the decoder for its extension
SmartPtr<RawImage> MultiImageDecoder::DecodeRaw(const char* path)
{
    this->SetFileName(path);
    return ImageDecoder::DecodeRaw(path);
}
// FUNCTION: WON 0x007484c0
// Reads the attributes of the file at the path with the decoder for its extension
bool MultiImageDecoder::GetImageAttributes(const char* path,ImageAttributes& attributes)
{
    this->SetFileName(path);
    return ImageDecoder::GetImageAttributes(path,attributes);
}
// FUNCTION: WON 0x007484e0
// Decodes the current reader into a native image with the current extension's decoder, keeping that
// decoder's animation; NULL when no decoder handles the extension
SmartPtr<NativeImage> MultiImageDecoder::DoDecode(DisplayContext* context)
{
    this->mAnimation=NULL;
    DecoderMap::iterator i=this->mDecoderMap.find(this->mCurExtension);
    if(i==this->mDecoderMap.end()) return NULL;
    ImageDecoder* decoder=i->second;
    SmartPtr<NativeImage> image=decoder->Decode(context,*this->mFileReader);
    if(image.get()!=NULL) this->mAnimation=decoder->GetAnimation();
    return image;
}
// FUNCTION: WON 0x00748670
// Decodes the current reader into a raw image with the current extension's decoder, keeping that
// decoder's animation; NULL when no decoder handles the extension
SmartPtr<RawImage> MultiImageDecoder::DoDecodeRaw()
{
    this->mAnimation=NULL;
    DecoderMap::iterator i=this->mDecoderMap.find(this->mCurExtension);
    if(i==this->mDecoderMap.end()) return NULL;
    ImageDecoder* decoder=i->second;
    SmartPtr<RawImage> image=decoder->DecodeRaw(*this->mFileReader);
    if(image.get()!=NULL) this->mAnimation=decoder->GetAnimation();
    return image;
}
// FUNCTION: WON 0x007487f0
// Reads the current reader's image attributes with the current extension's decoder; false if none
bool MultiImageDecoder::DoGetAttrib(ImageAttributes& attributes)
{
    DecoderMap::iterator i=this->mDecoderMap.find(this->mCurExtension);
    if(i==this->mDecoderMap.end()) return false;
    ImageDecoder* aDecoder=i->second;
    return aDecoder->GetImageAttributes(*this->mFileReader,attributes);
}
// FUNCTION: WON 0x00748870
// Returns the last decoded animation; NULL for a single-frame one unless evenIfOnlyOneFrame is set
SmartPtr<Animation> MultiImageDecoder::GetAnimation(bool evenIfOnlyOneFrame)
{
    if(evenIfOnlyOneFrame) return this->mAnimation;
    else if(this->mAnimation.get()==NULL || this->mAnimation->GetNumFrames()<=1) return NULL;
    else return this->mAnimation;
}
// FUNCTION: WON 0x007488e0
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


// Decoder overloads that read from a memory buffer (not identified in the original exe)
namespace WONAPI {
SmartPtr<NativeImage> ImageDecoder::Decode(DisplayContext *theContext, const void *theData, unsigned long theDataLen)
{
	FileReader aReader(theData,theDataLen);
	return this->Decode(theContext, aReader);
}

SmartPtr<RawImage> ImageDecoder::DecodeRaw(const void *theData, unsigned long theDataLen)
{
	FileReader aReader(theData,theDataLen);
	return this->DecodeRaw(aReader);
}

bool ImageDecoder::GetImageAttributes(const void *theData, unsigned long theDataLen, ImageAttributes &theAttrib)
{
	FileReader aReader(theData,theDataLen);
	return this->GetImageAttributes(aReader,theAttrib);
}
}

namespace WONAPI {
// Raw image allocation helpers for decoders (not identified in the original exe)
SmartPtr<RawImage32> ImageDecoder::CreateRawImage32(int theWidth,int theHeight)
{
    return new RawImage32(theWidth,theHeight);
}
SmartPtr<RawImage8> ImageDecoder::CreateRawImage8(int theWidth,int theHeight,Palette* thePalette)
{
    return new RawImage8(theWidth,theHeight,thePalette);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00747900
// (public: __thiscall WONAPI::ImageDecoder::ImageDecoder(void))
// TEMPLATE: WON 0x00747df0
// (public: __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class WONAPI::ImageDecoder *,struc)
// TEMPLATE: WON 0x007478f0
// (public: __thiscall WONAPI::ImageAttributes::ImageAttributes(void))
// TEMPLATE: WON 0x0074b670
// (protected: class std::_Tree<class WONAPI::ImageDecoder *,class WONAPI::ImageDecoder *,struct std::_Setk<class WONAPI::ImageDecoder *,struct std::less<)
// TEMPLATE: WON 0x0074b970
// (public: struct std::pair<class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<cla)
// TEMPLATE: WON 0x0074ba70
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0074bb20
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x0074cfb0
// (public: class std::_Tree<class WONAPI::ImageDecoder *,class WONAPI::ImageDecoder *,struct std::_Setk<class WONAPI::ImageDecoder *,struct std::less<cla)
// TEMPLATE: WON 0x0074d460
// (protected: void __thiscall std::_Tree<class WONAPI::ImageDecoder *,class WONAPI::ImageDecoder *,struct std::_Setk<class WONAPI::ImageDecoder *,struct )
// TEMPLATE: WON 0x0074d9a0
// (protected: struct std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic)
// TEMPLATE: WON 0x0074da30
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x0074da70
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)
// TEMPLATE: WON 0x00751f80
// (protected: struct std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic)
// TEMPLATE: WON 0x0074d4c0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x00754c60
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
