// The config subclasses' bodies as they were at ab1b33bcb (copied with git show from ConfigTableHost.cpp,
// ContainerConfigGroups.cpp, ComponentLayoutConfigs.cpp, FontConfig.cpp and ImageConfig.cpp), included in
// won_cfgbody34_test.cpp's namespace Orig over its world.
// Grows the button to fit the largest of a comma-separated list of texts, keeping its current text
void ComponentConfig::HandleFitText()
{
    Button* aButton=static_cast<Button*>(this->mComponent.get());
    int width=aButton->Width();
    int height=aButton->Height();
    GUIString oldText=aButton->GetText();
    for(;;)
    {
        GUIString text=this->ReadString(false);
        aButton->SetText(text,true);
        int desiredWidth=0,desiredHeight=0;
        aButton->GetDesiredSize(desiredWidth,desiredHeight);
        if(desiredWidth>width) width=desiredWidth;
        if(desiredHeight>height) height=desiredHeight;
        if(this->EndOfString()) break;
        this->EnsureComma();
    }
    aButton->SetText(oldText,true);
    aButton->SetSize(width,height);
}
// Reads a PERCENTLAYOUT group into a percent layout (relative to the component if no reference is given)
// or a COLORSCHEME group into the container's colour scheme; other instructions go to the base config
bool ContainerConfig::HandleInstruction(ConfigParser* parser,const std::string& type)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_CONTAINERCONFIG_HANDLEINSTRUCTION_TYPE,type.data(),type.size());
    if(aKeyIdx==0) {
        SmartPtr<PercentLayoutConfig> config=new PercentLayoutConfig;
        if(this->ParseChild(*parser,config.get(),false,true)) {
            int i;
            for(i=0;i<4;++i) if(config->refs[i]!=NULL) break;
            if(i==4) config->refs[0]=this->mComponent.get();
            PercentLayoutParam param(config->refs[0],config->refs[1],config->refs[2],config->refs[3],
                config->params[0],config->params[1],config->params[2],config->params[3],
                config->params[4],config->params[5],config->params[6],config->params[7]);
            this->mContainer->AddChildLayout(new PercentLayout(config->target,param),this->mCurLayoutId);
        }
        return true;
    }
    if(aKeyIdx==1) {
        SmartPtr<ColorSchemeConfig> config=new ColorSchemeConfig;
        if(this->ParseChild(*parser,config.get(),false,true))
            this->mContainer->SetColorScheme(config->mColorScheme.get());
        return true;
    }
    return ComponentConfig::HandleInstruction(parser,type);
}
// Finds the component this config is named after in the parent config; reports an error if it is missing
bool PercentLayoutConfig::PreParse(ConfigParser* parser)
{
    ComponentConfig* parent=static_cast<ComponentConfig*>(this->mParent);
    this->target=parent->Fn386860(this->mName);
    if(this->target==NULL) {
        parser->SetError("Component not found: "+this->mName,false);
        return false;
    }
    return true;
}
// Starts from the colors of the nearest parent component with its own scheme, else the window manager's
bool ColorSchemeConfig::PreParse(ConfigParser*)
{
    ComponentConfig* parent=static_cast<ComponentConfig*>(this->mParent);
    while(parent!=NULL) {
        Component* component=parent->mComponent.get();
        if(component!=NULL) {
            ColorScheme* scheme=component->GetOwnColorScheme();
            if(scheme!=NULL) {
                memcpy(this->mColorScheme->mColors,scheme->mColors,sizeof(this->mColorScheme->mColors));
                return true;
            }
        }
        parent=static_cast<ComponentConfig*>(parent->mParent);
    }
    ColorScheme* scheme=WONAPI::WindowManager::GetDefaultWindowManager()->mColorScheme.get();
    memcpy(this->mColorScheme->mColors,scheme->mColors,sizeof(this->mColorScheme->mColors));
    return true;
}
// Returns the configured font, first fetching it from the default window when the descriptor
// changed and no matching font is held
Font* FontConfig::GetFont()
{
    if(this->mNeedRecalcFont) {
        if(this->mFont.get()==NULL || this->mFont->mDescriptor!=this->mDescriptor)
            this->mFont=Window::GetDefaultWindow()->GetFont(this->mDescriptor).get();
    }
    return this->mFont.get();
}
// Sets the font directly, taking its descriptor, so no lookup is needed
void FontConfig::SetFont(Font* font)
{
    this->mFont=font;
    if(font!=NULL) this->mDescriptor=font->mDescriptor;
    this->mNeedRecalcFont=false;
}
bool FontConfig::GetNamedValue(const std::string& name,int& value)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_FONTCONFIG_GETNAMEDVALUE_NAME,name.data(),name.size());
    if(this->mFont.get()==NULL) return false;
    if(aKeyIdx==0) value=this->mDescriptor.mSize;
    else if(aKeyIdx==1) value=this->mFont->GetHeight();
    else return false;
    return true;
}
bool ImageConfig::GetNamedValue(const std::string& inName,int& outValue)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_IMAGECONFIG_GETNAMEDVALUE_INNAME,inName.data(),inName.size());
    if(aKeyIdx==0) outValue=this->mImage->GetWidth();
    else if(aKeyIdx==1) outValue=this->mImage->GetHeight();
    else return false;
    return true;
}
Image* ImageFilterConfig::GetImage()
{
    if(this->mImageIsRawImage)
    {
        this->mImage=this->mRawImage->GetNative(Window::GetDefaultWindow()->mDisplayContext.get()).get();
        this->mImageIsRawImage=false;
    }
    return this->mImage.get();
}
