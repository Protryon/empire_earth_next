// BMP reader: attributes and raw 4-, 8- and 24-bit images
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
using namespace WONAPI;
// FUNCTION: WON 0x007366e0
// Reads the BMP file header; returns whether it starts with "BM"
static bool BMPReadFileHeader(FileReader &theReader)
{
	unsigned short bfType = theReader.ReadShort();
	unsigned long bfSize = theReader.ReadLong();
	unsigned short bfReserved1 = theReader.ReadShort(); 
	unsigned short bfReserved2 = theReader.ReadShort(); 
	unsigned long bfOffBits = theReader.ReadLong(); 

	if (bfType != ((unsigned short) ('M' << 8) | 'B'))
		return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00736690
// Reads the image width and height from a BMP header; false when the file is not a BMP
bool BMPDecoder::DoGetAttrib(ImageAttributes &theAttrib)
{
	if(!BMPReadFileHeader(*this->mFileReader))
		return false;

	int aPos = this->mFileReader->pos();

	DWORD  biSize = this->mFileReader->ReadLong(); 
	long biWidth = this->mFileReader->ReadLong(); 
	long biHeight = this->mFileReader->ReadLong(); 
	theAttrib.mWidth = biWidth;
	theAttrib.mHeight = biHeight;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00736720
// Decodes a 4-, 8- or 24-bit BMP into a raw image (palette for 8 bits or less); NULL for other depths or read errors
SmartPtr<RawImage> BMPDecoder::DoDecodeRaw()
{
	try
	{
		if(!BMPReadFileHeader(*this->mFileReader))
			return NULL;

		int aPos = this->mFileReader->pos();

		DWORD  biSize = this->mFileReader->ReadLong(); 
		long biWidth = this->mFileReader->ReadLong(); 
		long biHeight = this->mFileReader->ReadLong(); 
		short biPlanes = this->mFileReader->ReadShort(); 
		short biBitCount = this->mFileReader->ReadShort();
		DWORD  biCompression = this->mFileReader->ReadLong(); 
		DWORD  biSizeImage = this->mFileReader->ReadLong(); 
		long biXPelsPerMeter = this->mFileReader->ReadLong(); 
		long biYPelsPerMeter = this->mFileReader->ReadLong(); 
		DWORD  biClrUsed = this->mFileReader->ReadLong(); 
		DWORD  biClrImportant = this->mFileReader->ReadLong(); 

		this->mFileReader->SkipBytes(biSize - (this->mFileReader->pos() - aPos));

		int aNumColors = biClrUsed?biClrUsed:(1<<biBitCount);
		int aTransparentIndex = -1;
		SmartPtr<Palette> aPalette;
		if(biBitCount<=8)
		{
			aPalette = new Palette(aNumColors);
			for(int i=0; i<aNumColors; i++)
			{
				unsigned char r,g,b,a;
				b = this->mFileReader->ReadByte();
				g = this->mFileReader->ReadByte();
				r = this->mFileReader->ReadByte();
				a = this->mFileReader->ReadByte();

				DWORD aColor = (r<<16) | (g<<8) | b;
//				if(aColor==mTransparentColor)
//					aTransparentIndex = i;
				
				aPalette->mColors[i]=aColor;
			}
		}

		int x,y;
		int startY, dy, yact;
		if(biHeight<0)
		{
			biHeight = -biHeight;
			startY = 0;
			dy = 1;
		}
		else
		{
			startY = biHeight-1;
			dy = -1;
		}


		if(biBitCount==4)
		{
			// The original pads 4-bit rows from the truncated byte count biWidth/2, so odd widths read the wrong
			// number of padding bytes
            int anExtra=(biWidth/2)%4;
            if(anExtra!=0) anExtra=4-anExtra;

			SmartPtr<RawImage8> anImage = new RawImage8(biWidth,biHeight,aPalette.get());
			if(aTransparentIndex>=0)
				anImage->mTransparentColor=aTransparentIndex;

			for(y=0, yact=startY; y<biHeight; y++, yact+=dy)
			{
				anImage->StartRowTraversal(yact);
				for(x=0; x<biWidth; x++)
				{
					unsigned char aByte = this->mFileReader->ReadByte();
					*anImage->mRowPtr=(aByte>>4)&0x0f;
					x++;
					if(x<biWidth)
					{
						++anImage->mRowPtr;
						*anImage->mRowPtr=(aByte)&0x0f;
					}
					++anImage->mRowPtr;
				}

				for(x=0; x<anExtra; x++)
					this->mFileReader->ReadByte();
			}

			return anImage.get();
		}
		if(biBitCount==8)
		{
			int anExtra = biWidth%4;
			if(anExtra!=0)
				anExtra = 4 - anExtra;

			SmartPtr<RawImage8> anImage = new RawImage8(biWidth,biHeight,aPalette.get());
			if(aTransparentIndex>=0)
				anImage->mTransparentColor=aTransparentIndex;

			for(y=0, yact=startY; y<biHeight; y++, yact+=dy)
			{
				anImage->StartRowTraversal(yact);
				for(x=0; x<biWidth; x++)
				{
					*anImage->mRowPtr=this->mFileReader->ReadByte();
					++anImage->mRowPtr;
				}

				for(x=0; x<anExtra; x++)
					this->mFileReader->ReadByte();
			}

			return anImage.get();
		}
		else if(biBitCount==24)
		{
			int anExtra = (biWidth*3)%4;
			if(anExtra!=0)
				anExtra = 4 - anExtra;

			SmartPtr<RawImage32> anImage = new RawImage32(biWidth,biHeight);
//			if(mTransparentColor>=0)
//				anImage->SetDoTransparency(true);

			for(y=0, yact=startY; y<biHeight; y++, yact+=dy)
			{
				anImage->StartRowTraversal(yact);
				for(x=0; x<biWidth; x++)
				{
					unsigned char b=this->mFileReader->ReadByte();
                    unsigned char g=this->mFileReader->ReadByte();
                    unsigned char r=this->mFileReader->ReadByte();
                    int aPixel=b | (g<<8) | (r<<16);
//					if(aPixel!=mTransparentColor)
//						aPixel |= 0xff000000;

					*anImage->mRowPtr=aPixel;
					++anImage->mRowPtr;
				}

				for(x=0; x<anExtra; x++)
					this->mFileReader->ReadByte();
			}

			return anImage.get();
		}
	}
	catch(FileReaderException&)
	{
	}

	return NULL;
}


// FUNCTION: WON 0x0072b9f0
// Returns a new BMP decoder
ImageDecoder* BMPDecoder::Duplicate() { return new BMPDecoder; }
