// TGADecoder: fieldless (8 bytes), optional; not identified in the original exe.
#ifndef EE_WONAPI_WONLOBBYTGADECODER_H
#define EE_WONAPI_WONLOBBYTGADECODER_H
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
namespace WONAPI {
class TGADecoder : public ImageDecoder
{
protected:
	virtual SmartPtr<RawImage> DoDecodeRaw();
	virtual bool DoGetAttrib(ImageAttributes &theAttrib);

public:
	virtual ImageDecoder* Duplicate() { return new TGADecoder; }
};
}
#endif
