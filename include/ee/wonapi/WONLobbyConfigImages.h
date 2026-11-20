#ifndef EE_WONAPI_WONLOBBYCONFIGIMAGES_H
#define EE_WONAPI_WONLOBBYCONFIGIMAGES_H
#include "ee/wonapi/WONLobbyStretchImage.h"
#include "rust/won_gui/cfgkind.h"
namespace WONAPI {

// Image configuration classes (their instructions are their kinds' tables: ee_won_gui cfgkind.rs, cfgtab.rs).
// ImageFilterConfig: ImageConfig (0x7c) + raw image pointer at +0x7c + bool at +0x80 = 0x84 bytes.
class ImageConfig : public ComponentConfig {
public:
    // 0x0078b2b0: marked as having no child components to create
    explicit ImageConfig(EeWonCfgKind inKind = EeWonCfgKind::Image) : ComponentConfig(inKind) { this->mHaveInitedComponents=true; }
    virtual bool PostParse(ConfigParser*) { return true; } // at 0x007099f0: skips ComponentConfig::PostParse
    virtual bool GetNamedValue(const std::string&,int&); //38b370
    virtual Image* GetImage() { return this->mImage.get(); } // slot 0x38, at 0x0078b2d0
    virtual RawImage* GetRawImage() { return NULL; } // slot 0x3c (base behaviour), at 0x006c7b10
    SmartPtr<Image> mImage; //78, total7c
};
// 0x80-byte configs; the raw pointer at +0x7c aliases the inherited image pointer at +0x78.
class StretchImageConfig : public ImageConfig {
public:
    //38b4e0: a new empty stretch image
    StretchImageConfig() : ImageConfig(EeWonCfgKind::StretchImage) { this->mStretchImage=new StretchImage(NULL,0,0,0,0); this->mImage=this->mStretchImage; }
    StretchImage* mStretchImage; // offset 0x7c, raw alias; class size 0x80
};
class StretchCenterImageConfig : public ImageConfig {
public:
    //38bf90: a new empty stretch-center image
    StretchCenterImageConfig() : ImageConfig(EeWonCfgKind::StretchCenterImage) { this->mStretchCenterImage=new StretchCenterImage; this->mImage=this->mStretchCenterImage; }
    StretchCenterImage* mStretchCenterImage; // offset 0x7c, raw alias; class size 0x80
};
class ImageFilterConfig : public ImageConfig {
public:
    ImageFilterConfig() : ImageConfig(EeWonCfgKind::ImageFilter) { this->mImageIsRawImage=false; } // at 0x0078c260
    virtual Image* GetImage(); //38cc80 slot38
    virtual RawImage* GetRawImage() { return this->mRawImage.get(); } // slot 0x3c, at 0x0078c280
    void SetRawImage(RawImage*); //38c6e0
    SmartPtr<RawImage> mRawImage; //7c
    bool mImageIsRawImage; //80 naturalpad, total84
};
}
#endif
