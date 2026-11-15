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
// Creates an image of a given size that decodes the file at a path when first needed
DelayLoadImage::DelayLoadImage(const std::string& path,int width,int height) : mImagePath(path)
{
    this->mHaveLoaded=false;
    this->mWidth=(unsigned short)width;
    this->mHeight=(unsigned short)height;
}

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
