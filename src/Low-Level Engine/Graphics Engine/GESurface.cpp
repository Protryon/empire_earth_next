// C:\EEX\Low-Level Engine\Graphics Engine\GESurface.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1002534b..0x10026fde: GESurface's image-file members (the class itself is abstract and header-inline; the
// renderer implements it). JPEG goes through the statically linked IJG libjpeg 6b (stdio source/destination on
// FSFileSpec's FOpen stream).
//
// port (R4.3, fifteenth part): the image members are Rust (ee_openformats surface_file.rs: the BMP writer, the JPEG
// and JPG2 readers and writers, the PNG reader, the type switches), in the original's statements. This unit is their
// forwarders and the host: the surface's and the file's virtuals, and libjpeg's calls (its structures stay C: a
// decompressor or compressor is a handle the host allocates).
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UReferenceCounter.h"
#include "lle/FSFileSpec.h"
#include "lle/GESurface.h"
#include "lle/Graphics Engine.h"
#include "rust/ee_openformats.h"   // the image members (surface_file.rs)

extern "C" {
#include "jpeglib/jpeglib.h"
}

// a libjpeg decompressor or compressor with its error manager and one-row buffer
template <typename Info> struct GEJpeg {
    Info mInfo;
    jpeg_error_mgr mError;
    JSAMPARRAY mBuffer;
};

