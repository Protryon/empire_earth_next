// Config handlers for stretch images and stretch-center images (file name inferred)
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
#include "ee/wonapi/WONLobbyConfigImages.h"
namespace WONAPI {
// FUNCTION: WON 0x0078b4e0
// Creates a stretch image config holding a new empty stretch image
StretchImageConfig::StretchImageConfig()
{
    this->mStretchImage=new StretchImage(NULL,0,0,0,0);
    this->mImage=this->mStretchImage;
}

// FUNCTION: WON 0x0078b630
// Reads SRC, TILE, BORDERS, SKIPCENTER and the EXPORT keys, which add copies with other images to the parent
bool StretchImageConfig::Virtual24(const std::string& instruction)
{
    if(instruction=="SRC") {
        this->mStretchImage->SetImage(this->ReadRawImage().get());
    }
    else if(instruction=="TILE") {
        bool horz=this->ReadBool();this->EnsureComma();
        bool vert=this->ReadBool();
        this->mStretchImage->SetTile(horz,vert);
    }
    else if(instruction=="BORDERS") {
        int left=this->ReadInt();this->EnsureComma();
        int top=this->ReadInt();this->EnsureComma();
        int right=this->ReadInt();this->EnsureComma();
        this->mStretchImage->SetBorders(left,top,right,this->ReadInt());
        if(!this->EndOfString()) {
            this->EnsureComma();
            bool horz=this->ReadBool();this->EnsureComma();
            bool vert=this->ReadBool();
            this->mStretchImage->SetTile(horz,vert);
        }
    }
    else if(instruction=="SKIPCENTER") {
        this->mStretchImage->SetSkipCenter(this->ReadBool());
    }
    else if(instruction=="MULTIEXPORT") {
        int count=this->ReadInt();this->EnsureComma();
        std::string imagePrefix=this->ReadRawString(false);this->EnsureComma();
        std::string configPrefix=this->ReadRawString(false);
        int offset=0;
        if(!this->EndOfString()) {
            this->EnsureComma();
            offset=this->ReadInt()-1;
        }
        for(int i=0;i<count;++i) {
            char imageName[512],configName[512];
            sprintf(imageName,"%s%d",imagePrefix.c_str(),i+1+offset);
            sprintf(configName,"%s%d",configPrefix.c_str(),i+1);
            SmartPtr<RawImage> image=this->GetRawImageFromStr(imageName);
            SmartPtr<StretchImageConfig> child=new StretchImageConfig;
            child->mStretchImage->CopyAttributes(this->mStretchImage);
            child->mStretchImage->SetImage(image.get());
            ComponentConfig* parent=static_cast<ComponentConfig*>(this->GetParentObject());
            parent->AddChildToConfigMap(configName,child.get());
        }
    }
    else if(instruction=="MULTICROPEXPORT") {
        int count=this->ReadInt();this->EnsureComma();
        SmartPtr<RawImage> image=this->ReadRawImage();this->EnsureComma();
        std::string configPrefix=this->ReadRawString(false);
        int width=image->GetWidth()/count;
        SmartPtr<CropImageFilter> crop=new CropImageFilter;
        crop->SetSize(width,image->GetHeight());
        for(int i=0;i<count;i++) {
            crop->SetPos(i*width,0);
            char configName[512];
            sprintf(configName,"%s%d",configPrefix.c_str(),i+1);
            SmartPtr<RawImage> cropped=crop->Filter(image.get());
            SmartPtr<StretchImageConfig> child=new StretchImageConfig;
            child->mStretchImage->CopyAttributes(this->mStretchImage);
            child->mStretchImage->SetImage(cropped.get());
            ComponentConfig* parent=static_cast<ComponentConfig*>(this->GetParentObject());
            parent->AddChildToConfigMap(configName,child.get());
        }
    }
    else if(instruction=="EXPORT") {
        GUIString name=this->ReadString(false);this->EnsureComma();
        SmartPtr<StretchImageConfig> child=new StretchImageConfig;
        child->mStretchImage->CopyAttributes(this->mStretchImage);
        child->mStretchImage->SetImage(this->ReadRawImage().get());
        ComponentConfig* parent=static_cast<ComponentConfig*>(this->GetParentObject());
        parent->AddChildToConfigMap(name,child.get());
    }
    else return ImageConfig::Virtual24(instruction);
    return true;
}

// FUNCTION: WON 0x0078bf90
// Creates a stretch-center image config holding a new empty stretch-center image
StretchCenterImageConfig::StretchCenterImageConfig()
{
    this->mStretchCenterImage=new StretchCenterImage;
    this->mImage=this->mStretchCenterImage;
}

// FUNCTION: WON 0x0078c0e0
// Reads the STRETCHIMAGE and CENTERIMAGE keys; other keys go to the image config
bool StretchCenterImageConfig::Virtual24(const std::string& instruction)
{
    if(instruction=="STRETCHIMAGE") this->mStretchCenterImage->SetStretchImage(this->ReadImage(false).get());
    else if(instruction=="CENTERIMAGE") this->mStretchCenterImage->SetCenterImage(this->ReadImage(false).get());
    else return ImageConfig::Virtual24(instruction);
    return true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0078b2d0
// (public: virtual class WONAPI::Image * __thiscall WONAPI::ImageConfig::GetImage(void)    [?GetImage@ImageConfig@WONAPI@@UAEPAVImage@2@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0078b300
// (public: virtual __thiscall WONAPI::ImageConfig::~ImageConfig(void)    [??1ImageConfig@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x0078b5a0
// (public: virtual void * __thiscall WONAPI::StretchImageConfig::`scalar deleting destructor'(unsigned int)    [??_GStretchImageConfig@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0078b5c0
// (public: virtual __thiscall WONAPI::StretchImageConfig::~StretchImageConfig(void)    [??1StretchImageConfig@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x0078c050
// (public: virtual void * __thiscall WONAPI::StretchCenterImageConfig::`scalar deleting destructor'(unsigned int)    [??_GStretchCenterImageConfig@WONAPI)
// TEMPLATE: WON 0x0078c070
// (public: virtual __thiscall WONAPI::StretchCenterImageConfig::~StretchCenterImageConfig(void)    [??1StretchCenterImageConfig@WONAPI@@UAE@XZ])
