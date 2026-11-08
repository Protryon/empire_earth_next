// ComponentConfig value readers: strings, fonts, images, cursors and selection colors (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
namespace WONAPI {
// FUNCTION: WON 0x007842f0
// Reads a string value, or the string resource named after '@'
GUIString ComponentConfig::ReadString(bool stopAtWhitespace)
{
    this->mStringParser->SkipWhitespace();
    if(this->mStringParser->GetChar(0)=='@') {
        this->mStringParser->IncrementPos(1);
        std::string key;
        this->mStringParser->ReadString(key,false);
        return this->GetStringResource(key);
    }
    std::wstring value;
    this->mStringParser->ReadString(value,stopAtWhitespace);
    return GUIString(value);
}
// FUNCTION: WON 0x00784480
// Reads a font: '@resource', '(CFG,name)' from a FontConfig found up the parent chain, or a literal font spec
// (the parent search has no end test: a FontConfig found nowhere walks past the root config)
SmartPtr<Font> ComponentConfig::ReadFont()
{
    this->mStringParser->SkipWhitespace();
    if(this->mStringParser->GetChar(0)=='@') {
        this->mStringParser->IncrementPos(1);
        std::string name;
        this->mStringParser->ReadString(name,false);
        return this->GetFontResource(name);
    }
    if(this->mStringParser->GetChar(0)=='(') {
        this->mStringParser->IncrementPos(1);
        std::string type;
        this->mStringParser->ReadString(type,false);
        if(_stricmp(type.c_str(),"CFG")==0) {
            this->EnsureComma();
            std::string name;
            this->mStringParser->ReadString(name,false);
            ComponentConfig* current=this;
            FontConfig* fontConfig;
            while(true) {
                fontConfig=dynamic_cast<FontConfig*>(current->GetComponentConfig(name,false));
                if(fontConfig!=NULL) break;
                current=static_cast<ComponentConfig*>(current->mParent);
            }
            if(fontConfig==NULL)
                throw ConfigObjectException("Invalid FontConfig: "+name);
            if(fontConfig->GetFont()==NULL)
                throw ConfigObjectException("NULL Font: "+name);
            if(!this->mStringParser->CheckNextChar(')'))
                throw ConfigObjectException("Expecting ')'");
            return fontConfig->GetFont();
        }
        throw ConfigObjectException("Bad Font Type: "+type);
    }
    return ResourceConfig::SafeReadFont(*this->mStringParser);
}
// FUNCTION: WON 0x00784890
// Resolves an image spec: '@CFG_name' from an ImageConfig up the parent chain, '@NULL', '@resource',
// or an image file to decode; throws on failure
SmartPtr<Image> ComponentConfig::GetImageFromStr(const std::string& spec,bool getRaw)
{
    if(spec[0]=='@') {
        std::string::size_type split=spec.find('_');
        if(split!=std::string::npos) {
            std::string type=spec.substr(1,split-1);
            if(_stricmp(type.c_str(),"CFG")==0) {
                std::string name=spec.substr(split+1);
                ComponentConfig* current=this;
                ImageConfig* config=NULL;
                while(current!=NULL) {
                    config=dynamic_cast<ImageConfig*>(current->GetComponentConfig(name,false));
                    if(config!=NULL) break;
                    current=static_cast<ComponentConfig*>(current->mParent);
                }
                if(config==NULL)
                    throw ConfigObjectException("Invalid ImageConfig: "+spec);
                else {
                SmartPtr<Image> image;
                image=getRaw?static_cast<Image*>(config->GetRawImage()):config->GetImage();
                if(image.get()==NULL)
                    throw ConfigObjectException("Image Doesn't Exist: "+spec);
                return image;
                }
            }
        } else if(_stricmp(spec.c_str(),"@NULL")==0) return NULL;
        else return this->GetImageResource(spec.c_str()+1);
        throw ConfigObjectException("Bad Image Specification: "+spec);
    }
    SmartPtr<Image> image;
    if(getRaw)
        image=Window::GetDefaultWindow()->DecodeImageRaw(ComponentConfig::GetResourceFile(spec).c_str()).get();
    else image=WONAPI::WindowManager::GetDefaultWindowManager()->DecodeDelayImage(ComponentConfig::GetResourceFile(spec).c_str());
    if(image.mPtr==NULL)
        throw ConfigObjectException("Failed to decode image: "+spec);
    return image;
}
// FUNCTION: WON 0x00784f10
// Resolves an image spec as a raw image
SmartPtr<RawImage> ComponentConfig::GetRawImageFromStr(const std::string& spec)
{
    return (RawImage*)this->GetImageFromStr(spec,true).get();
}
// FUNCTION: WON 0x00784fa0
// Reads an image spec and resolves it (raw or delay-loaded)
SmartPtr<Image> ComponentConfig::ReadImage(bool getRaw)
{
    std::string name;
    StringParser& aParser=*this->mStringParser;
    aParser.ReadString(name,false);
    return this->GetImageFromStr(name,getRaw);
}
// FUNCTION: WON 0x00785050
// Reads an image spec and resolves it as a raw image
SmartPtr<RawImage> ComponentConfig::ReadRawImage()
{
    SmartPtr<RawImage> result=(RawImage*)this->ReadImage(true).get();
    return result;
}
// FUNCTION: WON 0x00785120
// Reads a predefined cursor name (Arrow, Hand, IBeam, EastWest, NorthSouth ...); throws on an unknown name
StandardCursor ComponentConfig::ReadStandardCursor()
{
    GUIString name=this->ReadString(false);
    if(name.CompareNoCase("Arrow")==0) return StandardCursor_Arrow;
    if(name.CompareNoCase("Hand")==0) return StandardCursor_Hand;
    if(name.CompareNoCase("IBeam")==0) return StandardCursor_IBeam;
    if(name.CompareNoCase("EastWest")==0) return StandardCursor_EastWest;
    if(name.CompareNoCase("NorthSouth")==0) return StandardCursor_NorthSouth;
    if(name.CompareNoCase("NorthSouthEastWest")==0) return StandardCursor_NorthSouthEastWest;
    throw ConfigObjectException("Invalid predefined cursor type: "+(std::string)name);
}
// FUNCTION: WON 0x00785380
// Reads a cursor; only 'Predefined,<name>' is supported, anything else throws
SmartPtr<Cursor> ComponentConfig::ReadCursor()
{
    GUIString type=this->ReadString(false);
    if(type.CompareNoCase("Predefined")==0) {
        this->EnsureComma();
        return Cursor::GetStandardCursor(this->ReadStandardCursor());
    }
    throw ConfigObjectException("Invalid cursor type: "+(std::string)type);
}
// FUNCTION: WON 0x00785cd0
// Reads a selection color: a background color plus an optional foreground mode
// (USEFOREGROUND with a color, INVERTORIGINAL, USEORIGINAL)
SelectionColor ComponentConfig::ReadSelectionColor()
{
    DWORD background=this->ReadColor();
    if(this->EndOfString()) return background;
    this->EnsureComma();
    GUIString type=this->ReadString(false);
    if(type.CompareNoCase("USEFOREGROUND")==0) {
        this->EnsureComma();
        DWORD foreground=this->ReadColor();
        return SelectionColor(background,foreground);
    }
    if(type.CompareNoCase("INVERTORIGINAL")==0)
        return SelectionColor(background,ForegroundSelectionMode_InvertOriginalColor);
    if(type.CompareNoCase("USEORIGINAL")==0)
        return SelectionColor(background,ForegroundSelectionMode_UseOriginalColor);
    throw ConfigObjectException("Invalid SelectionColor");
}
}
