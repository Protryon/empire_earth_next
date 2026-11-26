// the members only the drivers use, cut from ee/wonapi/WONLobbyResource.h's WONFile (R6 WON GUI round 40):
// included in its class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
    bool NormalizePath() { return this->Num(EE_WON_FILE_NORMALIZE) != 0; } // false for an empty path
    std::string GetRelativePath(const WONFile& inSub) { return this->Text(EE_WON_FILE_RELATIVE, inSub.mFile); }
    bool SetReadOnly(bool inReadOnly) { return this->Num(EE_WON_FILE_SET_READ_ONLY, inReadOnly) != 0; }
    std::string GetFileDirectory() { return this->Text(EE_WON_FILE_DIRECTORY); }
    std::string GetFileExtension() { return this->Text(EE_WON_FILE_EXTENSION); }
    std::string GetFullPath() { return this->Text(EE_WON_FILE_FULL_PATH); }
    std::string GetFileNameNoExtension() { return this->Text(EE_WON_FILE_NAME_NO_EXTENSION); } // the member as it is
