// FTPGetOp's URL, commands and replies through ee_won's ftp module (rust/crates/ee_won/src/ftp.rs): Rust splits the
// URL and path, writes the commands, reads the status and passive replies and decides each reply's step; this
// applies them to the op's members in the original's order (FTPGetOp.cpp's statements). A template over the op so
// that tests/won/won_misc_test.cpp runs the same code over a stand-in; FTPGetOp makes it a friend.
#pragma once
#include <string>
#include "ee/wonapi/WONHttpMsg.h"
#include "rust/ee_rust.h"

namespace WONAPI {

template <class Op> struct FtpMsg {
    static void SetRemotePath(Op& o, const std::string& thePath)
    {
        o.mRemotePath = thePath;
        intptr_t aSlash = ee_won_ftp_remote_split(reinterpret_cast<const uint8_t*>(thePath.data()), thePath.size());
        if (aSlash < 0) {
            o.mActualPath = "";
            o.mActualFile = thePath;
        } else {
            o.mActualPath = thePath.substr(0, aSlash);
            o.mActualFile = thePath.substr(aSlash + 1);
        }
    }

    // FTPGetOp(url, extract): the user name and password (when asked), the host and the remote path
    static void FromURL(Op& o, const std::string& theURL, bool doExtract)
    {
        intptr_t r[8];
        ee_won_ftp_url(reinterpret_cast<const uint8_t*>(theURL.data()), theURL.size(), doExtract, r);
        if (r[0] >= 0) o.SetUserName(theURL.substr(r[0], r[1] - r[0]));
        if (r[2] >= 0) o.SetPassword(theURL.substr(r[2], r[3] - r[2]));
        o.mHost = theURL.substr(r[4], r[5] - r[4]);
        if (r[6] >= 0) SetRemotePath(o, theURL.substr(r[6], r[7] - r[6]));
    }

    // ExtractUserNameAndPasswordFromURL: the user name and password taken off the front of theURL
    static void ExtractUserNameAndPassword(Op& o, std::string& theURL)
    {
        intptr_t r[5];
        ee_won_ftp_credentials(reinterpret_cast<const uint8_t*>(theURL.data()), theURL.size(), r);
        std::string aURL = theURL;
        if (r[0] >= 0) {
            o.SetUserName(aURL.substr(r[0], r[1] - r[0]));
            theURL = aURL.substr(r[1] + 1);
        }
        if (r[2] >= 0) {
            o.SetPassword(aURL.substr(r[2], r[3] - r[2]));
            theURL = aURL.substr(r[3] + 1);
        }
    }

    static bool ExtractStatus(Op& o, const char* theStatusLine)
    {
        int32_t aStatus = 0;
        int aKind = ee_won_ftp_status(reinterpret_cast<const uint8_t*>(theStatusLine), &aStatus);
        if (aKind == 0) {
            o.Finish(EE_WON_FTP_INVALID_RESPONSE);
            return false;
        }
        o.mFTPStatus = aStatus;
        if (aKind == 2) {
            o.Finish(EE_WON_FTP_STATUS_ERROR);
            return false;
        }
        return true;
    }

    static bool ParsePasv(Op& o, const char* theStatusLine)
    {
        uint32_t aHost = 0;
        uint16_t aPort = 0;
        if (!ee_won_ftp_pasv(reinterpret_cast<const uint8_t*>(theStatusLine), &aHost, &aPort)) {
            o.Finish(EE_WON_FTP_INVALID_PASV);
            return false;
        }
        o.mDataAddr.Fn2B2C20(static_cast<long>(static_cast<int32_t>(aHost)), aPort);
        return true;
    }

    static std::string Command(Op& o)
    {
        std::string aReq;
        ee_won_ftp_command(o.mRequestState, reinterpret_cast<const uint8_t*>(o.mUserName.data()), o.mUserName.size(),
                           reinterpret_cast<const uint8_t*>(o.mPassword.data()), o.mPassword.size(),
                           reinterpret_cast<const uint8_t*>(o.mActualPath.data()), o.mActualPath.size(),
                           reinterpret_cast<const uint8_t*>(o.mActualFile.data()), o.mActualFile.size(), o.mIsBinary,
                           o.mLocalFileSize, HttpStringSink, &aReq);
        return aReq;
    }

    // HandleReply after the reply op's status check: the step for the line
    static bool HandleReply(Op& o, const char* aLine, int aLen)
    {
        EeWonFtpStep s;
        ee_won_ftp_reply(reinterpret_cast<const uint8_t*>(aLine), aLen < 0 ? 0 : aLen, o.mRequestState,
                         !o.mActualPath.empty(), o.mDoResume, &s);
        if (s.action == EE_WON_FTP_MORE) {
            if (o.IsAsync())
                o.RecvLineAsync();
            return true;
        }
        if (s.scanned)
            o.mFTPStatus = s.ftp_status;
        switch (s.action) {
        case EE_WON_FTP_FINISH:
            o.Finish(s.status);
            return false;
        case EE_WON_FTP_SEND:
            if (s.no_resume)
                o.mDoResume = false;
            o.mRequestState = static_cast<typename Op::RequestState>(s.state);
            return o.SendRequest();
        case EE_WON_FTP_DATA:
            o.mDataAddr.Fn2B2C20(static_cast<long>(static_cast<int32_t>(s.host)), s.port);
            o.mRequestState = static_cast<typename Op::RequestState>(s.state);
            if (!o.DoDataConnection())
                return false;
            return o.SendRequest();
        case EE_WON_FTP_RECV:
            o.RecvContent();
            return false;
        }
        return false;
    }
};

} // namespace WONAPI
