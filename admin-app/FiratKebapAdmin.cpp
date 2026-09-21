#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

static const wchar_t kApp[] = L"Firat Kebap Admin";
static const char kOwner[] = "ykula89";
static const char kRepo[] = "Firatkebap";
static const char kBranch[] = "main";
static const char kMenuPath[] = "data/menu.json";

struct Item { string id, name, desc, image; double price = 0; };
struct Section { string id, title; vector<Item> items; };
struct Menu { string name, subtitle, kicker; vector<Section> sections; };

static HINSTANCE gInst;
static HWND gMain, gList, gName, gDesc, gPrice, gStatus;
static Menu gMenu;
static string gToken;

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
    << "\",\n  \"kicker\": \"" << JsonEsc(m.kicker) << "\",\n  \"sections\": [\n";
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
  ExtractStr(json, "name", 0, json.size(), m.name);
  ExtractStr(json, "subtitle", 0, json.size(), m.subtitle);
  ExtractStr(json, "kicker", 0, json.size(), m.kicker);
  size_t secKey = json.find("\"sections\"");
  if (secKey == string::npos) return false;
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
    err = "GitHub hata " + std::to_string(st);
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
  if (sz > 700000) { CloseHandle(f); return false; }
  out.resize(sz); DWORD r;
  ReadFile(f, out.data(), sz, &r, nullptr); CloseHandle(f); out.resize(r);
  return !out.empty();
}
static void SetStatus(const wstring& t) { SetWindowTextW(gStatus, t.c_str()); }
static void FillList() {
  ListView_DeleteAllItems(gList);
  int row = 0;
  for (size_t s = 0; s < gMenu.sections.size(); s++) {
    for (size_t i = 0; i < gMenu.sections[s].items.size(); i++) {
      const auto& it = gMenu.sections[s].items[i];
      wstring cat = Utf16(gMenu.sections[s].title);
      LVITEMW lv{}; lv.mask = LVIF_TEXT | LVIF_PARAM; lv.iItem = row;
      lv.pszText = cat.data(); lv.lParam = (LPARAM)((s << 16) | i);
      ListView_InsertItem(gList, &lv);
      wstring nm = Utf16(it.name);
      ListView_SetItemText(gList, row, 1, nm.data());
      wchar_t pr[32]; swprintf(pr, 32, L"%.0f TL", it.price);
      ListView_SetItemText(gList, row, 2, pr);
      row++;
    }
  }
}
static bool Selected(size_t& s, size_t& i) {
  int row = ListView_GetNextItem(gList, -1, LVNI_SELECTED);
  if (row < 0) return false;
  LVITEMW lv{}; lv.mask = LVIF_PARAM; lv.iItem = row; ListView_GetItem(gList, &lv);
  s = ((size_t)lv.lParam >> 16) & 0xffff; i = (size_t)lv.lParam & 0xffff;
  return s < gMenu.sections.size() && i < gMenu.sections[s].items.size();
}
static void ShowItem() {
  size_t s, i; if (!Selected(s, i)) return;
  auto& it = gMenu.sections[s].items[i];
  SetWindowTextW(gName, Utf16(it.name).c_str());
  SetWindowTextW(gDesc, Utf16(it.desc).c_str());
  wchar_t pr[32]; swprintf(pr, 32, L"%.0f", it.price); SetWindowTextW(gPrice, pr);
}
static void GrabFields() {
  size_t s, i; if (!Selected(s, i)) return;
  wchar_t buf[512];
  GetWindowTextW(gName, buf, 512); gMenu.sections[s].items[i].name = Utf8(buf);
  GetWindowTextW(gDesc, buf, 512); gMenu.sections[s].items[i].desc = Utf8(buf);
  GetWindowTextW(gPrice, buf, 32); gMenu.sections[s].items[i].price = _wtof(buf);
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

enum { ID_LIST = 100, ID_NAME, ID_DESC, ID_PRICE, ID_SAVE, ID_NEW, ID_DEL, ID_PHOTO, ID_RELOAD, ID_STATUS };

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
  case WM_CREATE: {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);
    gList = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                          10, 10, 760, 320, h, (HMENU)ID_LIST, gInst, nullptr);
    ListView_SetExtendedListViewStyle(gList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    LVCOLUMNW c{LVCF_TEXT | LVCF_WIDTH};
    c.cx = 160; c.pszText = (LPWSTR)L"Kategori"; ListView_InsertColumn(gList, 0, &c);
    c.cx = 420; c.pszText = (LPWSTR)L"Urun"; ListView_InsertColumn(gList, 1, &c);
    c.cx = 140; c.pszText = (LPWSTR)L"Fiyat"; ListView_InsertColumn(gList, 2, &c);
    CreateWindowW(L"STATIC", L"Ad", WS_CHILD | WS_VISIBLE, 10, 340, 40, 22, h, 0, gInst, nullptr);
    gName = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 60, 338, 280, 24, h, (HMENU)ID_NAME, gInst, nullptr);
    CreateWindowW(L"STATIC", L"Fiyat", WS_CHILD | WS_VISIBLE, 360, 340, 40, 22, h, 0, gInst, nullptr);
    gPrice = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 410, 338, 80, 24, h, (HMENU)ID_PRICE, gInst, nullptr);
    CreateWindowW(L"STATIC", L"Aciklama", WS_CHILD | WS_VISIBLE, 10, 372, 70, 22, h, 0, gInst, nullptr);
    gDesc = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 90, 370, 400, 24, h, (HMENU)ID_DESC, gInst, nullptr);
    CreateWindowW(L"BUTTON", L"Kaydet", WS_CHILD | WS_VISIBLE, 10, 410, 100, 32, h, (HMENU)ID_SAVE, gInst, nullptr);
    CreateWindowW(L"BUTTON", L"Yeni urun", WS_CHILD | WS_VISIBLE, 120, 410, 100, 32, h, (HMENU)ID_NEW, gInst, nullptr);
    CreateWindowW(L"BUTTON", L"Sil", WS_CHILD | WS_VISIBLE, 230, 410, 80, 32, h, (HMENU)ID_DEL, gInst, nullptr);
    CreateWindowW(L"BUTTON", L"Fotograf", WS_CHILD | WS_VISIBLE, 320, 410, 100, 32, h, (HMENU)ID_PHOTO, gInst, nullptr);
    CreateWindowW(L"BUTTON", L"Yenile", WS_CHILD | WS_VISIBLE, 430, 410, 90, 32, h, (HMENU)ID_RELOAD, gInst, nullptr);
    gStatus = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 10, 452, 760, 22, h, (HMENU)ID_STATUS, gInst, nullptr);
    string err;
    if (!LoadMenu(err)) SetStatus(Utf16(err));
    else { FillList(); SetStatus(L"Menu yuklendi. Degisince Kaydet'e basin."); }
    return 0;
  }
  case WM_NOTIFY:
    if (((LPNMHDR)l)->idFrom == ID_LIST && ((LPNMHDR)l)->code == LVN_ITEMCHANGED) ShowItem();
    return 0;
  case WM_COMMAND: {
    int id = LOWORD(w); string err;
    if (id == ID_SAVE) {
      GrabFields(); SetStatus(L"Kaydediliyor...");
      if (SaveMenu(err)) { FillList(); SetStatus(L"Kaydedildi. Site ~1 dk icinde guncellenir."); }
      else SetStatus(Utf16(err));
    } else if (id == ID_NEW) {
      if (gMenu.sections.empty()) break;
      gMenu.sections[0].items.push_back(Item{NewId(), "Yeni urun", "", "", 0});
      FillList();
      SetStatus(L"Yeni urun eklendi. Kaydet'e basin.");
    } else if (id == ID_DEL) {
      size_t s, i; if (!Selected(s, i)) break;
      gMenu.sections[s].items.erase(gMenu.sections[s].items.begin() + (ptrdiff_t)i);
      FillList(); SetStatus(L"Silindi. Kaydet'e basin.");
    } else if (id == ID_PHOTO) {
      size_t s, i; if (!Selected(s, i)) { SetStatus(L"Once urun secin."); break; }
      wchar_t file[MAX_PATH]{};
      OPENFILENAMEW of{sizeof(of)};
      of.hwndOwner = h; of.lpstrFile = file; of.nMaxFile = MAX_PATH;
      of.lpstrFilter = L"Fotograf\0*.jpg;*.jpeg;*.png\0"; of.Flags = OFN_FILEMUSTEXIST;
      if (!GetOpenFileNameW(&of)) break;
      vector<unsigned char> bytes;
      if (!ReadFileBytes(file, bytes)) { SetStatus(L"Dosya buyuk (max ~700KB) veya okunamadi."); break; }
      string name = string("uploads/") + NewId() + ".jpg";
      SetStatus(L"Yukleniyor...");
      if (!PutFile(name, B64(bytes.data(), bytes.size()), "Urun fotografi", err)) { SetStatus(Utf16(err)); break; }
      gMenu.sections[s].items[i].image =
          string("https://raw.githubusercontent.com/") + kOwner + "/" + kRepo + "/main/" + name;
      GrabFields();
      if (SaveMenu(err)) SetStatus(L"Fotograf yuklendi."); else SetStatus(Utf16(err));
    } else if (id == ID_RELOAD) {
      if (!LoadMenu(err)) SetStatus(Utf16(err)); else { FillList(); SetStatus(L"Yenilendi."); }
    }
    return 0;
  }
  case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

