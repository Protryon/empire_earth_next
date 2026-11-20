#ifndef EE_WONAPI_WONLOBBYCONFIGIMAGES_H
#define EE_WONAPI_WONLOBBYCONFIGIMAGES_H
#include "ee/wonapi/WONLobbyStretchImage.h"
namespace WONAPI {

// Image configuration classes. ImageFilterConfig is constructed at 0x0078c260 and destroyed at
// 0x0078c2b0: ImageConfig (0x7c) + raw image pointer at +0x7c + bool at +0x80 = 0x84 bytes.
class ImageConfig : public ComponentConfig {
public:
    ImageConfig(); // at 0x0078b2b0
    // Implicit virtual destructor: releases the owned members, then the base.
    virtual bool PostParse(ConfigParser*); // at 0x007099f0
    virtual bool Virtual24(const std::string&); //38b3e0
    virtual bool GetNamedValue(const std::string&,int&); //38b370
    virtual Image* GetImage() { return this->mImage.get(); } // slot 0x38, at 0x0078b2d0
    virtual RawImage* GetRawImage() { return NULL; } // slot 0x3c (base behaviour), at 0x006c7b10
    SmartPtr<Image> mImage; //78, total7c
};
// 0x80-byte configs; the raw pointer at +0x7c aliases the inherited image pointer at +0x78.
class StretchImageConfig : public ImageConfig {
public:
    StretchImageConfig(); //38b4e0
    // Implicit virtual destructor: releases the owned members, then the base.
    virtual bool Virtual24(const std::string&); //38b630
    StretchImage* mStretchImage; // offset 0x7c, raw alias; class size 0x80
};
class StretchCenterImageConfig : public ImageConfig {
public:
    StretchCenterImageConfig(); //38bf90
    // Implicit virtual destructor: releases the owned members, then the base.
    virtual bool Virtual24(const std::string&); //38c0e0
    StretchCenterImage* mStretchCenterImage; // offset 0x7c, raw alias; class size 0x80
};
class ImageFilterConfig : public ImageConfig {
public:
    ImageFilterConfig(); // at 0x0078c260
    // Implicit virtual destructor: releases the owned members, then the base.
    virtual bool Virtual24(const std::string&); //38c780
    virtual Image* GetImage(); //38cc80 slot38
    virtual RawImage* GetRawImage() { return this->mRawImage.get(); } // slot 0x3c, at 0x0078c280
    void SetRawImage(RawImage*); //38c6e0
    SmartPtr<RawImage> mRawImage; //7c
    bool mImageIsRawImage; //80 naturalpad, total84
};
}
#endif
