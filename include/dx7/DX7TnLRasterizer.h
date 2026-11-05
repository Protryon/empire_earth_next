// DX7TnLRasterizer (RECONSTRUCTION): DX7HRTnLDisplay.dll's rasterizer, a DX7Rasterizer that creates the hardware
// transform-and-lighting device (GXIID_TnLHalDevice) and write-only video-memory vertex buffers. Exports:
// ctor(const UString&), copy ctor/operator= (implicit), dtor, ActivateTheScreen, CreateVertexBuffers, GetIsScreenValid,
// vtable ??_7DX7TnLRasterizer@@6B@ (0x10011470). No members of its own (copy ctor 0x10001322 = DX7Rasterizer's + vptr).
#ifndef DX7_DX7TNLRASTERIZER_H
#define DX7_DX7TNLRASTERIZER_H

#include "dx7/DX7Rasterizer.h"

class DX7_API DX7TnLRasterizer : public DX7Rasterizer {
public:
    DX7TnLRasterizer(const UString& inName);
    virtual ~DX7TnLRasterizer();

    virtual bool GetIsScreenValid(DX7Screen* inScreen);

protected:
    virtual long ActivateTheScreen(unsigned long inScreen, GESurface* inPrimary, GESurface* inSecondary,
                                   GETextureManager* inManager, GEWindow* inWindow);
    virtual long CreateVertexBuffers(unsigned long inScreen);
};

#endif
