// URL's members only the drivers use, cut from ee/wonapi/WONURL.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
// a relative URL against a base (fragments append to the base path, others replace its file part; 2d7e90)
void CalcRelativeTo(const URL& inBase) { ee_won_url_obj_relative(this->mUrl, inBase.mUrl); }
std::string GetPathAsLocalFilePath() const { return WONFile(this->GetPath()).GetFilePath(); } //2d7d10: backslashes
void GetEntire(std::string& outString) { outString = this->Text(2); } //2d8150: the full text from scheme, address and path
