#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

using std::string;
using std::wstring;
using std::vector;

static const char kOwner[] = "Firatkebap";
static const char kRepo[] = "Firatkebap.github.io";
static const char kBranch[] = "main";
static const char kMenuPath[] = "data/menu.json";

static const COLORREF C_BG = RGB(16, 10, 8);
static const COLORREF C_CARD = RGB(32, 22, 18);
static const COLORREF C_LINE = RGB(72, 52, 40);
static const COLORREF C_GOLD = RGB(232, 184, 109);
static const COLORREF C_EMBER = RGB(232, 93, 4);
static const COLORREF C_PAPER = RGB(243, 230, 208);
static const COLORREF C_MUTED = RGB(180, 156, 136);
static const COLORREF C_ROW = RGB(26, 16, 13);
static const int ROW_H = 86;

struct Item { string id, name, desc, image; double price = 0; };
struct Section { string id, title; vector<Item> items; };
struct Menu { string name, subtitle, kicker, note; vector<Section> sections; };

static HINSTANCE gInst;
static HWND gMain, gList, gName, gDesc, gPrice, gStatus;
static HFONT gFont, gFontB, gFontTitle, gFontSm;
static HBRUSH gBg, gCard, gRow, gEdit;
static Menu gMenu;
static string gToken;
static int gCat = 0, gSel = -1, gScroll = 0;

