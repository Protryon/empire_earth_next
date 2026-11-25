// The next engine's LLM client: an optional language model on the LAN speaks for the AI (Age of AI's
// voice/llm.c, the retail DLL's). This engine has no voice system, so this is the transport only: the Lua AI
// configures it (ai.llm_config), asks it (ai.llm) and polls it (ai.llm) - one request at a time, on a worker
// thread, so a slow or missing server never stalls the lockstep step. Windows WinHTTP, UTF-8 in and out.
//
// The answer extraction is deliberately small: the OpenAI chat API's first choice's message content, or the
// server's error text. Other fields are ignored. No prompt building, no persona data: those are the Lua side's.
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

// THE CHAT INBOX: every in-game chat line, for the Lua side to read (ai.chat). Portable (no Windows calls), so a
// simulation records them too; the engine's voice (ai.say) pushes its own lines as well, and the Lua side skips
// its own by the player id.
namespace {
struct ChatLine {
    int player;
    std::string text;
};
std::mutex g_chat_lock;
std::deque<ChatLine> g_chat;
const size_t kChatCap = 32;
}  // namespace

extern "C" void EECPNextChatPush(int player, const char* text) {
    if (text == NULL || *text == 0) return;
    std::lock_guard<std::mutex> h(g_chat_lock);
    while (g_chat.size() >= kChatCap) g_chat.pop_front();
    g_chat.push_back({player, std::string(text)});
}

extern "C" int EECPNextChatPoll(int* player, char* out, int cap) {
    std::lock_guard<std::mutex> h(g_chat_lock);
    if (g_chat.empty()) return 0;
    ChatLine line = g_chat.front();
    g_chat.pop_front();
    if (player != NULL) *player = line.player;
    if (out != NULL && cap > 0) {
        int n = (int)line.text.size();
        if (n > cap - 1) n = cap - 1;
        memcpy(out, line.text.data(), (size_t)n);
        out[n] = 0;
    }
    return 1;
}

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace {

struct LLMState {
    std::mutex lock;
    std::string url, key, model;
    bool enabled = false;
    int timeout_s = 20;
    bool think = false;
    std::string answer;          // the last finished answer
    unsigned long long answer_seq = 0;
    std::atomic<bool> busy{false};
    std::atomic<unsigned long long> done{0};
};

LLMState g;

std::wstring wide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w((size_t)(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// a JSON string's body (no surrounding quotes), escaped; the caller checked the opening quote
std::string json_escape(const std::string& in) {
    std::string o;
    o.reserve(in.size() + 8);
    for (unsigned char c : in) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) {
                char b[8];
                snprintf(b, sizeof b, "\\u%04x", c);
                o += b;
            } else {
                o += (char)c;
            }
        }
    }
    return o;
}

// the first "content" string under "choices" (the chat API), else the server's "error"/"message" text
std::string extract_answer(const std::string& body) {
    size_t c = body.find("\"choices\"");
    if (c != std::string::npos) {
        size_t m = body.find("\"message\"", c);
        if (m == std::string::npos) m = c;
        size_t k = body.find("\"content\"", m);
        if (k != std::string::npos) {
            size_t q = body.find('"', body.find(':', k) + 1);
            if (q != std::string::npos) {
                std::string out;
                for (size_t i = q + 1; i < body.size(); i++) {
                    char ch = body[i];
                    if (ch == '"') break;
                    if (ch == '\\' && i + 1 < body.size()) {
                        char e = body[++i];
                        if (e == 'n') out += '\n';
                        else if (e == 't') out += '\t';
                        else if (e == 'r') out += '\r';
                        else if (e == 'u' && i + 4 < body.size()) {
                            unsigned v = 0;
                            for (int d = 0; d < 4; d++) {
                                char h = body[++i];
                                v <<= 4;
                                if (h >= '0' && h <= '9') v |= (unsigned)(h - '0');
                                else if (h >= 'a' && h <= 'f') v |= (unsigned)(h - 'a' + 10);
                                else if (h >= 'A' && h <= 'F') v |= (unsigned)(h - 'A' + 10);
                            }
                            if (v < 0x80) out += (char)v;
                            else out += '?';
                        } else out += e;
                    } else out += ch;
                }
                return out;
            }
        }
    }
    size_t e = body.find("\"error\"");
    if (e == std::string::npos) e = body.find("\"message\"");
    if (e != std::string::npos) {
        size_t q = body.find(':', e);
        if (q != std::string::npos) {
            q = body.find('"', q + 1);
            if (q != std::string::npos) {
                size_t z = body.find('"', q + 1);
                if (z != std::string::npos) return body.substr(q + 1, z - q - 1);
            }
        }
    }
    return body.substr(0, 400);
}

