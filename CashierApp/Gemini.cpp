// Gemini.cpp - talks to the Gemini API over HTTPS using WinHTTP (built into Windows).
//
// How it works (4 steps):
//   1. Read the API key from the environment variable GEMINI_API_KEY.
//   2. Build a small JSON text:  {"contents":[{"parts":[{"text":"<our question>"}]}]}
//   3. Send it with an HTTPS POST to generativelanguage.googleapis.com.
//   4. Find the "text" field in the JSON reply and show it.
// Gemini only receives sales numbers. It never calculates prices, payment or stock.
#include "Gemini.h"
#include "Transaction.h"
#include <windows.h>
#include <winhttp.h>
#include <cstdlib>
#include <vector>

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty())
        return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0);
    std::wstring wide(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &wide[0], size);
    return wide;
}

// Builds the question for Gemini from the sales ranking in MySQL.
std::string BuildSalesPrompt()
{
    std::vector<SalesRow> ranking;
    if (!LoadSalesRanking(ranking) || ranking.empty())
        return "";

    int totalSold = 0;
    for (size_t i = 0; i < ranking.size(); i++)
        totalSold += ranking[i].sold;
    if (totalSold == 0)
        return "";                      // nothing sold yet

    std::string prompt = "You are a sales analysis assistant for a small shop. "
                         "Here is the sales data (items sold, and stock left):\n\n";
    for (size_t i = 0; i < ranking.size(); i++) {
        prompt += ranking[i].name + " = " + std::to_string(ranking[i].sold) + " sold, stock left "
                  + std::to_string(ranking[i].stock) + "\n";
    }
    prompt += "\nPlease answer in Indonesian, short and simple:\n"
              "1. Which product is the best-selling?\n"
              "2. Which product sells the least?\n"
              "3. Which products may need more stock?\n"
              "4. Give 2-3 simple recommendations for the shop owner.\n"
              "Do not recalculate any numbers. Plain text only, no markdown.";
    return prompt;
}

// Makes text safe to place inside a JSON string (quotes, new lines, ...).
static std::string JsonEscape(const std::string& text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}

// Adds one unicode character to a UTF-8 string.
static void AppendUtf8(std::string& out, unsigned int code)
{
    if (code < 0x80) {
        out += (char)code;
    } else if (code < 0x800) {
        out += (char)(0xC0 | (code >> 6));
        out += (char)(0x80 | (code & 0x3F));
    } else {
        out += (char)(0xE0 | (code >> 12));
        out += (char)(0x80 | ((code >> 6) & 0x3F));
        out += (char)(0x80 | (code & 0x3F));
    }
}

// Reads the first "text": "...." value from the JSON reply (decoding \n, \", \uXXXX).
static std::string ExtractText(const std::string& json)
{
    size_t pos = json.find("\"text\"");
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";
    pos++;                                   // first character inside the quotes

    std::string out;
    while (pos < json.size()) {
        char c = json[pos];
        if (c == '"') break;                 // end of the text
        if (c == '\\' && pos + 1 < json.size()) {
            char next = json[pos + 1];
            pos += 2;
            if (next == 'n')      out += '\n';
            else if (next == 't') out += ' ';
            else if (next == 'r') { }
            else if (next == 'u' && pos + 4 <= json.size()) {
                unsigned int code = (unsigned int)strtoul(json.substr(pos, 4).c_str(), NULL, 16);
                pos += 4;
                AppendUtf8(out, code);
            }
            else out += next;                // \" \\ \/
            continue;
        }
        out += c;
        pos++;
    }
    return out;
}

// Reads the "message": "...." value of an API error reply.
static std::string ExtractErrorMessage(const std::string& json)
{
    size_t pos = json.find("\"message\"");
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

bool AskGemini(const std::string& prompt, std::wstring& answer, std::string& error)
{
    // Step 1: API key from the environment (never written in the source code)
    const char* key = getenv("GEMINI_API_KEY");
    if (key == NULL || key[0] == '\0') {
        error = "GEMINI_API_KEY belum diatur. Lihat README, bagian 'Gemini API key'.";
        return false;
    }
    const char* modelEnv = getenv("GEMINI_MODEL");   // optional
    std::string model = (modelEnv != NULL && modelEnv[0] != '\0') ? modelEnv : "gemini-flash-latest";

    // Step 2: JSON body
    std::string body = "{\"contents\":[{\"parts\":[{\"text\":\"" + JsonEscape(prompt) + "\"}]}]}";

    // Step 3: HTTPS POST
    const std::string noInternet = "Tidak dapat terhubung ke Gemini AI.\nPeriksa koneksi internet.";
    HINTERNET session = WinHttpOpen(L"CashierApp/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == NULL) { error = noInternet; return false; }
    WinHttpSetTimeouts(session, 10000, 10000, 30000, 60000);   // timeouts in milliseconds

    HINTERNET connect = WinHttpConnect(session, L"generativelanguage.googleapis.com",
                                       INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connect == NULL) { WinHttpCloseHandle(session); error = noInternet; return false; }

    std::wstring path = L"/v1beta/models/" + Utf8ToWide(model) + L":generateContent";
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (request == NULL) {
        WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        error = noInternet;
        return false;
    }

    std::wstring headers = L"Content-Type: application/json\r\nx-goog-api-key: " + Utf8ToWide(key);
    BOOL ok = WinHttpSendRequest(request, headers.c_str(), (DWORD)-1L,
                                 (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (ok)
        ok = WinHttpReceiveResponse(request, NULL);

    std::string response;
    DWORD status = 0;
    if (ok) {
        DWORD statusSize = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

        DWORD available = 0;
        while (WinHttpQueryDataAvailable(request, &available) && available > 0) {   // read the reply
            std::vector<char> buffer(available);
            DWORD bytesRead = 0;
            if (!WinHttpReadData(request, &buffer[0], available, &bytesRead) || bytesRead == 0)
                break;
            response.append(&buffer[0], bytesRead);
        }
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    if (!ok) {
        DWORD code = GetLastError();
        if (code == ERROR_WINHTTP_TIMEOUT)
            error = "Permintaan ke Gemini AI timeout. Coba lagi.";
        else
            error = noInternet;
        return false;
    }

    // Step 4: look at the HTTP status, then read the text
    if (status == 429) {
        error = "Kuota / batas permintaan Gemini API habis. Coba lagi nanti.";
        return false;
    }
    else if (status == 400 || status == 401 || status == 403) {
        error = "Permintaan ditolak (HTTP " + std::to_string(status) + "). API key mungkin salah.\n"
                + ExtractErrorMessage(response);
        return false;
    }
    else if (status != 200) {
        error = "Gemini API error (HTTP " + std::to_string(status) + ").\n" + ExtractErrorMessage(response);
        return false;
    }

    std::string text = ExtractText(response);
    if (text.empty()) {
        error = "Respons Gemini kosong atau tidak valid.";
        return false;
    }

    // Prepare for the GUI: remove markdown stars and use Windows new lines
    std::wstring wide = Utf8ToWide(text);
    answer = L"";
    for (size_t i = 0; i < wide.size(); i++) {
        if (wide[i] == L'*') continue;
        if (wide[i] == L'\n') answer += L"\r\n";
        else answer += wide[i];
    }
    return true;
}