static wstring Utf16(const string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
  return w;
}
static string Utf8(const wstring& w) {
  if (w.empty()) return "";
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  string s(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
  return s;
}
static wstring ConfigPath() {
  wchar_t appdata[MAX_PATH];
  GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
  wstring dir = wstring(appdata) + L"\\FiratKebapAdmin";
  CreateDirectoryW(dir.c_str(), nullptr);
  return dir + L"\\account.dat";
}
static string Sha256(const string& s) {
  HCRYPTPROV prov = 0; HCRYPTHASH hash = 0; string out;
  if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return "";
  if (CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash)) {
    CryptHashData(hash, (BYTE*)s.data(), (DWORD)s.size(), 0);
    DWORD len = 32; BYTE buf[32];
    if (CryptGetHashParam(hash, HP_HASHVAL, buf, &len, 0)) {
      static const char* hex = "0123456789abcdef";
      out.resize(64);
      for (DWORD i = 0; i < 32; i++) { out[i * 2] = hex[buf[i] >> 4]; out[i * 2 + 1] = hex[buf[i] & 0xf]; }
    }
    CryptDestroyHash(hash);
  }
  CryptReleaseContext(prov, 0);
  return out;
}
static bool ProtectWrite(const wstring& path, const string& plain) {
  DATA_BLOB in{(DWORD)plain.size(), (BYTE*)plain.data()}, out{};
  if (!CryptProtectData(&in, L"FiratKebap", nullptr, nullptr, nullptr, 0, &out)) return false;
  HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) { LocalFree(out.pbData); return false; }
  DWORD w; WriteFile(f, out.pbData, out.cbData, &w, nullptr); CloseHandle(f); LocalFree(out.pbData);
  return true;
}
static bool ProtectRead(const wstring& path, string& plain) {
  HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (f == INVALID_HANDLE_VALUE) return false;
  DWORD sz = GetFileSize(f, nullptr); string blob(sz, 0); DWORD r;
  ReadFile(f, blob.data(), sz, &r, nullptr); CloseHandle(f);
  DATA_BLOB in{r, (BYTE*)blob.data()}, out{};
  if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) return false;
  plain.assign((char*)out.pbData, out.cbData); LocalFree(out.pbData);
  return true;
}
static string JsonEsc(const string& s) {
  string o; o.reserve(s.size());
  for (unsigned char c : s) {
    if (c == '"') o += "\\\""; else if (c == '\\') o += "\\\\"; else if (c == '\n') o += "\\n"; else o += (char)c;
  }
  return o;
}
static string MenuToJson(const Menu& m) {
  std::ostringstream o;
  o << "{\n  \"name\": \"" << JsonEsc(m.name) << "\",\n  \"subtitle\": \"" << JsonEsc(m.subtitle)
    << "\",\n  \"kicker\": \"" << JsonEsc(m.kicker) << "\",\n  \"note\": \"" << JsonEsc(m.note)
    << "\",\n  \"sections\": [\n";
  for (size_t s = 0; s < m.sections.size(); s++) {
    const auto& sec = m.sections[s];
    o << "    {\n      \"id\": \"" << JsonEsc(sec.id) << "\",\n      \"title\": \"" << JsonEsc(sec.title)
      << "\",\n      \"items\": [\n";
    for (size_t i = 0; i < sec.items.size(); i++) {
      const auto& it = sec.items[i];
      o << "        {\"id\": \"" << JsonEsc(it.id) << "\", \"name\": \"" << JsonEsc(it.name)
        << "\", \"desc\": \"" << JsonEsc(it.desc) << "\", \"price\": " << it.price
        << ", \"image\": \"" << JsonEsc(it.image) << "\"}";
      o << (i + 1 < sec.items.size() ? ",\n" : "\n");
    }
    o << "      ]\n    }" << (s + 1 < m.sections.size() ? ",\n" : "\n");
  }
  o << "  ]\n}\n";
  return o.str();
}
static string Unesc(const string& s) {
  string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '\\' && i + 1 < s.size()) { char n = s[++i]; o += (n == 'n' ? '\n' : n); }
    else o += s[i];
  }
  return o;
}
static bool ExtractStr(const string& json, const string& key, size_t from, size_t to, string& out) {
  string pat = "\"" + key + "\"";
  size_t k = json.find(pat, from);
  if (k == string::npos || k > to) return false;
  size_t q = json.find('"', json.find(':', k) + 1);
  if (q == string::npos || q > to) return false;
  size_t e = q + 1;
  while (e < json.size() && e <= to) {
    if (json[e] == '\\') { e += 2; continue; }
    if (json[e] == '"') break;
    e++;
  }
  out = Unesc(json.substr(q + 1, e - q - 1));
  return true;
}
static bool ParseMenu(const string& json, Menu& m) {
  m = {};
  size_t secKey = json.find("\"sections\"");
  if (secKey == string::npos) return false;
  ExtractStr(json, "name", 0, secKey, m.name);
  ExtractStr(json, "subtitle", 0, secKey, m.subtitle);
  ExtractStr(json, "kicker", 0, secKey, m.kicker);
  ExtractStr(json, "note", 0, secKey, m.note);
  size_t p = json.find('[', secKey);
  while (true) {
    size_t obj = json.find('{', p);
    if (obj == string::npos) break;
    size_t items = json.find("\"items\"", obj);
    if (items == string::npos) break;
    Section s;
    ExtractStr(json, "id", obj, items, s.id);
    ExtractStr(json, "title", obj, items, s.title);
    size_t arr = json.find('[', items);
    size_t arrEnd = json.find(']', arr);
    size_t ip = arr;
    while (true) {
      size_t io = json.find('{', ip);
      if (io == string::npos || io > arrEnd) break;
      size_t ie = json.find('}', io);
      Item it;
      ExtractStr(json, "id", io, ie, it.id);
      ExtractStr(json, "name", io, ie, it.name);
      ExtractStr(json, "desc", io, ie, it.desc);
      ExtractStr(json, "image", io, ie, it.image);
      size_t pk = json.find("\"price\"", io);
      if (pk != string::npos && pk < ie) it.price = atof(json.c_str() + json.find_first_of("0123456789.-", pk));
      s.items.push_back(it);
      ip = ie + 1;
    }
    m.sections.push_back(s);
    p = json.find('}', arrEnd);
    if (p == string::npos) break;
    p++;
    size_t nxt = json.find('{', p);
    size_t close = json.find(']', p);
    if (nxt == string::npos || (close != string::npos && close < nxt)) break;
  }
  return !m.sections.empty();
}
static string B64(const unsigned char* data, size_t n) {
  DWORD len = 0;
  CryptBinaryToStringA((BYTE*)data, (DWORD)n, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &len);
  string s(len, 0);
  CryptBinaryToStringA((BYTE*)data, (DWORD)n, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, s.data(), &len);
  if (!s.empty() && s.back() == 0) s.pop_back();
  return s;
}
static string Http(const wstring& method, const wstring& path, const string& body, int& status) {
  status = 0; string result;
  HINTERNET ses = WinHttpOpen(L"FiratKebapAdmin/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
  if (!ses) return "";
  HINTERNET con = WinHttpConnect(ses, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!con) { WinHttpCloseHandle(ses); return ""; }
  HINTERNET req = WinHttpOpenRequest(con, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!req) { WinHttpCloseHandle(con); WinHttpCloseHandle(ses); return ""; }
  wstring headers = L"Accept: application/vnd.github+json\r\nAuthorization: Bearer " + Utf16(gToken) +
                    L"\r\nContent-Type: application/json\r\nUser-Agent: FiratKebapAdmin\r\n";
  BOOL ok = WinHttpSendRequest(req, headers.c_str(), (DWORD)-1, body.empty() ? nullptr : (LPVOID)body.data(),
                               (DWORD)body.size(), (DWORD)body.size(), 0);
  if (ok) ok = WinHttpReceiveResponse(req, nullptr);
  DWORD code = 0, sz = sizeof(code);
  WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &sz, nullptr);
  status = (int)code;
  if (ok) {
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(req, &avail) && avail) {
      string chunk(avail, 0); DWORD read = 0;
      WinHttpReadData(req, chunk.data(), avail, &read);
      result.append(chunk.data(), read);
    }
  }
  WinHttpCloseHandle(req); WinHttpCloseHandle(con); WinHttpCloseHandle(ses);
  return result;
}
static string GhPath(const string& file) {
  return "/repos/" + string(kOwner) + "/" + string(kRepo) + "/contents/" + file;
}
static bool PutFile(const string& file, const string& contentB64, const string& message, string& err) {
  int st = 0;
  string cur = Http(L"GET", Utf16(GhPath(file)), "", st);
  string sha;
  size_t p = cur.find("\"sha\"");
  if (st == 200 && p != string::npos) {
    size_t q = cur.find('"', cur.find(':', p) + 1);
    size_t e = cur.find('"', q + 1);
    sha = cur.substr(q + 1, e - q - 1);
  }
  string body = string("{\"message\":\"") + JsonEsc(message) + "\",\"content\":\"" + contentB64 +
                "\",\"branch\":\"" + kBranch + "\"";
  if (!sha.empty()) body += ",\"sha\":\"" + sha + "\"";
  body += "}";
  string res = Http(L"PUT", Utf16(GhPath(file)), body, st);
  if (st < 200 || st >= 300) {
    err = "Kayit hatasi " + std::to_string(st);
    ExtractStr(res, "message", 0, res.size(), err);
    return false;
  }
  return true;
}
static bool LoadMenu(string& err) {
  int st = 0;
  string res = Http(L"GET", Utf16(GhPath(kMenuPath) + "?ref=" + kBranch), "", st);
  if (st != 200) { err = "Menu okunamadi"; return false; }
  string b64; ExtractStr(res, "content", 0, res.size(), b64);
  b64.erase(std::remove(b64.begin(), b64.end(), '\n'), b64.end());
  DWORD len = 0;
  CryptStringToBinaryA(b64.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &len, nullptr, nullptr);
  string json(len, 0);
  CryptStringToBinaryA(b64.c_str(), 0, CRYPT_STRING_BASE64, (BYTE*)json.data(), &len, nullptr, nullptr);
  json.resize(len);
  if (!ParseMenu(json, gMenu)) { err = "Menu cozulemedi"; return false; }
  return true;
}
static bool SaveMenu(string& err) {
  string json = MenuToJson(gMenu);
  return PutFile(kMenuPath, B64((unsigned char*)json.data(), json.size()), "Menu guncellendi", err);
}
static bool ReadFileBytes(const wstring& path, vector<unsigned char>& out) {
  HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (f == INVALID_HANDLE_VALUE) return false;
  DWORD sz = GetFileSize(f, nullptr);
  if (sz > 900000) { CloseHandle(f); return false; }
  out.resize(sz); DWORD r;
  ReadFile(f, out.data(), sz, &r, nullptr); CloseHandle(f); out.resize(r);
  return !out.empty();
}
static string NewId() { return std::to_string(GetTickCount64()); }
static string GhTokenFromCli() {
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE rd = 0, wr = 0; CreatePipe(&rd, &wr, &sa, 0);
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.hStdOutput = wr; si.hStdError = wr; si.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION pi{};
  wchar_t cmd[] = L"gh auth token";
  if (!CreateProcessW(nullptr, cmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    CloseHandle(rd); CloseHandle(wr); return "";
  }
  CloseHandle(wr);
  string out; char buf[256]; DWORD n;
  while (ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n) out.append(buf, n);
  WaitForSingleObject(pi.hProcess, 5000);
  CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(rd);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
  return out;
}

static void Status(const wstring& t) { if (gStatus) SetWindowTextW(gStatus, t.c_str()); }
static Section* CurSec() {
  if (gCat < 0 || gCat >= (int)gMenu.sections.size()) return nullptr;
  return &gMenu.sections[gCat];
}
static Item* CurItem() {
  auto* s = CurSec();
  if (!s || gSel < 0 || gSel >= (int)s->items.size()) return nullptr;
  return &s->items[gSel];
}
static void PullFields() {
  Item* it = CurItem(); if (!it) return;
  wchar_t buf[1024];
  GetWindowTextW(gName, buf, 512); it->name = Utf8(buf);
  GetWindowTextW(gDesc, buf, 1024); it->desc = Utf8(buf);
  GetWindowTextW(gPrice, buf, 32); it->price = _wtof(buf);
}
static void PushFields() {
  Item* it = CurItem();
  if (!it) {
    SetWindowTextW(gName, L""); SetWindowTextW(gDesc, L""); SetWindowTextW(gPrice, L"");
    return;
  }
  SetWindowTextW(gName, Utf16(it->name).c_str());
  SetWindowTextW(gDesc, Utf16(it->desc).c_str());
  wchar_t pr[32]; swprintf(pr, 32, L"%.0f", it->price); SetWindowTextW(gPrice, pr);
}
static void SyncScroll() {
  auto* s = CurSec();
  int n = s ? (int)s->items.size() : 0;
  RECT rc; GetClientRect(gList, &rc);
  int vis = rc.bottom / ROW_H;
  SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS};
  si.nMin = 0; si.nMax = std::max(0, n - 1); si.nPage = vis; si.nPos = gScroll;
  SetScrollInfo(gList, SB_VERT, &si, TRUE);
}

