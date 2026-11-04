// C:\EEX\Low-Level Engine\Graphics Engine\GESurface.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1002534b..0x10026fde: GESurface's image-file members (the class itself is abstract and header-inline; the
// renderer plug-ins implement it). Order: BMP, JPG, JPG2, TGA, then the type switches. JPEG goes through the
// statically linked IJG libjpeg 6b (stdio source/destination on FSFileSpec's FOpen stream). No init chains.
// GetBMPFileAttributes lives elsewhere (0x1003fd29).
//
// JPG2 (provisional names) = two JPEGs behind a two-word offset header (_putw/_getw): the colour image, then an
// image carrying the alpha in all three channels (LoadJPG2 merges both). SaveAsJPG2 writes placeholder offsets
// and patches them through FSFileSpec::Write after reopening; the TGA writer swaps the header width/height (as shipped).
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UReferenceCounter.h"
#include "lle/FSFileSpec.h"
#include "lle/GESurface.h"
#include "lle/Graphics Engine.h"

extern "C" {
#include "jpeglib/jpeglib.h"
}

// Targa file header (18 bytes, packed) (provisional names)
#include <pshpack1.h>
struct GETGAHeader {
    unsigned char mIDLength;            // +0x00
    unsigned char mColorMapType;        // +0x01
    unsigned char mImageType;           // +0x02: 2 = uncompressed true colour
    unsigned short mColorMapOrigin;     // +0x03
    unsigned short mColorMapLength;     // +0x05
    unsigned char mColorMapDepth;       // +0x07
    unsigned short mXOrigin;            // +0x08
    unsigned short mYOrigin;            // +0x0a
    unsigned short mWidth;              // +0x0c
    unsigned short mHeight;             // +0x0e
    unsigned char mPixelDepth;          // +0x10
    unsigned char mDescriptor;          // +0x11: bit 4 right-to-left, bit 5 top-down
};
#include <poppack.h>

// FUNCTION: LLE 0x1002534b
// Always fails: loading BMP files is not supported
long GESurface::LoadBMP(FSFileSpec& inFile)
{
    return kErrorFailed;
}

// FUNCTION: LLE 0x10025351
// Saves the surface as a bottom-up 24-bit BMP
// NEAR: register/slot choice (the original keeps theHeight in ebx from the biHeight store to the row start, ours
// theBytesPerRow); declaration orders and pointer-expression shapes tried
long GESurface::SaveAsBMP(FSFileSpec& inFile)
{
    unsigned long theFormat = this->GetPixelFormat();
    unsigned long theWidth = this->GetWidth();
    unsigned long theHeight = this->GetHeight();
    bool theWasOpen = inFile.GetIsOpen();
    long theResult;
    BITMAPFILEHEADER theFileHeader;
    BITMAPINFOHEADER theInfoHeader;
    unsigned long theRowSize;
    unsigned char* theBuffer;
    unsigned long theWritten;
    unsigned long theBytesPerRow;
    unsigned long thePitch;
    unsigned long* the32BitPixel;
    unsigned short* the16BitPixel;
    unsigned long x;
    unsigned long y;

    if (theWasOpen) {
        if (inFile.GetFileAccess() != kFileAccessWrite)
            theResult = kErrorFailed;
        else
            theResult = kErrorOK;
    } else
        theResult = inFile.Open(kFileAccessWrite);
    if (theResult == kErrorOK) {
        theRowSize = theWidth * 3 + theWidth % 4;
        theBuffer = new unsigned char[theRowSize];
        theFileHeader.bfType = 0x4d42;
        // The file holds theHeight rows (this used the width) plus the two trailing bytes written after the pixels.
        theFileHeader.bfSize = theRowSize * theHeight + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 2;
        theFileHeader.bfReserved1 = 0;
        theFileHeader.bfReserved2 = 0;
        theFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        theResult = inFile.Write((unsigned char*)&theFileHeader, sizeof(BITMAPFILEHEADER), theWritten);
        if (theResult == kErrorOK) {
            theInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
            theInfoHeader.biWidth = theWidth;
            theInfoHeader.biHeight = theHeight;
            theInfoHeader.biPlanes = 1;
            theInfoHeader.biBitCount = 24;
            theInfoHeader.biCompression = 0;
            theInfoHeader.biSizeImage = 0;
            theInfoHeader.biXPelsPerMeter = 2834;
            theInfoHeader.biYPelsPerMeter = 2834;
            theInfoHeader.biClrUsed = 0;
            theInfoHeader.biClrImportant = 0;
            theResult = inFile.Write((unsigned char*)&theInfoHeader, sizeof(BITMAPINFOHEADER), theWritten);
            if (theResult == kErrorOK) {
                theResult = this->Lock();
                if (theResult == kErrorOK) {
                    theBytesPerRow = this->GetBytesPerRow();
                    if (this->GetPixelDepth() == 32) {
                        thePitch = theBytesPerRow / 4;
                        the32BitPixel = reinterpret_cast<unsigned long*>(this->GetMemoryAddress()) + (theHeight - 1) * thePitch;
                        for (y = 0; y < theHeight; y++) {
                            for (x = 0; x < theWidth; x++) {
                                theBuffer[x * 3 + 2] = GEUnpackRed(theFormat, *the32BitPixel);
                                theBuffer[x * 3 + 1] = GEUnpackGreen(theFormat, *the32BitPixel);
                                theBuffer[x * 3] = GEUnpackBlue(theFormat, *the32BitPixel);
                                the32BitPixel++;
                            }
                            inFile.Write(theBuffer, theRowSize, theWritten);
                            the32BitPixel -= thePitch + theWidth;
                        }
                    } else {
                        thePitch = theBytesPerRow / 2;
                        the16BitPixel = reinterpret_cast<unsigned short*>(this->GetMemoryAddress()) + (theHeight - 1) * thePitch;
                        for (y = 0; y < theHeight; y++) {
                            for (x = 0; x < theWidth; x++) {
                                theBuffer[x * 3 + 2] = GEUnpackRed(theFormat, *the16BitPixel);
                                theBuffer[x * 3 + 1] = GEUnpackGreen(theFormat, *the16BitPixel);
                                theBuffer[x * 3] = GEUnpackBlue(theFormat, *the16BitPixel);
                                the16BitPixel++;
                            }
                            inFile.Write(theBuffer, theRowSize, theWritten);
                            the16BitPixel -= thePitch + theWidth;
                        }
                    }
                    this->Unlock();
                    theResult = inFile.Write((unsigned short)0);
                }
            }
        }
        delete theBuffer;
        if (!theWasOpen)
            theResult = inFile.Close();
    }
    return theResult;
}

