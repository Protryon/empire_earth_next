// BMPDecoder::DoDecode on the platform layer (BMPDecoderPlatform.cpp's GDI version made a device-dependent bitmap
// with CreateDIBitmap): reads the whole file and converts the DIB into a native image, the same pixels GDI made.
// Uncompressed 1/4/8/16/24/32-bit DIBs and BI_BITFIELDS 16/32-bit; RLE files decode to NULL.
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

static uint32_t BMPRead32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t BMPRead16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// One channel of a bit-field pixel scaled to 8 bits
static uint32_t BMPField(uint32_t inPixel, uint32_t inMask)
{
    if (inMask == 0)
        return 0;
    int theShift = 0;
    while (!((inMask >> theShift) & 1))
        theShift++;
    uint32_t theMax = inMask >> theShift;
    return (((inPixel & inMask) >> theShift) * 255 + theMax / 2) / theMax;
}

// Reads a whole BMP file into a native image for the display; NULL if it is not a BMP, is empty or is not supported
SmartPtr<NativeImage> BMPDecoder::DoDecode(DisplayContext* theContext)
{
    try {
        unsigned char aHeader[14];
        this->mFileReader->ReadBytes(aHeader, sizeof(aHeader));
        if (BMPRead16(aHeader) != (unsigned short)(('M' << 8) | 'B'))
            return NULL;
        int aSize = (int)BMPRead32(aHeader + 2) - (int)sizeof(aHeader);
        if (aSize <= 40)
            return NULL;
        std::vector<unsigned char> aData(aSize);
        this->mFileReader->ReadBytes(aData.data(), aSize);
        const unsigned char* anInfo = aData.data();
        uint32_t biSize = BMPRead32(anInfo);
        int32_t biWidth = (int32_t)BMPRead32(anInfo + 4);
        int32_t biHeight = (int32_t)BMPRead32(anInfo + 8);
        int biBitCount = BMPRead16(anInfo + 14);
        uint32_t biCompression = BMPRead32(anInfo + 16);
        uint32_t biClrUsed = BMPRead32(anInfo + 32);
        if (biSize < 40 || biSize > (uint32_t)aSize || biWidth <= 0 || biHeight == 0 || biWidth > 0x7fff ||
            biHeight > 0x7fff || biHeight < -0x7fff)
            return NULL;
        bool aBitFields = biCompression == 3;   // BI_BITFIELDS
        if (biCompression != 0 && !(aBitFields && (biBitCount == 16 || biBitCount == 32)))
            return NULL;
        if (biBitCount != 1 && biBitCount != 4 && biBitCount != 8 && biBitCount != 16 && biBitCount != 24 &&
            biBitCount != 32)
            return NULL;
        // as the original: the colour table follows the header, the bits follow the table (bfOffBits unused)
        int nColors = biClrUsed ? (int)biClrUsed : (biBitCount <= 8 ? 1 << biBitCount : 0);
        size_t aTable = biSize;
        uint32_t aMasks[3] = {0x7c00, 0x03e0, 0x001f};
        if (biBitCount == 32) {
            aMasks[0] = 0xff0000; aMasks[1] = 0xff00; aMasks[2] = 0xff;
        }
        size_t aBits;
        if (biBitCount > 8) {
            if (aBitFields) {
                size_t aMaskAt = 40;   // V3: after the header; V4/V5: inside it, at the same offset
                if (aMaskAt + 12 > (size_t)aSize)
                    return NULL;
                for (int i = 0; i < 3; i++)
                    aMasks[i] = BMPRead32(anInfo + aMaskAt + i * 4);
            }
            aBits = aTable + biClrUsed * 4 + ((aBitFields && biSize == 40) ? 12 : 0);
        } else
            aBits = aTable + (size_t)nColors * 4;
        int aWidth = biWidth, aHeight = biHeight < 0 ? -biHeight : biHeight;
        size_t aStride = (((size_t)aWidth * biBitCount + 31) / 32) * 4;
        if (aBits > (size_t)aSize || aBits + aStride * aHeight > (size_t)aSize)
            return NULL;
        if (biBitCount <= 8 && aTable + (size_t)nColors * 4 > (size_t)aSize)
            return NULL;

        SmartPtr<RawImage32> aRaw = new RawImage32(aWidth, aHeight);
        for (int y = 0; y < aHeight; y++) {
            const unsigned char* aRow = anInfo + aBits + aStride * (biHeight < 0 ? y : aHeight - 1 - y);
            DWORD* anOut = aRaw->mImageData + (size_t)y * aWidth;
            for (int x = 0; x < aWidth; x++) {
                uint32_t aColor = 0;
                if (biBitCount <= 8) {
                    int anIndex;
                    if (biBitCount == 8)
                        anIndex = aRow[x];
                    else if (biBitCount == 4)
                        anIndex = (aRow[x >> 1] >> ((x & 1) ? 0 : 4)) & 0xf;
                    else
                        anIndex = (aRow[x >> 3] >> (7 - (x & 7))) & 1;
                    if (anIndex < nColors) {
                        const unsigned char* q = anInfo + aTable + anIndex * 4;   // RGBQUAD: B, G, R
                        aColor = ((uint32_t)q[2] << 16) | (q[1] << 8) | q[0];
                    }
                } else if (biBitCount == 24) {
                    const unsigned char* q = aRow + x * 3;
                    aColor = ((uint32_t)q[2] << 16) | (q[1] << 8) | q[0];
                } else {
                    uint32_t aPixel = biBitCount == 16 ? BMPRead16(aRow + x * 2) : BMPRead32(aRow + x * 4);
                    aColor = (BMPField(aPixel, aMasks[0]) << 16) | (BMPField(aPixel, aMasks[1]) << 8) |
                             BMPField(aPixel, aMasks[2]);
                }
                anOut[x] = aColor;
            }
        }
        return aRaw->CreateNativeImage(theContext);
    } catch (FileReaderException&) {
    }
    return NULL;
}

} // namespace WONAPI