// The calls the image members make on other objects (EeOfSurfOp), in the original's statements
static intptr_t GESurfaceHost(EeOfSurfOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    GESurface* theSurface = (GESurface*)inObj;
    FSFileSpec* theFile = (FSFileSpec*)inObj;
    GEJpeg<jpeg_decompress_struct>* theDecoder = (GEJpeg<jpeg_decompress_struct>*)inObj;
    GEJpeg<jpeg_compress_struct>* theEncoder = (GEJpeg<jpeg_compress_struct>*)inObj;
    unsigned long theWritten;
    switch (inOp) {
    case EeOfSurfOp::Format: return (intptr_t)theSurface->GetPixelFormat();
    case EeOfSurfOp::Depth: return (intptr_t)theSurface->GetPixelDepth();
    case EeOfSurfOp::BytesPerRow: return (intptr_t)theSurface->GetBytesPerRow();
    case EeOfSurfOp::Memory: return (intptr_t)theSurface->GetMemoryAddress();
    case EeOfSurfOp::PixelWidth: return (intptr_t)theSurface->GetPixelWidth();
    case EeOfSurfOp::PixelHeight: return (intptr_t)theSurface->GetPixelHeight();
    case EeOfSurfOp::NoAlpha: return theSurface->GetSurfaceDescription().mNoAlpha;
    case EeOfSurfOp::Lock: return theSurface->Lock();
    case EeOfSurfOp::Unlock: return theSurface->Unlock();
    case EeOfSurfOp::FileIsOpen: return theFile->GetIsOpen();
    case EeOfSurfOp::FileAccess: return (intptr_t)theFile->GetFileAccess();
    case EeOfSurfOp::FileOpen: return theFile->Open((unsigned long)a);
    case EeOfSurfOp::FileClose: return theFile->Close();
    case EeOfSurfOp::FileFOpen: return theFile->FOpen((char*)a);
    case EeOfSurfOp::FileStream: return (intptr_t)(FILE*)*theFile;
    case EeOfSurfOp::FilePosition: return theFile->GetPosition(*(long*)a);
    case EeOfSurfOp::FileSetPosition: return theFile->SetPosition((long)a, (unsigned long)b, c != 0);
    case EeOfSurfOp::FileWrite: return theFile->Write((const unsigned char*)a, (unsigned long)b, theWritten);
    case EeOfSurfOp::FileWriteU16: return theFile->Write((unsigned short)a);
    case EeOfSurfOp::FileWriteLong: return theFile->Write((long)a);
    case EeOfSurfOp::FileRead: return theFile->Read((unsigned char*)a, (unsigned long)b, *(unsigned long*)c);
    case EeOfSurfOp::FileLength: return theFile->GetLength(*(unsigned long*)a);
    case EeOfSurfOp::JpegDecNew: {
        GEJpeg<jpeg_decompress_struct>* theJpeg = new GEJpeg<jpeg_decompress_struct>;
        theJpeg->mInfo.err = jpeg_std_error(&theJpeg->mError);
        jpeg_create_decompress(&theJpeg->mInfo);
        return (intptr_t)theJpeg;
    }
    case EeOfSurfOp::JpegDecStart: {
        jpeg_decompress_struct& theInfo = theDecoder->mInfo;
        jpeg_stdio_src(&theInfo, (FILE*)a);
        jpeg_read_header(&theInfo, TRUE);
        jpeg_start_decompress(&theInfo);
        const uint32_t theSize[3] = {theInfo.output_width, theInfo.output_height, (uint32_t)theInfo.output_components};
        memcpy((void*)b, theSize, sizeof theSize);
        JDIMENSION theRowStride = theInfo.output_width * theInfo.output_components;
        theDecoder->mBuffer = (*theInfo.mem->alloc_sarray)((j_common_ptr)&theInfo, JPOOL_IMAGE, theRowStride, 1);
        return (intptr_t)theRowStride;
    }
    case EeOfSurfOp::JpegDecRow:
        if (theDecoder->mInfo.output_scanline < theDecoder->mInfo.output_height &&
            jpeg_read_scanlines(&theDecoder->mInfo, theDecoder->mBuffer, 1) >= 1)
            return (intptr_t)theDecoder->mBuffer[0];
        return 0;
    case EeOfSurfOp::JpegDecFinish: jpeg_finish_decompress(&theDecoder->mInfo); return 0;
    case EeOfSurfOp::JpegDecDestroy: jpeg_destroy_decompress(&theDecoder->mInfo); delete theDecoder; return 0;
    case EeOfSurfOp::JpegEncNew: {
        GEJpeg<jpeg_compress_struct>* theJpeg = new GEJpeg<jpeg_compress_struct>;
        jpeg_compress_struct& theInfo = theJpeg->mInfo;
        theInfo.err = jpeg_std_error(&theJpeg->mError);
        jpeg_create_compress(&theInfo);
        jpeg_stdio_dest(&theInfo, (FILE*)a);
        theInfo.image_width = (JDIMENSION)b;
        theInfo.image_height = (JDIMENSION)c;
        theInfo.input_components = 3;
        theInfo.in_color_space = JCS_RGB;
        jpeg_set_defaults(&theInfo);
        jpeg_set_quality(&theInfo, 80, TRUE);
        jpeg_start_compress(&theInfo, TRUE);
        theJpeg->mBuffer = (*theInfo.mem->alloc_sarray)((j_common_ptr)&theInfo, JPOOL_IMAGE,
                                                        (JDIMENSION)(b * 3 + b % 4), 1);
        return (intptr_t)theJpeg;
    }
    case EeOfSurfOp::JpegEncBuffer: return (intptr_t)theEncoder->mBuffer[0];
    case EeOfSurfOp::JpegEncRow: jpeg_write_scanlines(&theEncoder->mInfo, theEncoder->mBuffer, 1); return 0;
    case EeOfSurfOp::JpegEncEnd:
        jpeg_finish_compress(&theEncoder->mInfo);
        jpeg_destroy_compress(&theEncoder->mInfo);
        delete theEncoder;
        return 0;
    }
    return 0;
}

// FUNCTION: LLE 0x10026eed
// Reads an image file's width, height and depth by file type (BMP: unsupported, JPG, JPG2, PNG)
long GESurface::GetImageFileAttributes(FSFileSpec& inFile, unsigned char inType, unsigned long& outWidth,
                                       unsigned long& outHeight, unsigned long& outDepth)
{
    unsigned long theOut[3] = {outWidth, outHeight, outDepth};
    long theResult = ee_of_surf_attributes(&GESurfaceHost, &inFile, inType, &theOut);
    outWidth = theOut[0];
    outHeight = theOut[1];
    outDepth = theOut[2];
    return theResult;
}

// FUNCTION: LLE 0x10026f56
// Loads an image file into the surface by file type (BMP: unsupported, JPG, JPG2, PNG; TGA: no longer read, D9)
long GESurface::Load(FSFileSpec& inFile, unsigned char inType)
{
    return ee_of_surf_load(&GESurfaceHost, this, &inFile, inType);
}

// FUNCTION: LLE 0x10026f9a
// Saves the surface as an image file of the given type (BMP, JPG, JPG2)
long GESurface::Save(FSFileSpec& inFile, unsigned char inType)
{
    return ee_of_surf_save(&GESurfaceHost, this, &inFile, inType);
}
