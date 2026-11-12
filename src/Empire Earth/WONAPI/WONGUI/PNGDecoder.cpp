// PNG image decoder (file name inferred). The original decoded on libpng 1.0.12; the port decodes with the game's
// own PNG reader (rust/crates/ee_openformats png.rs, ee_of_png_to_picture: GESurface::LoadPNG's), the same pixels
// on every picture the lobby reads (tests/won/won_image_test.cpp's golden over the libpng decoder).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "rust/ee_openformats.h"
namespace WONAPI {
// FUNCTION: WON 0x007d24e0
// Registers a PNG decoder for the "png" extension with a multi-format decoder
void PNGDecoder::Add(MultiImageDecoder* decoder)
{
    PNGDecoder* png=new PNGDecoder;
    decoder->AddDecoder(png,"png");
}
// FUNCTION: WON 0x007d29d0
// Returns a new PNG decoder
ImageDecoder* PNGDecoder::Duplicate() { return new PNGDecoder; }

// FUNCTION: WON 0x007d2550
// Reads the PNG header for the image size: the signature and IHDR, then the chunks up to the first IDAT, as libpng's
// png_read_info read them (their CRCs not checked); false when one is missing or cut short
bool PNGDecoder::DoGetAttrib(ImageAttributes& attributes)
{
    static const unsigned char kHead[16]={0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a,0,0,0,13,'I','H','D','R'};
    unsigned char head[33];
    if(this->mFileReader->ReadMaxBytes(head,33)!=33 || memcmp(head,kHead,16)!=0) return false;
    for(;;) {
        unsigned char chunk[8];
        if(this->mFileReader->ReadMaxBytes(chunk,8)!=8) return false;
        if(memcmp(chunk+4,"IDAT",4)==0) break;
        unsigned long length=((unsigned long)chunk[0]<<24)|(chunk[1]<<16)|(chunk[2]<<8)|chunk[3];
        if(this->mFileReader->Available()<length+4) return false;
        this->mFileReader->SkipBytes(length+4);
    }
    attributes.mWidth=(head[16]<<24)|(head[17]<<16)|(head[18]<<8)|head[19];
    attributes.mHeight=(head[20]<<24)|(head[21]<<16)|(head[22]<<8)|head[23];
    return true;
}

// FUNCTION: WON 0x007d2660
// Decodes the PNG into a 32-bit ARGB raw image, transparent when it has alpha or a tRNS chunk; NULL when it is
// not a PNG or does not decode
SmartPtr<RawImage> PNGDecoder::DoDecodeRaw()
{
    std::vector<unsigned char> bytes(this->mFileReader->Available());
    if(!bytes.empty())
        bytes.resize(this->mFileReader->ReadMaxBytes(bytes.data(),(int)bytes.size()));
    EeOfPicture picture;
    EeOfBytes error;
    bool ok=ee_of_png_to_picture(bytes.data(),bytes.size(),&picture,&error);
    ee_of_bytes_free(error);
    if(!ok) return NULL;
    SmartPtr<RawImage32> image=new RawImage32(picture.width,picture.height);
    image->SetDoTransparency(picture.depth==32);
    // B, G, R, A per pixel, top row first: the DWORD (A<<24)|(R<<16)|(G<<8)|B the original stored
    const unsigned char* pixel=picture.bgra.ptr;
    image->StartRowTraversal(0);
    for(unsigned int y=0;y<picture.height;++y) {
        for(unsigned int x=0;x<picture.width;++x,pixel+=4) {
            image->SetRowPixel(pixel[0]|(pixel[1]<<8)|(pixel[2]<<16)|((DWORD)pixel[3]<<24));
            image->NextRowPixel();
        }
        image->NextRow();
    }
    ee_of_bytes_free(picture.bgra);
    return image.get();
}
}
