// Windows BMP decoder: builds a GDI bitmap from the file (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <memory>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
using namespace WONAPI;
// FUNCTION: WON 0x007ce870
// Reads a whole BMP file into a GDI device-dependent bitmap for the display; NULL if it is not a BMP or is empty
SmartPtr<NativeImage> BMPDecoder::DoDecode(DisplayContext *theContext)
{
	try
	{
		// Read file header
		BITMAPFILEHEADER aHeader;
		this->mFileReader->ReadBytes(&aHeader,sizeof(aHeader));
		if (aHeader.bfType != ((unsigned short) ('M' << 8) | 'B'))
			return NULL;

		int aSize = aHeader.bfSize - sizeof(aHeader);
		if(aSize<=0)
			return NULL;

		char *aData = new char[aSize];
		std::auto_ptr<char> aDelData(aData);

		this->mFileReader->ReadBytes(aData,aSize);
		BITMAPINFOHEADER &bmiHeader = *(LPBITMAPINFOHEADER)(aData);
		BITMAPINFO &bmInfo = *(LPBITMAPINFO)(aData);

		// If bmiHeader.biClrUsed is zero we have to infer the number
		// of colors from the number of bits used to specify it.
		int nColors = bmiHeader.biClrUsed ? bmiHeader.biClrUsed : 
							1 << bmiHeader.biBitCount;

		LPVOID lpDIBBits;
		if( bmInfo.bmiHeader.biBitCount > 8 )
			lpDIBBits = (LPVOID)((LPDWORD)(bmInfo.bmiColors + bmInfo.bmiHeader.biClrUsed) + 
				((bmInfo.bmiHeader.biCompression == BI_BITFIELDS) ? 3 : 0));
		else
			lpDIBBits = (LPVOID)(bmInfo.bmiColors + nColors);

		HBITMAP hBmp = CreateDIBitmap( theContext->mDC,		// handle to device context
					&bmiHeader,	// pointer to bitmap size and format data
					CBM_INIT,	// initialization flag
					lpDIBBits,	// pointer to initialization data
					&bmInfo,	// pointer to bitmap color-format data
					DIB_RGB_COLORS);		// color-data usage

		SmartPtr<MSNativeImage> anImage = new MSNativeImage(theContext, hBmp);	
//		if(mTransparentColor>=0)
//			anImage->SetTransparentColor(mTransparentColor);

		return anImage.get();
	}
	catch(FileReaderException&)
	{
	}

	return NULL;
}