// FUNCTION: LLE 0x100255ff
// Reads a JPEG file's width, height and bit depth (closing the file afterwards)
long GESurface::GetJPGFileAttributes(FSFileSpec& inFile, unsigned long& outWidth, unsigned long& outHeight,
                                     unsigned long& outDepth)
{
    struct jpeg_decompress_struct theInfo;
    struct jpeg_error_mgr theError;
    FILE* theStream;
    long theResult = inFile.FOpen("rb");
    if (theResult != kErrorOK)
        return theResult;
    theInfo.err = jpeg_std_error(&theError);
    jpeg_create_decompress(&theInfo);
    theStream = inFile;
    if (theStream) {
        jpeg_stdio_src(&theInfo, theStream);
        jpeg_read_header(&theInfo, TRUE);
        jpeg_start_decompress(&theInfo);
        outWidth = theInfo.output_width;
        outHeight = theInfo.output_height;
        outDepth = theInfo.output_components * 8;
        jpeg_destroy_decompress(&theInfo);
    }
    theResult = inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x100256b8
// Reads a JPG2 file's size from its colour image and reports 32 bits (colour plus alpha)
long GESurface::GetJPG2FileAttributes(FSFileSpec& inFile, unsigned long& outWidth, unsigned long& outHeight,
                                      unsigned long& outDepth)
{
    long theResult = GESurface::GetJPGFileAttributes(inFile, outWidth, outHeight, outDepth);
    outDepth = 32;
    return theResult;
}

// FUNCTION: LLE 0x100256da
// Decodes a JPEG into the surface (opaque), from the file's current position; reopens a file that was open
long GESurface::LoadJPG(FSFileSpec& inFile)
{
    unsigned long* the32BitPixel = NULL;
    unsigned short* the16BitPixel = NULL;
    unsigned long theFormat = this->GetPixelFormat();
    long thePosition = 0;
    bool theWasOpen;
    long theResult;
    FILE* theStream;
    fpos_t theFilePosition;
    struct jpeg_decompress_struct theInfo;
    struct jpeg_error_mgr theError;
    JSAMPARRAY theBuffer;
    unsigned long theRowStride;
    unsigned long theSkip;
    unsigned long theIndex;
    unsigned char theRed;
    unsigned char theGreen;
    unsigned char theBlue;

    theWasOpen = inFile.GetIsOpen();
    if (theWasOpen && inFile.GetPosition(thePosition) == kErrorOK)
        inFile.Close();
    theResult = inFile.FOpen("rb");
    if (theResult != kErrorOK)
        return theResult;
    theStream = inFile;
    if (theStream) {
        theFilePosition = thePosition;
        fsetpos(theStream, &theFilePosition);
        theInfo.err = jpeg_std_error(&theError);
        jpeg_create_decompress(&theInfo);
        if (this->Lock() == kErrorOK) {
            jpeg_stdio_src(&theInfo, theStream);
            jpeg_read_header(&theInfo, TRUE);
            jpeg_start_decompress(&theInfo);
            theRowStride = theInfo.output_width * theInfo.output_components;
            theBuffer = (*theInfo.mem->alloc_sarray)((j_common_ptr)&theInfo, JPOOL_IMAGE, theRowStride, 1);
            if (this->GetPixelDepth() == 32) {
                theSkip = (this->GetBytesPerRow() - this->GetWidth() * 4) / 4;
                the32BitPixel = reinterpret_cast<unsigned long*>(this->GetMemoryAddress());
            } else {
                theSkip = (this->GetBytesPerRow() - this->GetWidth() * 2) / 2;
                the16BitPixel = reinterpret_cast<unsigned short*>(this->GetMemoryAddress());
            }
            while (theInfo.output_scanline < theInfo.output_height) {
                if (jpeg_read_scanlines(&theInfo, theBuffer, 1) < 1)
                    break;
                for (theIndex = 0; theIndex < theRowStride; theIndex += 3) {
                    theRed = theBuffer[0][theIndex];
                    theGreen = theBuffer[0][theIndex + 1];
                    theBlue = theBuffer[0][theIndex + 2];
                    if (this->GetPixelDepth() == 32)
                        *the32BitPixel++ = 0xff000000 | (theRed << 16) | (theGreen << 8) | theBlue;
                    else
                        *the16BitPixel++ = (unsigned short)GEPackColor(theFormat, theRed, theGreen, theBlue, 0xff);
                }
                if (this->GetPixelDepth() == 32)
                    the32BitPixel += theSkip;
                else
                    the16BitPixel += theSkip;
            }
            jpeg_finish_decompress(&theInfo);
            jpeg_destroy_decompress(&theInfo);
            this->Unlock();
        }
    }
    if (theWasOpen) {
        fgetpos(theStream, &theFilePosition);
        theResult = inFile.Close();
        if (theResult == kErrorOK) {
            theResult = inFile.Open(kFileAccessRead);
            if (theResult == kErrorOK) {
                thePosition = (long)theFilePosition;
                theResult = inFile.SetPosition(thePosition, kFileOriginBegin, false);
            }
        }
    } else
        theResult = inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x1002599d
// Decodes a JPG2 into the surface: the colour image, then the alpha image merged into each pixel's alpha
long GESurface::LoadJPG2(FSFileSpec& inFile)
{
    unsigned long* the32BitPixel = NULL;
    unsigned short* the16BitPixel = NULL;
    unsigned long theFormat = this->GetPixelFormat();
    long thePosition = 0;
    long theAlphaPosition;
    long theEndPosition = 0;
    bool theWasOpen;
    long theResult;
    FILE* theStream;
    struct jpeg_decompress_struct theInfo;
    struct jpeg_error_mgr theError;
    JSAMPARRAY theBuffer;
    unsigned long theRowStride;
    unsigned long theSkip;
    unsigned long theIndex;
    unsigned char theRed;
    unsigned char theGreen;
    unsigned char theBlue;
    unsigned char theAlpha;

    theWasOpen = inFile.GetIsOpen();
    if (theWasOpen && inFile.GetPosition(thePosition) == kErrorOK)
        inFile.Close();
    theResult = inFile.FOpen("rb");
    if (theResult != kErrorOK)
        return theResult;
    theStream = inFile;
    if (theStream) {
        fseek(theStream, thePosition, SEEK_SET);
        theAlphaPosition = _getw(theStream);
        theEndPosition = _getw(theStream);
        theInfo.err = jpeg_std_error(&theError);
        jpeg_create_decompress(&theInfo);
        if (this->Lock() == kErrorOK) {
            jpeg_stdio_src(&theInfo, theStream);
            jpeg_read_header(&theInfo, TRUE);
            jpeg_start_decompress(&theInfo);
            theRowStride = theInfo.output_width * theInfo.output_components;
            theBuffer = (*theInfo.mem->alloc_sarray)((j_common_ptr)&theInfo, JPOOL_IMAGE, theRowStride, 1);
            if (this->GetPixelDepth() == 32) {
                theSkip = (this->GetBytesPerRow() - this->GetWidth() * 4) / 4;
                the32BitPixel = reinterpret_cast<unsigned long*>(this->GetMemoryAddress());
            } else {
                theSkip = (this->GetBytesPerRow() - this->GetWidth() * 2) / 2;
                the16BitPixel = reinterpret_cast<unsigned short*>(this->GetMemoryAddress());
            }
            while (theInfo.output_scanline < theInfo.output_height) {
                if (jpeg_read_scanlines(&theInfo, theBuffer, 1) < 1)
                    break;
                for (theIndex = 0; theIndex < theRowStride; theIndex += 3) {
                    theRed = theBuffer[0][theIndex];
                    theGreen = theBuffer[0][theIndex + 1];
                    theBlue = theBuffer[0][theIndex + 2];
                    if (this->GetPixelDepth() == 32)
                        *the32BitPixel++ = 0xff000000 | (theRed << 16) | (theGreen << 8) | theBlue;
                    else
                        *the16BitPixel++ = (unsigned short)GEPackColor(theFormat, theRed, theGreen, theBlue, 0xff);
                }
                if (this->GetPixelDepth() == 32)
                    the32BitPixel += theSkip;
                else
                    the16BitPixel += theSkip;
            }
            jpeg_finish_decompress(&theInfo);
            jpeg_destroy_decompress(&theInfo);

            fseek(theStream, theAlphaPosition, SEEK_SET);
            theInfo.err = jpeg_std_error(&theError);
            jpeg_create_decompress(&theInfo);
            jpeg_stdio_src(&theInfo, theStream);
            jpeg_read_header(&theInfo, TRUE);
            jpeg_start_decompress(&theInfo);
            theRowStride = theInfo.output_width * theInfo.output_components;
            theBuffer = (*theInfo.mem->alloc_sarray)((j_common_ptr)&theInfo, JPOOL_IMAGE, theRowStride, 1);
            if (this->GetPixelDepth() == 32) {
                theSkip = (this->GetBytesPerRow() - this->GetWidth() * 4) / 4;
                the32BitPixel = reinterpret_cast<unsigned long*>(this->GetMemoryAddress());
            } else {
                theSkip = (this->GetBytesPerRow() - this->GetWidth() * 2) / 2;
                the16BitPixel = reinterpret_cast<unsigned short*>(this->GetMemoryAddress());
            }
            while (theInfo.output_scanline < theInfo.output_height) {
                if (jpeg_read_scanlines(&theInfo, theBuffer, 1) < 1)
                    break;
                for (theIndex = 0; theIndex < theRowStride; theIndex += 3) {
                    theAlpha = theBuffer[0][theIndex];
                    if (this->GetPixelDepth() == 32) {
                        theRed = GEUnpackRed(theFormat, *the32BitPixel);
                        theGreen = GEUnpackGreen(theFormat, *the32BitPixel);
                        theBlue = GEUnpackBlue(theFormat, *the32BitPixel);
                        *the32BitPixel++ = (theAlpha << 24) | (theRed << 16) | (theGreen << 8) | theBlue;
                    } else {
                        theRed = GEUnpackRed(theFormat, *the16BitPixel);
                        theGreen = GEUnpackGreen(theFormat, *the16BitPixel);
                        theBlue = GEUnpackBlue(theFormat, *the16BitPixel);
                        *the16BitPixel++ = (unsigned short)GEPackColor(theFormat, theRed, theGreen, theBlue,
                                                                       theAlpha);
                    }
                }
                if (this->GetPixelDepth() == 32)
                    the32BitPixel += theSkip;
                else
                    the16BitPixel += theSkip;
            }
            jpeg_finish_decompress(&theInfo);
            jpeg_destroy_decompress(&theInfo);
            this->Unlock();
        }
    }
    if (theWasOpen) {
        theResult = inFile.Close();
        if (theResult == kErrorOK) {
            theResult = inFile.Open(kFileAccessRead);
            if (theResult == kErrorOK)
                theResult = inFile.SetPosition(theEndPosition, kFileOriginBegin, false);
        }
    } else
        theResult = inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x10025e6f
// Encodes the surface as a quality-80 JPEG, appending at the position of a file that was already open
long GESurface::SaveAsJPG(FSFileSpec& inFile)
{
    unsigned long theFormat = this->GetPixelFormat();
    unsigned long theWidth = this->GetWidth();
    unsigned long theHeight = this->GetHeight();
    long thePosition = 0;
    bool theWasOpen;
    long theResult;
    FILE* theStream;
    fpos_t theFilePosition;
    struct jpeg_compress_struct theInfo;
    struct jpeg_error_mgr theError;
    JSAMPARRAY theBuffer;
    unsigned long theBytesPerRow;
    unsigned long* the32BitPixel;
    unsigned short* the16BitPixel;
    unsigned long x;
    unsigned long y;

    theWasOpen = inFile.GetIsOpen();
    if (theWasOpen) {
        theResult = inFile.GetPosition(thePosition);
        if (theResult == kErrorOK)
            theResult = inFile.Close();
        if (theResult == kErrorOK)
            theResult = inFile.FOpen("ab");
    } else
        theResult = inFile.FOpen("wb");
    if (theResult != kErrorOK)
        return theResult;
    theStream = inFile;
    if (theStream) {
        theFilePosition = thePosition;
        fsetpos(theStream, &theFilePosition);
        theInfo.err = jpeg_std_error(&theError);
        jpeg_create_compress(&theInfo);
        jpeg_stdio_dest(&theInfo, theStream);
        theInfo.image_width = theWidth;
        theInfo.image_height = theHeight;
        theInfo.input_components = 3;
        theInfo.in_color_space = JCS_RGB;
        jpeg_set_defaults(&theInfo);
        jpeg_set_quality(&theInfo, 80, TRUE);
        jpeg_start_compress(&theInfo, TRUE);
        theBuffer = (*theInfo.mem->alloc_sarray)((j_common_ptr)&theInfo, JPOOL_IMAGE,
                                                 theWidth * 3 + theWidth % 4, 1);
        if (this->Lock() == kErrorOK) {
            theBytesPerRow = this->GetBytesPerRow();
            if (this->GetPixelDepth() == 32) {
                for (y = 0; y < theHeight; y++) {
                    the32BitPixel = reinterpret_cast<unsigned long*>(this->GetMemoryAddress()) + y * (theBytesPerRow / 4);
                    for (x = 0; x < theWidth; x++) {
                        theBuffer[0][x * 3] = GEUnpackRed(theFormat, *the32BitPixel);
                        theBuffer[0][x * 3 + 1] = GEUnpackGreen(theFormat, *the32BitPixel);
                        theBuffer[0][x * 3 + 2] = GEUnpackBlue(theFormat, *the32BitPixel);
                        the32BitPixel++;
                    }
                    jpeg_write_scanlines(&theInfo, theBuffer, 1);
                }
            } else {
                for (y = 0; y < theHeight; y++) {
                    the16BitPixel = reinterpret_cast<unsigned short*>(this->GetMemoryAddress()) + y * (theBytesPerRow / 2);
                    for (x = 0; x < theWidth; x++) {
                        theBuffer[0][x * 3] = GEUnpackRed(theFormat, *the16BitPixel);
                        theBuffer[0][x * 3 + 1] = GEUnpackGreen(theFormat, *the16BitPixel);
                        theBuffer[0][x * 3 + 2] = GEUnpackBlue(theFormat, *the16BitPixel);
                        the16BitPixel++;
                    }
                    jpeg_write_scanlines(&theInfo, theBuffer, 1);
                }
            }
        }
        this->Unlock();
        jpeg_finish_compress(&theInfo);
        jpeg_destroy_compress(&theInfo);
    }
    if (theWasOpen) {
        fgetpos(theStream, &theFilePosition);
        theResult = inFile.Close();
        if (theResult == kErrorOK) {
            theResult = inFile.Open(kFileAccessAppend);
            if (theResult == kErrorOK) {
                thePosition = (long)theFilePosition;
                theResult = inFile.SetPosition(thePosition, kFileOriginBegin, false);
            }
        }
    } else
        theResult = inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x1002619b
// Encodes the surface as JPG2 (colour JPEG plus alpha JPEG) and patches the two offsets into its header
long GESurface::SaveAsJPG2(FSFileSpec& inFile)
{
    unsigned long theFormat = this->GetPixelFormat();
    unsigned long theWidth = this->GetWidth();
    unsigned long theHeight = this->GetHeight();
    long thePosition = 0;
    long theAlphaPosition = 0;
    long theEndPosition = 0;
    bool theWasOpen;
    long theResult;
    FILE* theStream;
    struct jpeg_compress_struct theInfo;
    struct jpeg_error_mgr theError;
    JSAMPARRAY theBuffer;
    unsigned long theRowSize;
    unsigned long theBytesPerRow;
    unsigned long* the32BitPixel;
    unsigned short* the16BitPixel;
    unsigned long x;
    unsigned long y;

    theWasOpen = inFile.GetIsOpen();
    if (theWasOpen) {
        theResult = inFile.GetPosition(thePosition);
        if (theResult == kErrorOK)
            theResult = inFile.Close();
        if (theResult == kErrorOK)
            theResult = inFile.FOpen("ab");
    } else
        theResult = inFile.FOpen("wb");
    if (theResult != kErrorOK)
        return theResult;
    theStream = inFile;
    if (theStream) {
        _putw(theAlphaPosition, theStream);
        _putw(theEndPosition, theStream);

        theInfo.err = jpeg_std_error(&theError);
        jpeg_create_compress(&theInfo);
        jpeg_stdio_dest(&theInfo, theStream);
        theInfo.image_width = theWidth;
        theInfo.image_height = theHeight;
        theInfo.input_components = 3;
        theInfo.in_color_space = JCS_RGB;
        jpeg_set_defaults(&theInfo);
        jpeg_set_quality(&theInfo, 80, TRUE);
        jpeg_start_compress(&theInfo, TRUE);
        theRowSize = theWidth * 3 + theWidth % 4;
        theBuffer = (*theInfo.mem->alloc_sarray)((j_common_ptr)&theInfo, JPOOL_IMAGE, theRowSize, 1);
        if (this->Lock() == kErrorOK) {
            theBytesPerRow = this->GetBytesPerRow();
            if (this->GetPixelDepth() == 32) {
                for (y = 0; y < theHeight; y++) {
                    the32BitPixel = reinterpret_cast<unsigned long*>(this->GetMemoryAddress()) + y * (theBytesPerRow / 4);
                    for (x = 0; x < theWidth; x++) {
                        theBuffer[0][x * 3] = GEUnpackRed(theFormat, *the32BitPixel);
                        theBuffer[0][x * 3 + 1] = GEUnpackGreen(theFormat, *the32BitPixel);
                        theBuffer[0][x * 3 + 2] = GEUnpackBlue(theFormat, *the32BitPixel);
                        the32BitPixel++;
                    }
                    jpeg_write_scanlines(&theInfo, theBuffer, 1);
                }
            } else {
                for (y = 0; y < theHeight; y++) {
                    the16BitPixel = reinterpret_cast<unsigned short*>(this->GetMemoryAddress()) + y * (theBytesPerRow / 2);
                    for (x = 0; x < theWidth; x++) {
                        theBuffer[0][x * 3] = GEUnpackRed(theFormat, *the16BitPixel);
                        theBuffer[0][x * 3 + 1] = GEUnpackGreen(theFormat, *the16BitPixel);
                        theBuffer[0][x * 3 + 2] = GEUnpackBlue(theFormat, *the16BitPixel);
                        the16BitPixel++;
                    }
                    jpeg_write_scanlines(&theInfo, theBuffer, 1);
                }
            }
        }
        jpeg_finish_compress(&theInfo);
        jpeg_destroy_compress(&theInfo);
        theAlphaPosition = ftell(theStream);

        theInfo.err = jpeg_std_error(&theError);
        jpeg_create_compress(&theInfo);
        jpeg_stdio_dest(&theInfo, theStream);
        theInfo.image_width = theWidth;
        theInfo.image_height = theHeight;
        theInfo.input_components = 3;
        theInfo.in_color_space = JCS_RGB;
        jpeg_set_defaults(&theInfo);
        jpeg_set_quality(&theInfo, 80, TRUE);
        jpeg_start_compress(&theInfo, TRUE);
        theBuffer = (*theInfo.mem->alloc_sarray)((j_common_ptr)&theInfo, JPOOL_IMAGE, theRowSize, 1);
        theBytesPerRow = this->GetBytesPerRow();
        if (this->GetPixelDepth() == 32) {
            for (y = 0; y < theHeight; y++) {
                the32BitPixel = reinterpret_cast<unsigned long*>(this->GetMemoryAddress()) + y * (theBytesPerRow / 4);
                for (x = 0; x < theWidth; x++) {
                    theBuffer[0][x * 3] = GEUnpackAlpha(theFormat, *the32BitPixel);
                    theBuffer[0][x * 3 + 1] = GEUnpackAlpha(theFormat, *the32BitPixel);
                    theBuffer[0][x * 3 + 2] = GEUnpackAlpha(theFormat, *the32BitPixel);
                    the32BitPixel++;
                }
                jpeg_write_scanlines(&theInfo, theBuffer, 1);
            }
        } else {
            for (y = 0; y < theHeight; y++) {
                the16BitPixel = reinterpret_cast<unsigned short*>(this->GetMemoryAddress()) + y * (theBytesPerRow / 2);
                for (x = 0; x < theWidth; x++) {
                    theBuffer[0][x * 3] = GEUnpackAlpha(theFormat, *the16BitPixel);
                    theBuffer[0][x * 3 + 1] = GEUnpackAlpha(theFormat, *the16BitPixel);
                    theBuffer[0][x * 3 + 2] = GEUnpackAlpha(theFormat, *the16BitPixel);
                    the16BitPixel++;
                }
                jpeg_write_scanlines(&theInfo, theBuffer, 1);
            }
        }
        this->Unlock();
        jpeg_finish_compress(&theInfo);
        jpeg_destroy_compress(&theInfo);
        theEndPosition = ftell(theStream);
    }
    if (theWasOpen) {
        theResult = inFile.Close();
        if (theResult == kErrorOK) {
            theResult = inFile.Open(kFileAccessReadWrite);
            if (theResult == kErrorOK) {
                theResult = inFile.SetPosition(thePosition, kFileOriginBegin, false);
                if (theResult == kErrorOK) {
                    theResult = inFile.Write(theAlphaPosition);
                    if (theResult == kErrorOK) {
                        theResult = inFile.Write(theEndPosition);
                        if (theResult == kErrorOK)
                            theResult = inFile.SetPosition(theEndPosition, kFileOriginBegin, false);
                    }
                }
            }
        }
    } else {
        theResult = inFile.Close();
        if (theResult == kErrorOK) {
            theResult = inFile.Open(kFileAccessReadWrite);
            if (theResult == kErrorOK) {
                theResult = inFile.SetPosition(thePosition, kFileOriginBegin, false);
                if (theResult == kErrorOK) {
                    theResult = inFile.Write(theAlphaPosition);
                    if (theResult == kErrorOK)
                        theResult = inFile.Write(theEndPosition);
                }
                theResult = inFile.Close();
            }
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x100267a1
// Reads a Targa file's width, height and bit depth from its header
// The original reads inFile+0x34 directly here, in LoadTGA, SaveAsTGA and SaveAsBMP (GetFileAccess is inline)
long GESurface::GetTGAFileAttributes(FSFileSpec& inFile, unsigned long& outWidth, unsigned long& outHeight,
                                     unsigned long& outDepth)
{
    GETGAHeader theHeader;
    unsigned long theRead;
    bool theWasOpen = inFile.GetIsOpen();
    long theResult;

    if (theWasOpen) {
        if (inFile.GetFileAccess() != kFileAccessRead)
            return kErrorFailed;
    } else {
        theResult = inFile.Open(kFileAccessRead);
        if (theResult != kErrorOK)
            return theResult;
    }
    theResult = inFile.Read(reinterpret_cast<unsigned char*>(&theHeader), sizeof(GETGAHeader), theRead);
    if (theResult == kErrorOK && theRead == sizeof(GETGAHeader)) {
        outWidth = theHeader.mWidth;
        outHeight = theHeader.mHeight;
        outDepth = theHeader.mPixelDepth;
    }
    if (!theWasOpen)
        theResult = inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x10026816
// Loads an uncompressed 24/32-bit Targa into the surface, honouring its row and column order; short reads succeed
// NEAR: register allocation rotated (original this=ebx, inFile=esi, bytes-per-pixel=edi; ours esi/edi/ebx), which
// moves every spill; theSource/theData order and theSize reuse tried
long GESurface::LoadTGA(FSFileSpec& inFile)
{
    unsigned long theFormat = this->GetPixelFormat();
    bool theNoAlpha = this->GetSurfaceDescription().mNoAlpha;
    bool theWasOpen = inFile.GetIsOpen();
    long theResult;
    GETGAHeader theHeader;
    unsigned long theRead;
    unsigned long theBytesPerPixel;
    bool theLeftToRight;
    bool theTopDown;
    unsigned long theSize;
    unsigned char* theData;
    unsigned char* theSource;
    unsigned long theBytesPerRow;
    unsigned long theSourceRowSize;
    unsigned long theSkip;
    unsigned long* the32BitPixel;
    unsigned short* the16BitPixel;
    unsigned char theRed;
    unsigned char theGreen;
    unsigned char theBlue;
    unsigned char theAlpha;
    unsigned long x;
    unsigned long y;

    if (theWasOpen) {
        if (inFile.GetFileAccess() != kFileAccessRead)
            theResult = kErrorFailed;
        else
            theResult = kErrorOK;
    } else
        theResult = inFile.Open(kFileAccessRead);
    if (theResult == kErrorOK) {
        theResult = inFile.Read(reinterpret_cast<unsigned char*>(&theHeader), sizeof(GETGAHeader), theRead);
        theBytesPerPixel = theHeader.mPixelDepth / 8;
        theLeftToRight = (theHeader.mDescriptor & kTGADescriptorRightToLeft) == 0;
        theTopDown = (theHeader.mDescriptor & kTGADescriptorTopToBottom) != 0;
        if (theResult == kErrorOK && theRead == sizeof(GETGAHeader)) {
            theResult = inFile.SetPosition(theHeader.mIDLength, kFileOriginCurrent, true);
            if (theResult == kErrorOK) {
                theSize = theHeader.mPixelDepth / 8 * theHeader.mWidth * theHeader.mHeight;
                theSource = new unsigned char[theSize];
                if (theSource) {
                    theData = theSource;
                    theResult = inFile.Read(theData, theSize, theRead);
                    if (theResult == kErrorOK && theRead == theSize) {
                        theResult = this->Lock();
                        if (theResult == kErrorOK) {
                            theBytesPerRow = this->GetBytesPerRow();
                            theSourceRowSize = theHeader.mWidth * theBytesPerPixel;
                            if (!theTopDown)
                                theSource += (theHeader.mHeight - 1) * theSourceRowSize;
                            if (!theLeftToRight)
                                theSource += theSourceRowSize - theBytesPerPixel;
                            if (this->GetPixelDepth() == 32) {
                                theSkip = (theBytesPerRow - this->GetWidth() * 4) / 4;
                                the32BitPixel = reinterpret_cast<unsigned long*>(this->GetMemoryAddress());
                                if (theBytesPerPixel == 4) {
                                    for (y = 0; y < theHeader.mHeight; y++) {
                                        for (x = 0; x < theHeader.mWidth; x++) {
                                            theBlue = theSource[0];
                                            theGreen = theSource[1];
                                            theRed = theSource[2];
                                            theAlpha = theSource[3];
                                            if (theNoAlpha)
                                                theAlpha = 0xff;
                                            if (theLeftToRight)
                                                theSource += 4;
                                            else
                                                theSource -= 4;
                                            *the32BitPixel++ = (theAlpha << 24) | (theRed << 16) | (theGreen << 8) |
                                                               theBlue;
                                        }
                                        the32BitPixel += theSkip;
                                        if (!theLeftToRight)
                                            theSource += theSourceRowSize * 2;
                                        if (!theTopDown)
                                            theSource -= theSourceRowSize * 2;
                                    }
                                } else {
                                    for (y = 0; y < theHeader.mHeight; y++) {
                                        for (x = 0; x < theHeader.mWidth; x++) {
                                            theBlue = theSource[0];
                                            theGreen = theSource[1];
                                            theRed = theSource[2];
                                            if (theLeftToRight)
                                                theSource += 3;
                                            else
                                                theSource -= 3;
                                            *the32BitPixel++ = 0xff000000 | (theRed << 16) | (theGreen << 8) |
                                                               theBlue;
                                        }
                                        the32BitPixel += theSkip;
                                        if (!theLeftToRight)
                                            theSource += theSourceRowSize * 2;
                                        if (!theTopDown)
                                            theSource -= theSourceRowSize * 2;
                                    }
                                }
                            } else {
                                theSkip = (theBytesPerRow - this->GetWidth() * 2) / 2;
                                the16BitPixel = reinterpret_cast<unsigned short*>(this->GetMemoryAddress());
                                if (theBytesPerPixel == 4) {
                                    for (y = 0; y < theHeader.mHeight; y++) {
                                        for (x = 0; x < theHeader.mWidth; x++) {
                                            theBlue = theSource[0];
                                            theGreen = theSource[1];
                                            theRed = theSource[2];
                                            theAlpha = theSource[3];
                                            if (theNoAlpha)
                                                theAlpha = 0xff;
                                            if (theLeftToRight)
                                                theSource += 4;
                                            else
                                                theSource -= 4;
                                            *the16BitPixel++ = (unsigned short)GEPackColor(theFormat, theRed,
                                                                                           theGreen, theBlue,
                                                                                           theAlpha);
                                        }
                                        the16BitPixel += theSkip;
                                        if (!theLeftToRight)
                                            theSource += theSourceRowSize * 2;
                                        if (!theTopDown)
                                            theSource -= theSourceRowSize * 2;
                                    }
                                } else {
                                    for (y = 0; y < theHeader.mHeight; y++) {
                                        for (x = 0; x < theHeader.mWidth; x++) {
                                            theBlue = theSource[0];
                                            theGreen = theSource[1];
                                            theRed = theSource[2];
                                            if (theLeftToRight)
                                                theSource += 3;
                                            else
                                                theSource -= 3;
                                            *the16BitPixel++ = (unsigned short)GEPackColor(theFormat, theRed,
                                                                                           theGreen, theBlue, 0xff);
                                        }
                                        the16BitPixel += theSkip;
                                        if (!theLeftToRight)
                                            theSource += theSourceRowSize * 2;
                                        if (!theTopDown)
                                            theSource -= theSourceRowSize * 2;
                                    }
                                }
                            }
                            theResult = this->Unlock();
                        }
                    }
                    delete[] theData;
                } else
                    theResult = kErrorNativeError;
            }
        }
        if (!theWasOpen)
            theResult = inFile.Close();
    }
    return theResult;
}

// FUNCTION: LLE 0x10026c57
// Saves the surface as a bottom-up 24-bit Targa
// NEAR: the original loads theHeight into cx before the zero stores and puts theResult in the pixel pointer's slot
// (-4); field order and chained zero stores tried
long GESurface::SaveAsTGA(FSFileSpec& inFile)
{
    unsigned long theFormat = this->GetPixelFormat();
    unsigned long theWidth = this->GetWidth();
    unsigned long theHeight = this->GetHeight();
    bool theWasOpen = inFile.GetIsOpen();
    long theResult;
    GETGAHeader theHeader;
    unsigned long theRowSize;
    unsigned char* theBuffer;
    unsigned long theWritten;
    unsigned long theBytesPerRow;
    unsigned long thePitch;
    unsigned long* the32BitPixel;
    unsigned short* the16BitPixel;
    unsigned long x;
    unsigned long y;

    if (theWasOpen) {
        if (inFile.GetFileAccess() != kFileAccessWrite)
            theResult = kErrorFailed;
        else
            theResult = kErrorOK;
    } else
        theResult = inFile.Open(kFileAccessWrite);
    if (theResult == kErrorOK) {
        theHeader.mYOrigin = theHeader.mXOrigin = theHeader.mColorMapOrigin = theHeader.mColorMapLength = 0;
        theHeader.mDescriptor = theHeader.mIDLength = theHeader.mColorMapType = theHeader.mColorMapDepth = 0;
        // Width and height go to their own fields, and Targa rows are not padded (both were BMP habits).
        theHeader.mWidth = (unsigned short)theWidth;
        theHeader.mPixelDepth = 24;
        theHeader.mHeight = (unsigned short)theHeight;
        theHeader.mImageType = 2;
        theRowSize = theWidth * 3;
        theBuffer = new unsigned char[theRowSize];
        theResult = inFile.Write(reinterpret_cast<unsigned char*>(&theHeader), sizeof(GETGAHeader), theWritten);
        if (theResult == kErrorOK) {
            if (this->Lock() == kErrorOK) {
                theBytesPerRow = this->GetBytesPerRow();
                if (this->GetPixelDepth() == 32) {
                    thePitch = theBytesPerRow / 4;
                    the32BitPixel = reinterpret_cast<unsigned long*>(this->GetMemoryAddress()) + (theHeight - 1) * thePitch;
                    for (y = 0; y < theHeight; y++) {
                        for (x = 0; x < theWidth; x++) {
                            theBuffer[x * 3] = GEUnpackBlue(theFormat, *the32BitPixel);
                            theBuffer[x * 3 + 1] = GEUnpackGreen(theFormat, *the32BitPixel);
                            theBuffer[x * 3 + 2] = GEUnpackRed(theFormat, *the32BitPixel);
                            the32BitPixel++;
                        }
                        inFile.Write(theBuffer, theRowSize, theWritten);
                        the32BitPixel -= thePitch + theWidth;
                    }
                } else {
                    thePitch = theBytesPerRow / 2;
                    the16BitPixel = reinterpret_cast<unsigned short*>(this->GetMemoryAddress()) + (theHeight - 1) * thePitch;
                    for (y = 0; y < theHeight; y++) {
                        for (x = 0; x < theWidth; x++) {
                            theBuffer[x * 3] = GEUnpackBlue(theFormat, *the16BitPixel);
                            theBuffer[x * 3 + 1] = GEUnpackGreen(theFormat, *the16BitPixel);
                            theBuffer[x * 3 + 2] = GEUnpackRed(theFormat, *the16BitPixel);
                            the16BitPixel++;
                        }
                        inFile.Write(theBuffer, theRowSize, theWritten);
                        the16BitPixel -= thePitch + theWidth;
                    }
                }
            }
            this->Unlock();
            theResult = inFile.Write((unsigned short)0);
        }
        delete theBuffer;
        if (!theWasOpen)
            theResult = inFile.Close();
    }
    return theResult;
}

// FUNCTION: LLE 0x10026eed
// Reads an image file's width, height and depth by file type (TGA, BMP, JPG, JPG2); fails for other types
long GESurface::GetImageFileAttributes(FSFileSpec& inFile, unsigned char inType, unsigned long& outWidth,
                                       unsigned long& outHeight, unsigned long& outDepth)
{
    switch (inType) {
    case kImageFileTGA:
        return GESurface::GetTGAFileAttributes(inFile, outWidth, outHeight, outDepth);
    case kImageFileBMP:
        return GESurface::GetBMPFileAttributes(inFile, outWidth, outHeight, outDepth);
    case kImageFileJPG:
        return GESurface::GetJPGFileAttributes(inFile, outWidth, outHeight, outDepth);
    case kImageFileJPG2:
        return GESurface::GetJPG2FileAttributes(inFile, outWidth, outHeight, outDepth);
    }
    return kErrorFailed;
}

// FUNCTION: LLE 0x10026f56
// Loads an image file into the surface by file type (TGA, BMP, JPG, JPG2); fails for other types
long GESurface::Load(FSFileSpec& inFile, unsigned char inType)
{
    switch (inType) {
    case kImageFileTGA:
        return this->LoadTGA(inFile);
    case kImageFileBMP:
        return this->LoadBMP(inFile);
    case kImageFileJPG:
        return this->LoadJPG(inFile);
    case kImageFileJPG2:
        return this->LoadJPG2(inFile);
    }
    return kErrorFailed;
}

// FUNCTION: LLE 0x10026f9a
// Saves the surface as an image file of the given type (TGA, BMP, JPG, JPG2); fails for other types
long GESurface::Save(FSFileSpec& inFile, unsigned char inType)
{
    switch (inType) {
    case kImageFileTGA:
        return this->SaveAsTGA(inFile);
    case kImageFileBMP:
        return this->SaveAsBMP(inFile);
    case kImageFileJPG:
        return this->SaveAsJPG(inFile);
    case kImageFileJPG2:
        return this->SaveAsJPG2(inFile);
    }
    return kErrorFailed;
}

// ---- Always fails (BMP attributes unsupported). Exported, no address of its own: ICF-folded into an identical
// `return 9` (0x3fd29)
long GESurface::GetBMPFileAttributes(FSFileSpec& inFile, unsigned long& outWidth, unsigned long& outHeight,
                                     unsigned long& outDepth)
{
    return kErrorFailed;
}