static void RoundRectFill(HDC dc, RECT r, COLORREF c, int rad) {
  HBRUSH b = CreateSolidBrush(c);
  HPEN p = CreatePen(PS_SOLID, 1, c);
  HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
  RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
  SelectObject(dc, ob); SelectObject(dc, op);
  DeleteObject(b); DeleteObject(p);
}
static void TextAt(HDC dc, int x, int y, const wstring& t, COLORREF c, HFONT f) {
  SetBkMode(dc, TRANSPARENT); SetTextColor(dc, c); SelectObject(dc, f);
  TextOutW(dc, x, y, t.c_str(), (int)t.size());
}

static LRESULT CALLBACK ListProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
  case WM_PAINT: {
    PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
    RECT rc; GetClientRect(h, &rc);
    HBRUSH bg = CreateSolidBrush(C_BG);
    FillRect(dc, &rc, bg); DeleteObject(bg);
    auto* sec = CurSec();
    int n = sec ? (int)sec->items.size() : 0;
    if (!n) TextAt(dc, 28, 28, L"Bu kategoride urun yok. + Urun ekle.", C_MUTED, gFont);
    for (int i = gScroll; i < n; i++) {
      int y = (i - gScroll) * ROW_H + 8;
      if (y > rc.bottom) break;
      RECT card{12, y, rc.right - 12, y + ROW_H - 8};
      RoundRectFill(dc, card, i == gSel ? RGB(48, 28, 18) : C_ROW, 16);
      HPEN pen = CreatePen(PS_SOLID, 1, i == gSel ? C_EMBER : C_LINE);
      HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_BRUSH));
      SelectObject(dc, pen);
      RoundRect(dc, card.left, card.top, card.right, card.bottom, 16, 16);
      SelectObject(dc, op); DeleteObject(pen);
      RECT ph{card.left + 14, card.top + 12, card.left + 70, card.bottom - 12};
      RoundRectFill(dc, ph, RGB(42, 28, 22), 12);
      const auto& it = sec->items[i];
      TextAt(dc, ph.left + 8, ph.top + 16, it.image.empty() ? L"FOTO" : L"OK", C_MUTED, gFontSm);
      wstring nm = Utf16(it.name);
      wstring ds = Utf16(it.desc);
      TextAt(dc, 92, y + 16, nm, C_PAPER, gFontB);
      TextAt(dc, 92, y + 42, ds.empty() ? L"Aciklama yok" : ds, C_MUTED, gFontSm);
      wchar_t pr[40]; swprintf(pr, 40, L"%.0f TL", it.price);
      SIZE sz{}; SelectObject(dc, gFontB); GetTextExtentPoint32W(dc, pr, (int)wcslen(pr), &sz);
      TextAt(dc, card.right - 24 - sz.cx, y + 28, pr, C_GOLD, gFontB);
    }
    EndPaint(h, &ps);
    return 0;
  }
  case WM_LBUTTONDOWN: {
    int y = GET_Y_LPARAM(l);
    int i = gScroll + y / ROW_H;
    auto* sec = CurSec();
    if (sec && i >= 0 && i < (int)sec->items.size()) {
      PullFields();
      gSel = i;
      PushFields();
      InvalidateRect(h, nullptr, FALSE);
    }
    SetFocus(h);
    return 0;
  }
  case WM_MOUSEWHEEL: {
    int d = GET_WHEEL_DELTA_WPARAM(w) > 0 ? -1 : 1;
    gScroll = std::max(0, gScroll + d);
    SyncScroll(); InvalidateRect(h, nullptr, FALSE);
    return 0;
  }
  case WM_VSCROLL: {
    auto* sec = CurSec();
    int n = sec ? (int)sec->items.size() : 0;
    int maxs = std::max(0, n - 1);
    switch (LOWORD(w)) {
    case SB_LINEUP: gScroll--; break;
    case SB_LINEDOWN: gScroll++; break;
    case SB_PAGEUP: gScroll -= 4; break;
    case SB_PAGEDOWN: gScroll += 4; break;
    case SB_THUMBTRACK: gScroll = HIWORD(w); break;
    }
    gScroll = std::max(0, std::min(gScroll, maxs));
    SyncScroll(); InvalidateRect(h, nullptr, FALSE);
    return 0;
  }
  case WM_ERASEBKGND: return 1;
  }
  return DefWindowProcW(h, m, w, l);
}

static HWND gLblName, gLblPrice, gLblDesc, gBtnPhoto, gBtnDel, gBtnNew, gBtnSave;
static HWND gBtnAddCat, gBtnRenCat, gBtnDelCat, gBtnSite;
static int gListTop = 148;

enum {
  ID_SAVE = 201, ID_NEW = 202, ID_DEL = 203, ID_PHOTO = 204,
  ID_ADDCAT = 205, ID_RENCAT = 206, ID_DELCAT = 207, ID_SITE = 208,
  ID_CAT0 = 300, MAX_CAT = 32
};

static wstring TrimW(wstring s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == L' ' || s[a] == L'\t')) a++;
  while (b > a && (s[b - 1] == L' ' || s[b - 1] == L'\t')) b--;
  return s.substr(a, b - a);
}

static bool gAskDone = false;
static wchar_t gAskBuf[256];

