// WIM image decoder: "WIM" signature, 32-bit RGBA pixels (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstring>
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
namespace WONAPI {
// FUNCTION: WON 0x0074a560
// Reads a WIM header and returns its width and height; false when the signature or bit depth (32) is wrong
bool WIMDecoder::DoGetAttrib(ImageAttributes& attributes)
{
    char signature[4];
    this->mFileReader->ReadBytes(signature,3);
    signature[3]=0;
    if(strcmp(signature,"WIM")!=0) return false;
    if((unsigned long)this->mFileReader->ReadLong()!=32) return false;
    this->mFileReader->ReadByte();
    unsigned long width=this->mFileReader->ReadLong();
    unsigned long height=this->mFileReader->ReadLong();
    attributes.mWidth=width;
    attributes.mHeight=height;
    return true;
}
// FUNCTION: WON 0x0074a600
// Decodes a WIM file into a 32-bit raw image with its transparency flag; returns NULL on a bad header,
// too little data or a read error
SmartPtr<RawImage> WIMDecoder::DoDecodeRaw()
{
    try
    {
        char signature[4];
        this->mFileReader->ReadBytes(signature,3);
        signature[3]=0;
        if(strcmp(signature,"WIM")!=0) return NULL;
        if((unsigned long)this->mFileReader->ReadLong()!=32) return NULL;
        bool transparency=this->mFileReader->ReadByte()!=0;
        unsigned long width=this->mFileReader->ReadLong();
        unsigned long height=this->mFileReader->ReadLong();
        unsigned long numBytes=width*height*4;
        if(this->mFileReader->Available()<numBytes) return NULL;

        SmartPtr<RawImage32> image=new RawImage32(width,height);
        image->mDoTransparency=transparency;
        this->mFileReader->ReadBytes(image->GetImageData(),numBytes);
        return image.get();
    }
    catch(FileReaderException&) {}
    return NULL;
}
// FUNCTION: WON 0x0072ba40
// Returns a new WIM decoder
ImageDecoder* WIMDecoder::Duplicate() { return new WIMDecoder; }
}
