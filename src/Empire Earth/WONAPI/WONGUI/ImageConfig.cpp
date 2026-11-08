// Image configurations (Image, StretchImage, StretchCenterImage, ImageFilter) and their registration
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstdio>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
namespace WONAPI {

// FUNCTION: WON 0x0078b370
// Returns the image's WIDTH or HEIGHT for a named-value lookup; false for any other name
bool ImageConfig::GetNamedValue(const std::string& inName,int& outValue)
{
    if(inName=="WIDTH") outValue=this->mImage->mWidth;
    else if(inName=="HEIGHT") outValue=this->mImage->mHeight;
    else return false;
    return true;
}

// FUNCTION: WON 0x0078b3e0
// Handles the image configuration keys SRC (loads the image) and SIZE (resizes it); false for other keys
bool ImageConfig::Virtual24(const std::string& inInstruction)
{
    if(inInstruction=="SRC") this->mImage=this->ReadImage(false);
    else if(inInstruction=="SIZE")
    {
        int width=this->ReadInt();
        this->EnsureComma();
        int height=this->ReadInt();
        this->mImage->SetSize(width,height);
    }
    else return false;
    return true;
}

// FUNCTION: WON 0x0078c340
// Reads a filter spec (SCALEPERCENT, SCALEABS, TINT, CROP, PIXELTRANS) and returns the filtered copy of
// the raw image; an empty pointer for an unknown filter name
SmartPtr<RawImage> ImageFilterConfig::HandleFilter()
{
    SmartPtr<ImageFilter> filter;
    GUIString name=this->ReadString(false);
    this->EnsureComma();
    if(name.CompareNoCase("SCALEPERCENT")==0)
    {
        int width=this->ReadInt(),height=width;
        if(!this->EndOfString())
        {
            this->EnsureComma();
            height=this->ReadInt();
        }
        SmartPtr<ScaleImageFilter> scale=new ScaleImageFilter(0);
        scale->SetPercentSize(width,height);
        filter=scale.get();
    }
    else if(name.CompareNoCase("SCALEABS")==0)
    {
        int width=this->ReadInt();
        this->EnsureComma();
        int height=this->ReadInt();
        filter=new ScaleImageFilter(width,height);
    }
    else if(name.CompareNoCase("TINT")==0)
    {
        int red=this->ReadInt();
        this->EnsureComma();
        int green=this->ReadInt();
        this->EnsureComma();
        int blue=this->ReadInt();
        filter=new TintImageFilter(red,green,blue);
    }
    else if(name.CompareNoCase("CROP")==0)
    {
        int x=this->ReadInt();
        this->EnsureComma();
        int y=this->ReadInt();
        this->EnsureComma();
        int width=this->ReadInt();
        this->EnsureComma();
        int height=this->ReadInt();
        filter=new CropImageFilter(x,y,width,height);
    }
    else if(name.CompareNoCase("PIXELTRANS")==0)
        filter=new TransparentImageFilter(this->ReadColor());
    if(filter.get()!=NULL) return filter->Filter(this->mRawImage.get());
    return SmartPtr<RawImage>();
}

// FUNCTION: WON 0x0078c6e0
// Sets the raw image (also as the current image) and marks the native image as not yet built
void ImageFilterConfig::SetRawImage(RawImage* inImage)
{
    this->mRawImage=inImage;
    this->mImage=inImage;
    this->mImageIsRawImage=true;
}

// FUNCTION: WON 0x0078c780
// Handles the image filter keys SRC, FILTER, DUPLICATE, MULTIHORZCROP (splits the image into numbered
// horizontal crops added to the parent) and EXPORT; passes other keys to the image configuration
bool ImageFilterConfig::Virtual24(const std::string& inInstruction)
{
    SmartPtr<RawImage> anImage;
    if(inInstruction=="SRC") this->SetRawImage(this->ReadRawImage().get());
    else if(inInstruction=="FILTER") this->SetRawImage(this->HandleFilter().get());
    else if(inInstruction=="DUPLICATE")
        this->SetRawImage(static_cast<RawImage*>(this->mRawImage->Duplicate().get()));
    else if(inInstruction=="MULTIHORZCROP")
    {
        int count=this->ReadInt();
        this->EnsureComma();
        std::string prefix=this->ReadString(false);
        int width=this->mRawImage->GetWidth()/count;
        SmartPtr<CropImageFilter> crop=new CropImageFilter;
        crop->SetSize(width,this->mRawImage->GetHeight());
        for(int i=0;i<count;i++)
        {
            crop->SetPos(i*width,0);
            SmartPtr<RawImage> image=crop->Filter(this->mRawImage.get());
            char name[512];
            sprintf(name,"%s%d",prefix.c_str(),i+1);
            SmartPtr<ImageFilterConfig> config=new ImageFilterConfig;
            config->SetRawImage(image.get());
            ComponentConfig* parent=static_cast<ComponentConfig*>(this->GetParentObject());
            parent->AddChildToConfigMap(GUIString(name),config.get());
        }
    }
    else if(inInstruction=="EXPORT")
    {
        GUIString name=this->ReadString(false);
        this->EnsureComma();
        SmartPtr<ImageFilterConfig> config=new ImageFilterConfig;
        config->SetRawImage(this->HandleFilter().get());
        ComponentConfig* parent=static_cast<ComponentConfig*>(this->GetParentObject());
        parent->AddChildToConfigMap(name,config.get());
    }
    else return ImageConfig::Virtual24(inInstruction);
    return true;
}

// FUNCTION: WON 0x0078cc80
// Returns the image, first converting a pending raw image to a native one on the default window
Image* ImageFilterConfig::GetImage()
{
    if(this->mImageIsRawImage)
    {
        this->mImage=this->mRawImage->GetNative(Window::GetDefaultWindow()->mDisplayContext.get()).get();
        this->mImageIsRawImage=false;
    }
    return this->mImage.get();
}

}