static LRESULT CALLBACK AskProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
  case WM_CREATE: {
    auto* cs = (CREATESTRUCTW*)l;
    CreateWindowW(L"STATIC", (LPCWSTR)cs->lpCreateParams, WS_CHILD | WS_VISIBLE, 24, 18, 430, 24, h, 0, gInst, nullptr);
    HWND e = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", gAskBuf,
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                             24, 50, 430, 40, h, (HMENU)1, gInst, nullptr);
    SendMessageW(e, WM_SETFONT, (WPARAM)gFont, TRUE);
    SendMessageW(e, EM_SETLIMITTEXT, 80, 0);
    CreateWindowW(L"BUTTON", L"Tamam", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                  184, 108, 130, 42, h, (HMENU)IDOK, gInst, nullptr);
    CreateWindowW(L"BUTTON", L"Iptal", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                  324, 108, 130, 42, h, (HMENU)IDCANCEL, gInst, nullptr);
    SetFocus(e);
    SendMessageW(e, EM_SETSEL, 0, -1);
    return 0;
  }
  case WM_COMMAND:
    if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) {
      if (LOWORD(w) == IDOK) GetDlgItemTextW(h, 1, gAskBuf, 256);
      else gAskBuf[0] = 0;
      DestroyWindow(h);
    }
    return 0;
  case WM_CLOSE:
    gAskBuf[0] = 0;
    DestroyWindow(h);
    return 0;
  case WM_DESTROY:
    gAskDone = true;
    return 0;
  case WM_CTLCOLORSTATIC: {
    HDC dc = (HDC)w;
    SetTextColor(dc, C_PAPER); SetBkColor(dc, C_CARD);
    return (LRESULT)gCard;
  }
  case WM_CTLCOLOREDIT: {
    HDC dc = (HDC)w;
    SetTextColor(dc, C_PAPER); SetBkColor(dc, RGB(22, 14, 12));
    return (LRESULT)gEdit;
  }
  case WM_ERASEBKGND: {
    RECT rc; GetClientRect(h, &rc);
    FillRect((HDC)w, &rc, gCard);
    return 1;
  }
  }
  return DefWindowProcW(h, m, w, l);
}

static wstring AskText(HWND parent, const wchar_t* title, const wchar_t* prompt, const wchar_t* def) {
  static bool reg = false;
  if (!reg) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = AskProc; wc.hInstance = gInst; wc.hbrBackground = gCard;
    wc.lpszClassName = L"FiratAsk"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc); reg = true;
  }
  wcsncpy(gAskBuf, def ? def : L"", 255); gAskBuf[255] = 0;
  gAskDone = false;
  RECT pr; GetWindowRect(parent, &pr);
  int x = pr.left + ((pr.right - pr.left) - 500) / 2;
  int y = pr.top + ((pr.bottom - pr.top) - 190) / 2;
  HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"FiratAsk", title,
                             WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_POPUP,
                             x, y, 500, 190, parent, nullptr, gInst, (LPVOID)prompt);
  EnableWindow(parent, FALSE);
  MSG msg;
  while (!gAskDone && GetMessageW(&msg, nullptr, 0, 0)) {
    if (!IsDialogMessageW(dlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
  }
  EnableWindow(parent, TRUE);
  SetForegroundWindow(parent);
  return TrimW(gAskBuf);
}

static bool gSiteDone = false, gSiteOk = false;
static HWND gSiteName, gSiteSub, gSiteKick, gSiteNote;
static HFONT SiteFont(HWND e) {
  SendMessageW(e, WM_SETFONT, (WPARAM)gFont, TRUE);
  return gFont;
}
static LRESULT CALLBACK SiteProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
  case WM_CREATE: {
    auto mkL = [&](const wchar_t* t, int y) {
      HWND s = CreateWindowW(L"STATIC", t, WS_CHILD | WS_VISIBLE, 28, y, 460, 22, h, 0, gInst, nullptr);
      SendMessageW(s, WM_SETFONT, (WPARAM)gFontSm, TRUE);
    };
    auto mkE = [&](int y, int id, int ht, bool multi) {
      DWORD st = WS_CHILD | WS_VISIBLE | WS_TABSTOP | (multi ? (ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL) : ES_AUTOHSCROLL);
      HWND e = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", st, 28, y, 500, ht, h, (HMENU)(INT_PTR)id, gInst, nullptr);
      SiteFont(e);
      SendMessageW(e, EM_SETLIMITTEXT, 400, 0);
      return e;
    };
    mkL(L"Restoran adi", 16);
    gSiteName = mkE(40, 1, 38, false);
    mkL(L"Alt yazi (ornek: Pide · Lahmacun Salonu)", 86);
    gSiteSub = mkE(110, 2, 38, false);
    mkL(L"Ust satir (bos birakilirsa sitede yazmaz)", 156);
    gSiteKick = mkE(180, 3, 38, false);
    mkL(L"Sayfa alti yazi (fiyat notu vs. bos = gizle)", 226);
    gSiteNote = mkE(250, 4, 72, true);
    CreateWindowW(L"BUTTON", L"Tamam", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                  248, 340, 130, 42, h, (HMENU)IDOK, gInst, nullptr);
    CreateWindowW(L"BUTTON", L"Iptal", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                  388, 340, 130, 42, h, (HMENU)IDCANCEL, gInst, nullptr);
    SetWindowTextW(gSiteName, Utf16(gMenu.name).c_str());
    SetWindowTextW(gSiteSub, Utf16(gMenu.subtitle).c_str());
    SetWindowTextW(gSiteKick, Utf16(gMenu.kicker).c_str());
    SetWindowTextW(gSiteNote, Utf16(gMenu.note).c_str());
    return 0;
  }
  case WM_COMMAND:
    if (LOWORD(w) == IDOK) {
      wchar_t buf[1024];
      GetWindowTextW(gSiteName, buf, 512); gMenu.name = Utf8(TrimW(buf));
      GetWindowTextW(gSiteSub, buf, 512); gMenu.subtitle = Utf8(TrimW(buf));
      GetWindowTextW(gSiteKick, buf, 512); gMenu.kicker = Utf8(TrimW(buf));
      GetWindowTextW(gSiteNote, buf, 1024); gMenu.note = Utf8(TrimW(buf));
      gSiteOk = true;
      DestroyWindow(h);
    } else if (LOWORD(w) == IDCANCEL) {
      gSiteOk = false;
      DestroyWindow(h);
    }
    return 0;
  case WM_CLOSE: gSiteOk = false; DestroyWindow(h); return 0;
  case WM_DESTROY: gSiteDone = true; return 0;
  case WM_CTLCOLORSTATIC: {
    HDC dc = (HDC)w; SetTextColor(dc, C_PAPER); SetBkColor(dc, C_CARD); return (LRESULT)gCard;
  }
  case WM_CTLCOLOREDIT: {
    HDC dc = (HDC)w; SetTextColor(dc, C_PAPER); SetBkColor(dc, RGB(22, 14, 12)); return (LRESULT)gEdit;
  }
  case WM_ERASEBKGND: {
    RECT rc; GetClientRect(h, &rc); FillRect((HDC)w, &rc, gCard); return 1;
  }
  }
  return DefWindowProcW(h, m, w, l);
}