struct LoginData { wstring emailStr, p1, p2; bool ok = false; bool first = false; HWND emailHwnd = 0, p1w = 0, p2w = 0; };

static HWND MakeEdit(HWND p, int x, int y, int w, int id, bool pw) {
  DWORD st = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | (pw ? ES_PASSWORD : 0);
  return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", st, x, y, w, 26, p, (HMENU)(INT_PTR)id, gInst, nullptr);
}
static LRESULT CALLBACK LoginWnd(HWND h, UINT m, WPARAM w, LPARAM l) {
  LoginData* d = (LoginData*)GetWindowLongPtrW(h, GWLP_USERDATA);
  switch (m) {
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
  const wchar_t* cls = L"FiratLogin2";
  WNDCLASSW wc{};
  wc.lpfnWndProc = LoginWnd; wc.hInstance = gInst;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = cls; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);
  LoginData d{};
  string blob;
  d.first = !ProtectRead(ConfigPath(), blob);
  HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, cls, d.first ? L"Hesap olustur" : L"Giris - Firat Kebap",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                             450, d.first ? 330 : 270, nullptr, nullptr, gInst, nullptr);
  SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&d);
  CreateWindowW(L"STATIC", L"E-posta", WS_CHILD | WS_VISIBLE, 24, 20, 200, 20, dlg, 0, gInst, nullptr);
  d.emailHwnd = MakeEdit(dlg, 24, 42, 380, 1001, false);
  CreateWindowW(L"STATIC", L"Sifre", WS_CHILD | WS_VISIBLE, 24, 78, 200, 20, dlg, 0, gInst, nullptr);
  d.p1w = MakeEdit(dlg, 24, 100, 380, 1002, true);
  if (d.first) {
    CreateWindowW(L"STATIC", L"Sifre tekrar", WS_CHILD | WS_VISIBLE, 24, 136, 200, 20, dlg, 0, gInst, nullptr);
    d.p2w = MakeEdit(dlg, 24, 158, 380, 1003, true);
    CreateWindowW(L"STATIC", L"Ilk acilista GitHub oturumu bu PC'den alinir. Sonra sadece e-posta/sifre.",
                  WS_CHILD | WS_VISIBLE, 24, 198, 380, 36, dlg, 0, gInst, nullptr);
  }
  CreateWindowW(L"BUTTON", L"Tamam", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 220, d.first ? 250 : 160, 90, 32, dlg, (HMENU)IDOK, gInst, nullptr);
  CreateWindowW(L"BUTTON", L"Iptal", WS_CHILD | WS_VISIBLE, 320, d.first ? 250 : 160, 84, 32, dlg, (HMENU)IDCANCEL, gInst, nullptr);
  ShowWindow(dlg, SW_SHOW);
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    if (!IsDialogMessageW(dlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
  }
  if (!d.ok) return false;
  if (d.emailStr.find(L'@') == wstring::npos || d.p1.size() < 4) {
    MessageBoxW(nullptr, L"Gecerli e-posta ve en az 4 karakter sifre girin.", kApp, MB_ICONWARNING);
    return false;
  }
  if (d.first) {
    if (d.p1 != d.p2) { MessageBoxW(nullptr, L"Sifreler ayni degil.", kApp, MB_ICONWARNING); return false; }
    gToken = GhTokenFromCli();
    if (gToken.empty()) {
      MessageBoxW(nullptr, L"GitHub baglantisi yok. Bu PC'de bir kez gh auth login yapin.", kApp, MB_ICONERROR);
      return false;
    }
    return ProtectWrite(ConfigPath(), Utf8(d.emailStr) + "\n" + Sha256(Utf8(d.p1)) + "\n" + gToken);
  }
  std::istringstream in(blob);
  string savedEmail, savedHash, savedTok;
  std::getline(in, savedEmail); std::getline(in, savedHash); std::getline(in, savedTok);
  if (Utf8(d.emailStr) != savedEmail || Sha256(Utf8(d.p1)) != savedHash) {
    MessageBoxW(nullptr, L"E-posta veya sifre yanlis.", kApp, MB_ICONWARNING);
    return false;
  }
  gToken = savedTok;
  return true;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
  gInst = inst;
  if (!RunLogin()) return 0;
  INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx(&icc);
  WNDCLASSW wc{};
  wc.lpfnWndProc = MainProc; wc.hInstance = inst;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = L"FiratAdminMain"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);
  gMain = CreateWindowW(L"FiratAdminMain", kApp, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                        CW_USEDEFAULT, CW_USEDEFAULT, 800, 520, nullptr, nullptr, inst, nullptr);
  ShowWindow(gMain, show);
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
  return 0;
}
