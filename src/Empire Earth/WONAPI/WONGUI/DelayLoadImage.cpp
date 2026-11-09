// Image that decodes its file only when it is first drawn (file name inferred)
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
#include "ee/wonapi/WONLobbyDelayLoadImage.h"

namespace WONAPI {
// FUNCTION: WON 0x00737c00
// Creates an image of a given size that decodes the file at a path when first needed
DelayLoadImage::DelayLoadImage(const std::string& path,int width,int height) : mImagePath(path)
{
    this->mHaveLoaded=false;
    this->mWidth=(unsigned short)width;
    this->mHeight=(unsigned short)height;
}
// FUNCTION: WON 0x00737c90
// Deletes the delay-load image (scalar deleting destructor, compiler-generated)
// (??_GDelayLoadImage@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x00737cb0
// Destroys the delay-load image, releasing the decoded image and the path (compiler-generated)
// (??1DelayLoadImage@WONAPI@@UAE@XZ)

// FUNCTION: WON 0x00737d50
// Decodes the image on the first call, then draws it if decoding succeeded
void DelayLoadImage::Draw(DisplayContext* context,int x,int y,int left,int top,int width,int height)
{
    if(!this->mHaveLoaded)
    {
        this->mHaveLoaded=true;
        this->mImage=WONAPI::WindowManager::GetDefaultWindowManager()->DecodeImage(this->mImagePath.c_str());
    }
    if(this->mImage.get()!=NULL) this->mImage->Draw(context,x,y,left,top,width,height);
}
// FUNCTION: WON 0x00737e50
// Decodes the image on the first call and returns it (NULL if decoding failed)
SmartPtr<NativeImage> DelayLoadImage::GetNative(DisplayContext*)
{
    if(!this->mHaveLoaded)
    {
        this->mHaveLoaded=true;
        this->mImage=WONAPI::WindowManager::GetDefaultWindowManager()->DecodeImage(this->mImagePath.c_str());
    }
    return this->mImage;
}
}