namespace WONAPI {
// FUNCTION: WON 0x0078adf0
// Registers the Image, StretchImage, StretchCenterImage and ImageFilter configuration types
void ImageConfig_Init(ResourceConfigTable* table)
{
    table->AddAllocator("Image",ImageConfig::CfgFactory,ImageConfig::CompFactory);
    table->AddAllocator("StretchImage",StretchImageConfig::CfgFactory,ImageConfig::CompFactory);
    table->AddAllocator("StretchCenterImage",StretchCenterImageConfig::CfgFactory,ImageConfig::CompFactory);
    table->AddAllocator("ImageFilter",ImageFilterConfig::CfgFactory,ImageConfig::CompFactory);
}
}

// Factory callbacks used by the registration above
namespace WONAPI {

// FUNCTION: WON 0x0078b130
// Creates a new image configuration
ComponentConfig* ImageConfig::CfgFactory()
{
    return new ImageConfig;
}

// FUNCTION: WON 0x0078b190
// Creates a new stretch image configuration
ComponentConfig* StretchImageConfig::CfgFactory()
{
    return new StretchImageConfig;
}

// FUNCTION: WON 0x0078b1f0
// Creates a new stretch-center image configuration
ComponentConfig* StretchCenterImageConfig::CfgFactory()
{
    return new StretchCenterImageConfig;
}

// FUNCTION: WON 0x0078b250
// Creates a new image filter configuration
ComponentConfig* ImageFilterConfig::CfgFactory()
{
    return new ImageFilterConfig;
}

// Returns no component (image configurations create none); the linker folds this body into the
// identical one at 0x006c7b10
Component* ImageConfig::CompFactory()
{
    return NULL;
}

}

namespace WONAPI {
// Accepts the parsed configuration without the base checks; the linker folds this body into the
// identical one at 0x007099f0
bool ImageConfig::PostParse(ConfigParser*)
{
    return true; // skips ComponentConfig::PostParse
}
}
