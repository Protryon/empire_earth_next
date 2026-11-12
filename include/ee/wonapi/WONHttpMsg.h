// HTTPGetOp's lines and request through ee_won's http module (rust/crates/ee_won/src/http.rs): Rust reads the
// status and header lines, the redirect target and the chunk length and writes the request; these apply the
// results to the op's members in the original's order (HTTPGetOp.cpp's statements). Templates over the op so that
// tests/won/won_http_test.cpp runs the same code over a stand-in. WONStatus comes from WONRouting.h, included first.
#pragma once
#include <time.h>
#include <string>
#include "ee/wonapi/WONBuffer.h"
#include "rust/ee_rust.h"

namespace WONAPI {

inline void HttpStringSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}

// MakeInternetDateTime: "Sun, 06 Nov 1994 08:49:37 GMT"
inline std::string HttpFormatDate(time_t inTime)
{
    std::string s;
    ee_won_http_format_date(static_cast<int64_t>(inTime), HttpStringSink, &s);
    return s;
}

// ParseInternetDateTime: 0 when malformed
inline time_t HttpParseDate(const char* inText)
{
    return static_cast<time_t>(ee_won_http_parse_date(reinterpret_cast<const uint8_t*>(inText)));
}

// ExtractStatus: finishes with 100092 for a malformed line, 100093 for an error status on a closing connection
template <class Op> bool HttpExtractStatus(Op& o, const char* inStatusLine)
{
    int32_t aStatus = 0;
    if (!ee_won_http_status_line(reinterpret_cast<const uint8_t*>(inStatusLine), &aStatus)) {
        o.Finish(100092);
        return false;
    }
    o.mHTTPStatus = aStatus;
    if (o.mHTTPStatus > 304 && o.mCloseConnection) {
        o.Finish(100093);
        return false;
    }
    return true;
}

// ExtractRedirect: finishes with 100094 without a scheme
template <class Op> bool HttpExtractRedirect(Op& o, const char* inLocation)
{
    size_t aHost = 0, anEnd = 0;
    intptr_t aPath = -1;
    if (!ee_won_http_redirect(reinterpret_cast<const uint8_t*>(inLocation), &aHost, &anEnd, &aPath)) {
        o.Finish(100094);
        return false;
    }
    if (aPath < 0) {
        o.mRedirectHost = inLocation + aHost;
        o.mRedirectPath = '/';
    } else {
        o.mRedirectHost.assign(inLocation + aHost, anEnd - aHost);
        o.mRedirectPath = inLocation + aPath;
    }
    return true;
}

// ExtractHeaderLine: records a known header (length, type, dates, location, chunked, close, ETag)
template <class Op> bool HttpExtractHeaderLine(Op& o, const char* inLine)
{
    EeWonHttpHeader h;
    ee_won_http_header_line(reinterpret_cast<const uint8_t*>(inLine), &h);
    if (!h.colon)
        return true;
    const char* aVal = inLine + h.value;
    switch (h.key) {
    case 0: o.mContentLength = h.number; break;
    case 1: o.mContentType.assign(aVal); break;
    case 2: o.mLastModified = static_cast<time_t>(h.time); break;
    case 3: return HttpExtractRedirect(o, aVal);
    case 4:
        if (h.flag) {
            o.mContentIsChunked = true;
            o.mIsFirstChunk = true;
        }
        break;
    case 5:
        if (h.flag)
            o.mDoClose = true;
        break;
    case 6: o.mServerTime = static_cast<time_t>(h.time); break;
    case 7: o.mExpireTime = static_cast<time_t>(h.time); break;
    case 8:
        o.mEntityTag = aVal;
        if (h.tag_len < 0)
            o.mEntityTag.erase();
        else
            o.mEntityTag.resize(h.tag_len);
        break;
    }
    return true;
}

// SendRequest's text (the local file's size and time when its path is set and it exists)
inline ByteBufferPtr HttpRequestBytes(bool inProxy, const std::string& inHost, const std::string& inPath, bool inClose,
                                      bool inFile, size_t inSize, time_t inTime, bool inRangeResume,
                                      const std::string& inEntityTag, bool inCacheOk)
{
    EeWonHttpRequest q;
    q.proxy = inProxy;
    q.host = reinterpret_cast<const uint8_t*>(inHost.data());
    q.host_len = inHost.size();
    q.path = reinterpret_cast<const uint8_t*>(inPath.data());
    q.path_len = inPath.size();
    q.close = inClose;
    q.file = inFile;
    q.size = static_cast<uint64_t>(inSize);
    q.time = static_cast<int64_t>(inTime);
    q.range_resume = inRangeResume;
    q.tag = reinterpret_cast<const uint8_t*>(inEntityTag.data());
    q.tag_len = inEntityTag.size();
    q.cache_ok = inCacheOk;
    ByteBufferPtr aBuffer;
    ee_won_http_request(&q, WonByteBufferSink, &aBuffer);
    return aBuffer;
}

} // namespace WONAPI