bool http_post(const std::string& url, const std::string& key, const std::string& body, int timeout_s, std::string& out) {
    std::wstring wurl = wide(url);
    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof uc);
    uc.dwStructSize = sizeof uc;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    uc.dwSchemeLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return false;
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.lpszExtraInfo && uc.dwExtraInfoLength) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    HINTERNET session = WinHttpOpen(L"Empire Earth AI/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    int ms = timeout_s * 1000;
    WinHttpSetTimeouts(session, ms, ms, ms, ms);
    HINTERNET conn = WinHttpConnect(session, host.c_str(), uc.nPort, 0);
    if (!conn) {
        WinHttpCloseHandle(session);
        return false;
    }
    DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(conn, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) {
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        return false;
    }
    std::wstring wh(L"Content-Type: application/json\r\n");
    if (!key.empty()) wh += L"Authorization: Bearer " + wide(key) + L"\r\n";
    BOOL ok = WinHttpSendRequest(req, wh.c_str(), (DWORD)-1, (LPVOID)body.data(), (DWORD)body.size(),
                                 (DWORD)body.size(), 0);
    if (ok) ok = WinHttpReceiveResponse(req, NULL);
    if (ok) {
        DWORD code = 0, len = sizeof code;
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &code, &len, WINHTTP_NO_HEADER_INDEX);
        std::string raw;
        char buf[8192];
        for (;;) {
            DWORD got = 0;
            if (!WinHttpReadData(req, buf, sizeof buf, &got) || got == 0) break;
            raw.append(buf, got);
            if (raw.size() > 262144) break;
        }
        out = raw;
        ok = code >= 200 && code < 300;
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return ok != FALSE;
}

void worker(std::string url, std::string key, std::string body, int timeout_s) {
    std::string raw, answer;
    bool ok = http_post(url, key, body, timeout_s, raw);
    {
        std::lock_guard<std::mutex> h(g.lock);
        answer = extract_answer(raw);
        if (!ok) answer = "llm error: " + answer;
        g.answer = answer;
        g.answer_seq++;
    }
    g.done++;
    g.busy = false;
}

}  // namespace

// configure the client (ai.llm_config); called by the Lua AI from its settings
extern "C" void EECPNextLLMConfigure(const char* url, const char* key, const char* model, int enabled, int timeout_s,
                                     int think) {
    std::lock_guard<std::mutex> h(g.lock);
    g.url = url ? url : "";
    g.key = key ? key : "";
    g.model = model ? model : "";
    g.enabled = enabled != 0;
    g.timeout_s = timeout_s > 0 && timeout_s < 300 ? timeout_s : 20;
    g.think = think != 0;
}

// start one request; 0 when disabled or busy, the sequence number otherwise
extern "C" unsigned long long EECPNextLLMRequest(const char* system_prompt, const char* prompt) {
    std::string url, key, model;
    bool enabled;
    int timeout_s;
    bool think;
    {
        std::lock_guard<std::mutex> h(g.lock);
        url = g.url;
        key = g.key;
        model = g.model;
        enabled = g.enabled;
        timeout_s = g.timeout_s;
        think = g.think;
    }
    if (!enabled || url.empty() || prompt == NULL) return 0;
    if (g.busy.exchange(true)) return 0;
    std::string body = "{\"messages\":[";
    if (system_prompt != NULL && *system_prompt != 0) {
        body += "{\"role\":\"system\",\"content\":\"" + json_escape(system_prompt) + "\"},";
    }
    body += "{\"role\":\"user\",\"content\":\"" + json_escape(prompt) + "\"}],";
    body += "\"max_tokens\":300,\"temperature\":0.8";
    if (!model.empty()) body += ",\"model\":\"" + json_escape(model) + "\"";
    if (!think) body += ",\"reasoning_effort\":\"none\"";
    body += "}";
    unsigned long long seq;
    {
        std::lock_guard<std::mutex> h(g.lock);
        seq = g.answer_seq + 1;
    }
    std::thread(worker, url, key, body, timeout_s).detach();
    return seq;
}

// copy the answer started after `seq` (0: the latest): 1 ready, 0 nothing new
extern "C" int EECPNextLLMPoll(unsigned long long since, char* out, int cap) {
    std::lock_guard<std::mutex> h(g.lock);
    if (g.answer.empty() || g.answer_seq <= since) return 0;
    if (out == NULL || cap <= 0) return 1;
    int n = (int)g.answer.size();
    if (n > cap - 1) n = cap - 1;
    memcpy(out, g.answer.data(), (size_t)n);
    out[n] = 0;
    return 1;
}

extern "C" unsigned long long EECPNextLLMSeq(void) {
    std::lock_guard<std::mutex> h(g.lock);
    return g.answer_seq;
}

#else  // not _WIN32: no WinHTTP, the transport is inert (the voice is off)

extern "C" void EECPNextLLMConfigure(const char* url, const char* key, const char* model, int enabled, int timeout_s,
                                     int think) {
    (void)url;
    (void)key;
    (void)model;
    (void)enabled;
    (void)timeout_s;
    (void)think;
}

extern "C" unsigned long long EECPNextLLMRequest(const char* system_prompt, const char* prompt) {
    (void)system_prompt;
    (void)prompt;
    return 0;
}

extern "C" int EECPNextLLMPoll(unsigned long long since, char* out, int cap) {
    (void)since;
    if (out != NULL && cap > 0) out[0] = 0;
    return 0;
}

extern "C" unsigned long long EECPNextLLMSeq(void) {
    return 0;
}

#endif  // _WIN32