static bool EditSiteTexts(HWND parent) {
  static bool reg = false;
  if (!reg) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = SiteProc; wc.hInstance = gInst; wc.hbrBackground = gCard;
    wc.lpszClassName = L"FiratSite"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc); reg = true;
  }
  gSiteDone = false; gSiteOk = false;
  RECT pr; GetWindowRect(parent, &pr);
  int x = pr.left + ((pr.right - pr.left) - 570) / 2;
  int y = pr.top + ((pr.bottom - pr.top) - 440) / 2;
  HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"FiratSite", L"Site yazilari",
                             WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_POPUP,
                             x, y, 570, 440, parent, nullptr, gInst, nullptr);
  EnableWindow(parent, FALSE);
  MSG msg;
  while (!gSiteDone && GetMessageW(&msg, nullptr, 0, 0)) {
    if (!IsDialogMessageW(dlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
  }
  EnableWindow(parent, TRUE);
  SetForegroundWindow(parent);
  return gSiteOk;
}

static void LayoutCats(HWND h) {
  RECT rc; GetClientRect(h, &rc);
  int addW = 168;
  int x = 24, y = 92;
  int maxx = rc.right - 28 - addW;
  HDC dc = GetDC(h);
  SelectObject(dc, gFontB);
  for (int i = 0; i < MAX_CAT; i++) {
    HWND b = GetDlgItem(h, ID_CAT0 + i);
    if (!b) continue;
    if (i >= (int)gMenu.sections.size()) { ShowWindow(b, SW_HIDE); continue; }
    wstring t = Utf16(gMenu.sections[i].title);
    SetWindowTextW(b, t.c_str());
    SIZE sz{}; GetTextExtentPoint32W(dc, t.c_str(), (int)t.size(), &sz);
    int bw = sz.cx + 48; if (bw < 88) bw = 88;
    if (x + bw > maxx && x > 24) { x = 24; y += 50; }
    MoveWindow(b, x, y, bw, 44, TRUE);
    ShowWindow(b, SW_SHOW);
    x += bw + 10;
  }
  ReleaseDC(h, dc);
  if (x + addW > rc.right - 24 && x > 24) { x = 24; y += 50; }
  if (gBtnAddCat) MoveWindow(gBtnAddCat, x, y, addW, 44, TRUE);
  gListTop = y + 58;
}

static LRESULT CALLBACK BtnProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
  if (m == WM_MOUSEMOVE) {
    if (!GetPropW(h, L"hot")) {
      SetPropW(h, L"hot", (HANDLE)1);
      TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0};
      TrackMouseEvent(&t);
      InvalidateRect(h, nullptr, FALSE);
    }
  } else if (m == WM_MOUSELEAVE) {
    RemovePropW(h, L"hot");
    InvalidateRect(h, nullptr, FALSE);
  }
  return DefSubclassProc(h, m, w, l);
}
static HWND MakeBtn(HWND p, const wchar_t* t, int x, int y, int w, int h, int id, bool ember) {
  HWND b = CreateWindowW(L"BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                         x, y, w, h, p, (HMENU)(INT_PTR)id, gInst, nullptr);
  SetWindowLongPtrW(b, GWLP_USERDATA, ember ? 1 : 0);
  SetWindowSubclass(b, BtnProc, 1, 0);
  return b;
}
static HWND MakeEdit(HWND p, int x, int y, int w, int h, int id, bool multi) {
  DWORD st = WS_CHILD | WS_VISIBLE | WS_TABSTOP | (multi ? (ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL) : ES_AUTOHSCROLL);
  HWND e = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", st, x, y, w, h, p, (HMENU)(INT_PTR)id, gInst, nullptr);
  SendMessageW(e, WM_SETFONT, (WPARAM)gFont, TRUE);
  SendMessageW(e, EM_SETLIMITTEXT, 1000, 0);
  return e;
}
static HWND MakeLbl(HWND p, const wchar_t* t, int x, int y) {
  HWND s = CreateWindowW(L"STATIC", t, WS_CHILD | WS_VISIBLE, x, y, 320, 22, p, 0, gInst, nullptr);
  SendMessageW(s, WM_SETFONT, (WPARAM)gFontSm, TRUE);
  return s;
}
static void LayoutUI(HWND h) {
  if (!gList) return;
  LayoutCats(h);
  RECT rc; GetClientRect(h, &rc);
  int W = rc.right, H = rc.bottom;
  int side = std::min(440, std::max(340, W * 34 / 100));
  int sx = W - side - 20;
  int listW = sx - 44;
  int listH = H - gListTop - 82;
  if (listW < 280) listW = W - 40;
  if (listH < 160) listH = 160;
  MoveWindow(gList, 24, gListTop, listW, listH, TRUE);
  int fx = sx + 22, fw = side - 44;
  int ty = gListTop + 20;
  MoveWindow(gLblName, fx, ty, fw, 22, TRUE);
  MoveWindow(gName, fx, ty + 24, fw, 42, TRUE);
  MoveWindow(gLblPrice, fx, ty + 78, fw, 22, TRUE);
  MoveWindow(gPrice, fx, ty + 102, fw, 42, TRUE);
  MoveWindow(gLblDesc, fx, ty + 156, fw, 22, TRUE);
  int descTop = ty + 180;
  int delY = H - 88 - 56;
  int photoY = delY - 64;
  int descH = photoY - 16 - descTop;
  if (descH < 80) descH = 80;
  MoveWindow(gDesc, fx, descTop, fw, descH, TRUE);
  MoveWindow(gBtnPhoto, fx, photoY, fw, 52, TRUE);
  MoveWindow(gBtnDel, fx, delY, fw, 52, TRUE);
  int yb = H - 68;
  MoveWindow(gBtnNew, 24, yb, 180, 52, TRUE);
  MoveWindow(gBtnRenCat, 214, yb, 190, 52, TRUE);
  MoveWindow(gBtnDelCat, 414, yb, 180, 52, TRUE);
  MoveWindow(gBtnSave, W - 250, yb - 2, 230, 56, TRUE);
  MoveWindow(gBtnSite, W - 220, 18, 196, 44, TRUE);
  MoveWindow(gStatus, 360, 52, W - 600, 28, TRUE);
  SyncScroll();
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
  case WM_CREATE: {
    gFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    gFontB = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    gFontTitle = CreateFontW(-34, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Georgia");
    gFontSm = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    gBg = CreateSolidBrush(C_BG); gCard = CreateSolidBrush(C_CARD);
    gRow = CreateSolidBrush(C_ROW); gEdit = CreateSolidBrush(RGB(22, 14, 12));

    WNDCLASSW lc{};
    lc.lpfnWndProc = ListProc; lc.hInstance = gInst; lc.lpszClassName = L"FiratList";
    lc.hCursor = LoadCursor(nullptr, IDC_HAND); lc.hbrBackground = gBg;
    RegisterClassW(&lc);
    gList = CreateWindowW(L"FiratList", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL, 24, 150, 620, 430, h, 0, gInst, nullptr);

    gLblName = MakeLbl(h, L"Urun adi", 668, 168);
    gName = MakeEdit(h, 668, 192, 290, 42, 401, false);
    gLblPrice = MakeLbl(h, L"Fiyat (TL)", 668, 246);
    gPrice = MakeEdit(h, 668, 270, 290, 42, 402, false);
    gLblDesc = MakeLbl(h, L"Urun aciklamasi (duzenle)", 668, 324);
    gDesc = MakeEdit(h, 668, 348, 290, 140, 403, true);

    gBtnPhoto = MakeBtn(h, L"Fotograf ekle", 668, 500, 290, 52, ID_PHOTO, 0);
    gBtnDel = MakeBtn(h, L"Urunu sil", 668, 560, 290, 52, ID_DEL, 0);
    gBtnNew = MakeBtn(h, L"+ Yeni urun", 24, 592, 200, 52, ID_NEW, 0);
    gBtnRenCat = MakeBtn(h, L"Kategori adi", 236, 592, 210, 52, ID_RENCAT, 0);
    gBtnDelCat = MakeBtn(h, L"Kategoriyi sil", 458, 592, 200, 52, ID_DELCAT, 0);
    gBtnSave = MakeBtn(h, L"Kaydet ve yayinla", 720, 588, 252, 56, ID_SAVE, 1);
    gBtnAddCat = MakeBtn(h, L"+ Kategori", 24, 92, 168, 44, ID_ADDCAT, 0);
    gBtnSite = MakeBtn(h, L"Site yazilari", 800, 18, 196, 44, ID_SITE, 0);
    gStatus = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 240, 604, 460, 28, h, 0, gInst, nullptr);
    SendMessageW(gStatus, WM_SETFONT, (WPARAM)gFontSm, TRUE);

    for (int i = 0; i < MAX_CAT; i++) {
      HWND b = CreateWindowW(L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW, 0, 0, 0, 0, h, (HMENU)(INT_PTR)(ID_CAT0 + i), gInst, nullptr);
      SetWindowSubclass(b, BtnProc, 1, 0);
    }

    string err;
    if (!LoadMenu(err)) Status(Utf16(err));
    else {
      gCat = 0; gSel = CurSec() && !CurSec()->items.empty() ? 0 : -1; PushFields();
      Status(L"Kategori ekle / ad degistir altta. Urunu sagda duzenle, sonra Kaydet.");
    }
    LayoutUI(h);
    return 0;
  }
  case WM_SIZE:
    LayoutUI(h);
    InvalidateRect(h, nullptr, TRUE);
    return 0;
  case WM_GETMINMAXINFO: {
    auto* mmi = (MINMAXINFO*)l;
    mmi->ptMinTrackSize = {900, 640};
    return 0;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
    RECT rc; GetClientRect(h, &rc);
    FillRect(dc, &rc, gBg);
    TextAt(dc, 28, 16, L"FIRAT KEBAP", C_GOLD, gFontTitle);
    TextAt(dc, 28, 56, L"Menu yonetimi  ·  firatkebap.github.io", C_MUTED, gFontSm);
    int side = std::min(420, std::max(320, (int)(rc.right * 36 / 100)));
    RECT sideR{rc.right - side - 20, gListTop, rc.right - 20, rc.bottom - 88};
    RoundRectFill(dc, sideR, C_CARD, 18);
    TextAt(dc, sideR.left + 22, gListTop - 26, L"Secili urun — duzenle", C_GOLD, gFontB);
    EndPaint(h, &ps);
    return 0;
  }
  case WM_DRAWITEM: {
    DRAWITEMSTRUCT* d = (DRAWITEMSTRUCT*)l;
    wchar_t cap[96]; GetWindowTextW(d->hwndItem, cap, 96);
    int id = d->CtlID;
    bool ember = GetWindowLongPtrW(d->hwndItem, GWLP_USERDATA) == 1;
    bool cat = id >= ID_CAT0 && id < ID_CAT0 + MAX_CAT;
    bool on = cat && (id - ID_CAT0) == gCat;
    bool hot = GetPropW(d->hwndItem, L"hot") != nullptr;
    bool down = (d->itemState & ODS_SELECTED) != 0;
    COLORREF fill = C_CARD;
    COLORREF tx = C_PAPER;
    COLORREF brd = C_LINE;
    if (ember) { fill = down ? RGB(180, 60, 0) : (hot ? RGB(255, 110, 20) : C_EMBER); tx = RGB(255,255,255); brd = fill; }
    else if (on) { fill = C_EMBER; tx = RGB(255,255,255); brd = C_EMBER; }
    else if (hot) { fill = RGB(52, 34, 26); brd = C_GOLD; }
    else if (cat) { fill = RGB(36, 24, 20); brd = C_LINE; }
    else { fill = RGB(40, 26, 20); brd = C_GOLD; }
    RECT r = d->rcItem;
    RoundRectFill(d->hDC, r, fill, 18);
    HPEN pen = CreatePen(PS_SOLID, 2, brd);
    HGDIOBJ op = SelectObject(d->hDC, pen);
    HGDIOBJ ob = SelectObject(d->hDC, GetStockObject(NULL_BRUSH));
    RoundRect(d->hDC, r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, 18, 18);
    SelectObject(d->hDC, op); SelectObject(d->hDC, ob); DeleteObject(pen);
    SetBkMode(d->hDC, TRANSPARENT); SetTextColor(d->hDC, tx);
    SelectObject(d->hDC, gFontB);
    DrawTextW(d->hDC, cap, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return TRUE;
  }
  case WM_CTLCOLORSTATIC: {
    HDC dc = (HDC)w;
    SetTextColor(dc, C_MUTED); SetBkColor(dc, C_BG);
    HWND ctl = (HWND)l;
    if (ctl == gLblName || ctl == gLblPrice || ctl == gLblDesc) {
      SetBkColor(dc, C_CARD);
      return (LRESULT)gCard;
    }
    return (LRESULT)gBg;
  }
  case WM_CTLCOLOREDIT: {
    HDC dc = (HDC)w;
    SetTextColor(dc, C_PAPER); SetBkColor(dc, RGB(22, 14, 12));
    return (LRESULT)gEdit;
  }
  case WM_COMMAND: {
    int id = LOWORD(w);
    int ntf = HIWORD(w);
    if ((id == 401 || id == 402 || id == 403) && (ntf == EN_KILLFOCUS || ntf == EN_CHANGE)) {
      if (ntf == EN_KILLFOCUS) PullFields();
      return 0;
    }
    if (id >= ID_CAT0 && id < ID_CAT0 + MAX_CAT) {
      int c = id - ID_CAT0;
      if (c >= (int)gMenu.sections.size()) return 0;
      PullFields();
      gCat = c; gSel = CurSec() && !CurSec()->items.empty() ? 0 : -1; gScroll = 0;
      PushFields(); SyncScroll(); InvalidateRect(h, nullptr, TRUE); InvalidateRect(gList, nullptr, FALSE);
      return 0;
    }
    string err;
    if (id == ID_SITE) {
      if (EditSiteTexts(h))
        Status(L"Site yazilari guncellendi. Kalici olmasi icin Kaydet.");
    } else if (id == ID_ADDCAT) {
      if ((int)gMenu.sections.size() >= MAX_CAT) { Status(L"En fazla 32 kategori."); break; }
      wstring name = AskText(h, L"Kategori ekle", L"Yeni kategori adi", L"Yeni kategori");
      if (name.empty()) break;
      PullFields();
      gMenu.sections.push_back(Section{NewId(), Utf8(name), {}});
      gCat = (int)gMenu.sections.size() - 1; gSel = -1; gScroll = 0;
      PushFields(); LayoutUI(h);
      InvalidateRect(h, nullptr, TRUE); InvalidateRect(gList, nullptr, FALSE);
      Status(L"Kategori eklendi. Urun ekle, sonra Kaydet ve yayinla.");
    } else if (id == ID_RENCAT) {
      auto* s = CurSec(); if (!s) break;
      wstring name = AskText(h, L"Kategori adi", L"Kategori adini yaz", Utf16(s->title).c_str());
      if (name.empty()) break;
      s->title = Utf8(name);
      LayoutUI(h); InvalidateRect(h, nullptr, TRUE);
      Status(L"Kategori adi degisti. Kalici olmasi icin Kaydet.");
    } else if (id == ID_DELCAT) {
      if (gMenu.sections.size() <= 1) { Status(L"Son kategoriyi silemezsin."); break; }
      auto* s = CurSec(); if (!s) break;
      wstring q = L"\"" + Utf16(s->title) + L"\" ve icindeki urunler silinsin mi?";
      if (MessageBoxW(h, q.c_str(), L"Kategoriyi sil", MB_YESNO | MB_ICONWARNING) != IDYES) break;
      gMenu.sections.erase(gMenu.sections.begin() + gCat);
      if (gCat >= (int)gMenu.sections.size()) gCat = (int)gMenu.sections.size() - 1;
      gSel = CurSec() && !CurSec()->items.empty() ? 0 : -1; gScroll = 0;
      PushFields(); LayoutUI(h);
      InvalidateRect(h, nullptr, TRUE); InvalidateRect(gList, nullptr, FALSE);
      Status(L"Kategori silindi. Kalici olmasi icin Kaydet.");
    } else if (id == ID_SAVE) {
      PullFields(); Status(L"Yayinlaniyor...");
      if (SaveMenu(err)) Status(L"Kaydedildi. Musteri menusu ~1 dk icinde guncellenir.");
      else Status(Utf16(err));
    } else if (id == ID_NEW) {
      auto* s = CurSec(); if (!s) break;
      PullFields();
      s->items.push_back(Item{NewId(), "Yeni urun", "", "", 0});
      gSel = (int)s->items.size() - 1;
      PushFields(); SyncScroll(); InvalidateRect(gList, nullptr, FALSE);
      Status(L"Yeni urun eklendi. Ad/fiyat yazip Kaydet.");
    } else if (id == ID_DEL) {
      auto* s = CurSec(); if (!s || gSel < 0) break;
      s->items.erase(s->items.begin() + gSel);
      gSel = s->items.empty() ? -1 : std::min(gSel, (int)s->items.size() - 1);
      PushFields(); InvalidateRect(gList, nullptr, FALSE);
      Status(L"Silindi. Kalici olmasi icin Kaydet.");
    } else if (id == ID_PHOTO) {
      Item* it = CurItem();
      if (!it) { Status(L"Once soldan urun secin."); break; }
      wchar_t file[MAX_PATH]{};
      OPENFILENAMEW of{sizeof(of)};
      of.hwndOwner = h; of.lpstrFile = file; of.nMaxFile = MAX_PATH;
      of.lpstrFilter = L"Fotograf (JPG, PNG)\0*.jpg;*.jpeg;*.png\0"; of.Flags = OFN_FILEMUSTEXIST;
      if (!GetOpenFileNameW(&of)) break;
      vector<unsigned char> bytes;
      if (!ReadFileBytes(file, bytes)) { Status(L"Dosya cok buyuk. 900 KB alti JPG/PNG secin."); break; }
      string name = string("uploads/") + NewId() + ".jpg";
      Status(L"Fotograf gonderiliyor...");
      if (!PutFile(name, B64(bytes.data(), bytes.size()), "Urun fotografi", err)) { Status(Utf16(err)); break; }
      it->image = string("https://raw.githubusercontent.com/") + kOwner + "/" + kRepo + "/main/" + name;
      PullFields();
      if (SaveMenu(err)) { Status(L"Fotograf yayinda."); InvalidateRect(gList, nullptr, FALSE); }
      else Status(Utf16(err));
    }
    return 0;
  }
  case WM_DESTROY:
    DeleteObject(gFont); DeleteObject(gFontB); DeleteObject(gFontTitle); DeleteObject(gFontSm);
    DeleteObject(gBg); DeleteObject(gCard); DeleteObject(gRow); DeleteObject(gEdit);
    PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

struct LoginData { wstring emailStr, p1, p2; bool ok = false; bool first = false; HWND emailHwnd = 0, p1w = 0, p2w = 0; };
static HWND DarkEdit(HWND p, int x, int y, int w, int id, bool pw) {
  DWORD st = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | (pw ? ES_PASSWORD : 0);
  HWND e = CreateWindowExW(0, L"EDIT", L"", st, x, y, w, 36, p, (HMENU)(INT_PTR)id, gInst, nullptr);
  SendMessageW(e, WM_SETFONT, (WPARAM)gFont, TRUE);
  return e;
}
static LRESULT CALLBACK LoginWnd(HWND h, UINT m, WPARAM w, LPARAM l) {
  LoginData* d = (LoginData*)GetWindowLongPtrW(h, GWLP_USERDATA);
  switch (m) {
  case WM_ERASEBKGND: {
    RECT rc; GetClientRect(h, &rc);
    FillRect((HDC)w, &rc, gBg);
    return 1;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
    RECT rc; GetClientRect(h, &rc);
    FillRect(dc, &rc, gBg);
    RECT card{70, 40, rc.right - 70, rc.bottom - 40};
    RoundRectFill(dc, card, C_CARD, 24);
    TextAt(dc, 100, 62, L"FIRAT KEBAP", C_GOLD, gFontTitle);
    TextAt(dc, 100, 108, d && d->first ? L"Ilk giris — hesap olustur" : L"Menu paneli", C_MUTED, gFontSm);
    EndPaint(h, &ps);
    return 0;
  }
  case WM_CTLCOLORSTATIC: {
    SetTextColor((HDC)w, C_MUTED); SetBkColor((HDC)w, C_CARD); return (LRESULT)gCard;
  }
  case WM_CTLCOLOREDIT: {
    SetTextColor((HDC)w, C_PAPER); SetBkColor((HDC)w, RGB(22,14,12)); return (LRESULT)gEdit;
  }
  case WM_DRAWITEM: {
    DRAWITEMSTRUCT* ds = (DRAWITEMSTRUCT*)l;
    RoundRectFill(ds->hDC, ds->rcItem, C_EMBER, 22);
    SetBkMode(ds->hDC, TRANSPARENT); SetTextColor(ds->hDC, RGB(255,255,255));
    SelectObject(ds->hDC, gFontB);
    DrawTextW(ds->hDC, L"Giriş", -1, &ds->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return TRUE;
  }
  case WM_COMMAND:
    if (LOWORD(w) == IDCANCEL) { if (d) d->ok = false; DestroyWindow(h); return 0; }
    if (LOWORD(w) == IDOK && d) {
      wchar_t a[256], b[256], c[256]{};
      GetWindowTextW(d->emailHwnd, a, 256);
      GetWindowTextW(d->p1w, b, 256);
      if (d->p2w) GetWindowTextW(d->p2w, c, 256);
      d->emailStr = a; d->p1 = b; d->p2 = c; d->ok = true;
      DestroyWindow(h); return 0;
    }
    break;
  case WM_CLOSE: if (d) d->ok = false; DestroyWindow(h); return 0;
  case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
static bool RunLogin() {
  gFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
  gFontB = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
  gFontTitle = CreateFontW(-32, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Georgia");
  gFontSm = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
  gBg = CreateSolidBrush(C_BG); gCard = CreateSolidBrush(C_CARD); gEdit = CreateSolidBrush(RGB(22,14,12));

  const wchar_t* cls = L"FiratLoginDark";
  WNDCLASSW wc{};
  wc.lpfnWndProc = LoginWnd; wc.hInstance = gInst; wc.hbrBackground = gBg;
  wc.lpszClassName = cls; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);
  LoginData d{};
  string blob;
  d.first = !ProtectRead(ConfigPath(), blob);
  HWND dlg = CreateWindowExW(0, cls, L"Firat Kebap",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                             520, d.first ? 520 : 430, nullptr, nullptr, gInst, nullptr);
  SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&d);
  CreateWindowW(L"STATIC", L"E-posta", WS_CHILD | WS_VISIBLE, 100, 150, 300, 20, dlg, 0, gInst, nullptr);
  d.emailHwnd = DarkEdit(dlg, 100, 172, 300, 1001, false);
  CreateWindowW(L"STATIC", L"Sifre", WS_CHILD | WS_VISIBLE, 100, 218, 300, 20, dlg, 0, gInst, nullptr);
  d.p1w = DarkEdit(dlg, 100, 240, 300, 1002, true);
  if (d.first) {
    CreateWindowW(L"STATIC", L"Sifre tekrar", WS_CHILD | WS_VISIBLE, 100, 286, 300, 20, dlg, 0, gInst, nullptr);
    d.p2w = DarkEdit(dlg, 100, 308, 300, 1003, true);
  }
  CreateWindowW(L"BUTTON", L"Giris", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
                100, d.first ? 370 : 300, 300, 48, dlg, (HMENU)IDOK, gInst, nullptr);
  ShowWindow(dlg, SW_SHOW);
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    if (!IsDialogMessageW(dlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
  }
  if (!d.ok) return false;
  if (d.emailStr.find(L'@') == wstring::npos || d.p1.size() < 4) {
    MessageBoxW(nullptr, L"Gecerli e-posta ve en az 4 karakter sifre girin.", L"Firat Kebap", MB_ICONWARNING);
    return false;
  }
  if (d.first) {
    if (d.p1 != d.p2) { MessageBoxW(nullptr, L"Sifreler ayni degil.", L"Firat Kebap", MB_ICONWARNING); return false; }
    gToken = GhTokenFromCli();
    if (gToken.empty()) {
      MessageBoxW(nullptr, L"GitHub baglantisi yok. Bu PC'de bir kez gh auth login yapin.", L"Firat Kebap", MB_ICONERROR);
      return false;
    }
    return ProtectWrite(ConfigPath(), Utf8(d.emailStr) + "\n" + Sha256(Utf8(d.p1)) + "\n" + gToken);
  }
  std::istringstream in(blob);
  string savedEmail, savedHash, savedTok;
  std::getline(in, savedEmail); std::getline(in, savedHash); std::getline(in, savedTok);
  if (Utf8(d.emailStr) != savedEmail || Sha256(Utf8(d.p1)) != savedHash) {
    MessageBoxW(nullptr, L"E-posta veya sifre yanlis.", L"Firat Kebap", MB_ICONWARNING);
    return false;
  }
  gToken = savedTok;
  return true;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
  SetProcessDPIAware();
  INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&icc);
  gInst = inst;
  if (!RunLogin()) return 0;
  WNDCLASSW wc{};
  wc.lpfnWndProc = MainProc; wc.hInstance = inst; wc.hbrBackground = gBg;
  wc.lpszClassName = L"FiratAdminMain"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  RegisterClassW(&wc);
  gMain = CreateWindowW(L"FiratAdminMain", L"Firat Kebap — Menu",
                        WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800, nullptr, nullptr, inst, nullptr);
  ShowWindow(gMain, SW_SHOWMAXIMIZED);
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    HWND f = GetFocus();
    if (f != gDesc && IsDialogMessageW(gMain, &msg)) continue;
    TranslateMessage(&msg); DispatchMessageW(&msg);
  }
  return 0;
}
