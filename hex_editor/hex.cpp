/*
-static-libgcc -lgdi32 -lcomctl32 -lcomdlg32 -municode -mwindows
*/

//#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <vector>
#include <string>
#include <cstring>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static const int BYTES_PER_LINE = 16;
static const int MARGIN         = 6;

#define IDC_HEXVIEW 1001
#define IDC_STATUS  1002

#define IDM_OPEN    40001
#define IDM_SAVE    40002
#define IDM_SAVEAS  40003
#define IDM_EXIT    40004
#define IDM_ABOUT   40005
#define IDM_FIND    40006

#define IDC_FIND_EDIT   2001
#define IDC_FIND_HEX    2002
#define IDC_FIND_ASCII  2003
#define IDC_FIND_NEXT   2004
#define IDC_FIND_PREV   2005

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static HINSTANCE g_hInst   = NULL;
static HWND      g_hMain   = NULL;
static HWND      g_hHex    = NULL;
static HWND      g_hStatus = NULL;
static HACCEL    g_hAccel  = NULL;

static std::vector<BYTE> g_data;
static std::wstring      g_filePath;
static bool              g_modified = false;

static HFONT g_hFont   = NULL;
static int   g_charW   = 8;
static int   g_lineH   = 16;

static int    g_topLine   = 0;
static size_t g_caret     = 0;
static int    g_nibble    = 0;
static bool   g_asciiMode = false;

// Find state
static HWND              g_hFindDlg        = NULL;
static std::vector<BYTE> g_findPattern;
static size_t            g_findMatchStart  = (size_t)-1;
static size_t            g_findMatchLen    = 0;

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
struct HexLayout {
    int charW, lineH;
    int addrX, hexX, asciiX;
    int linesVisible;
    int clientW, clientH;
};

static void GetLayout(HWND hwnd, HexLayout& L) {
    RECT rc; GetClientRect(hwnd, &rc);
    L.clientW = rc.right;
    L.clientH = rc.bottom;
    L.charW   = g_charW;
    L.lineH   = g_lineH;
    L.addrX   = MARGIN;
    L.hexX    = L.addrX + 8 * L.charW + L.charW;
    L.asciiX  = L.hexX + BYTES_PER_LINE * 3 * L.charW + L.charW;
    int h = rc.bottom - 2 * MARGIN;
    L.linesVisible = (h > 0 && L.lineH > 0) ? h / L.lineH : 1;
    if (L.linesVisible < 1) L.linesVisible = 1;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static int TotalLines() {
    if (g_data.empty()) return 0;
    return (int)((g_data.size() + BYTES_PER_LINE - 1) / BYTES_PER_LINE);
}

static int HexDigit(wchar_t c) {
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= L'a' && c <= L'f') return c - L'a' + 10;
    if (c >= L'A' && c <= L'F') return c - L'A' + 10;
    return -1;
}

static void UpdateScrollBar(HWND hwnd) {
    HexLayout L; GetLayout(hwnd, L);
    int total  = TotalLines();
    int maxTop = total - L.linesVisible;
    if (maxTop < 0) maxTop = 0;
    if (g_topLine > maxTop) g_topLine = maxTop;
    if (g_topLine < 0)      g_topLine = 0;

    SCROLLINFO si = { sizeof(si) };
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin  = 0;
    si.nMax  = (total > 0) ? total - 1 : 0;
    si.nPage = (UINT)L.linesVisible;
    si.nPos  = g_topLine;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

static void SetTitle() {
    std::wstring name = g_filePath.empty() ? L"Untitled" : g_filePath;
    size_t p = name.find_last_of(L"\\/");
    if (p != std::wstring::npos) name = name.substr(p + 1);

    std::wstring t = L"Hex Editor - " + name + (g_modified ? L" *" : L"");
    SetWindowTextW(g_hMain, t.c_str());
}

static void UpdateStatus() {
    if (!g_hStatus) return;
    wchar_t buf[256];
    if (g_data.empty())
        wsprintfW(buf, L"Empty file");
    else
        wsprintfW(buf, L"Offset: 0x%08X    Size: 0x%08X (%u bytes)",
                  (unsigned)g_caret, (unsigned)g_data.size(), (unsigned)g_data.size());

    SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)buf);

    wchar_t mode[64];
    wsprintfW(mode, L"%s", g_asciiMode ? L"ASCII mode" : L"HEX mode");
    SendMessageW(g_hStatus, SB_SETTEXTW, 1, (LPARAM)mode);
}

static void EnsureVisible();

static void MarkModified() {
    if (!g_modified) {
        g_modified = true;
        SetTitle();
    }
    UpdateStatus();
}

static void MoveCaret(size_t pos, int nibble, bool ascii) {
    if (g_data.empty()) return;
    if (pos >= g_data.size()) pos = g_data.size() - 1;
    g_caret = pos;
    if (nibble >= 0) g_nibble = nibble;
    g_asciiMode = ascii;
    EnsureVisible();
}

static void EnsureVisible() {
    if (g_data.empty()) return;
    HexLayout L; GetLayout(g_hHex, L);
    int line = (int)(g_caret / BYTES_PER_LINE);
    if (line < g_topLine) g_topLine = line;
    if (line >= g_topLine + L.linesVisible) g_topLine = line - L.linesVisible + 1;
    if (g_topLine < 0) g_topLine = 0;

    UpdateScrollBar(g_hHex);
    InvalidateRect(g_hHex, NULL, FALSE);
    UpdateStatus();
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------
static void PaintHex(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc; GetClientRect(hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) { EndPaint(hwnd, &ps); return; }

    HDC     mem    = CreateCompatibleDC(hdc);
    HBITMAP bmp    = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    HFONT   oldFnt = (HFONT)SelectObject(mem, g_hFont);

    FillRect(mem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SetBkMode(mem, TRANSPARENT);

    HexLayout L; GetLayout(hwnd, L);

    HBRUSH brCurLine = CreateSolidBrush(RGB(238, 246, 255));
    HBRUSH brCaret   = CreateSolidBrush(RGB(255, 150, 0));
    HBRUSH brSel     = CreateSolidBrush(RGB(170, 200, 255));
    HBRUSH brMatch   = CreateSolidBrush(RGB(255, 235, 120));

    int caretLine = g_data.empty() ? -1 : (int)(g_caret / BYTES_PER_LINE);
    wchar_t buf[16];

    for (int row = 0; row < L.linesVisible; ++row) {
        int    line = g_topLine + row;
        size_t off  = (size_t)line * BYTES_PER_LINE;
        if (off >= g_data.size()) break;

        int y = MARGIN + row * L.lineH;

        if (line == caretLine) {
            RECT r = { 0, y, rc.right, y + L.lineH };
            FillRect(mem, &r, brCurLine);
        }

        wsprintfW(buf, L"%08X", (unsigned)off);
        SetTextColor(mem, RGB(120, 120, 120));
        TextOutW(mem, L.addrX, y, buf, 8);

        for (int i = 0; i < BYTES_PER_LINE; ++i) {
            size_t o = off + i;
            if (o >= g_data.size()) break;

            BYTE b   = g_data[o];
            int  hx  = L.hexX   + i * 3 * L.charW;
            int  ax  = L.asciiX + i * L.charW;
            bool cur = (o == g_caret);
            bool printable = (b >= 0x20 && b < 0x7F);

            // current-byte selection background
            if (cur && !g_asciiMode) {
                RECT r = { hx, y, hx + 2 * L.charW, y + L.lineH };
                FillRect(mem, &r, brSel);
            }
            if (cur && g_asciiMode) {
                RECT r = { ax, y, ax + L.charW, y + L.lineH };
                FillRect(mem, &r, brSel);
            }

            // find-match background
            if (g_findMatchLen > 0 &&
                o >= g_findMatchStart && o < g_findMatchStart + g_findMatchLen) {
                RECT r1 = { hx, y, hx + 2 * L.charW, y + L.lineH };
                RECT r2 = { ax, y, ax + L.charW,    y + L.lineH };
                FillRect(mem, &r1, brMatch);
                FillRect(mem, &r2, brMatch);
            }

            // HEX value
            wsprintfW(buf, L"%02X", b);
            SetTextColor(mem, RGB(0, 0, 0));
            TextOutW(mem, hx, y, buf, 2);

            // ASCII char
            wchar_t ch = printable ? (wchar_t)b : L'.';
            SetTextColor(mem, printable ? RGB(0, 100, 160) : RGB(180, 180, 180));
            TextOutW(mem, ax, y, &ch, 1);
        }
    }

    if (!g_data.empty() && !g_asciiMode && g_caret < g_data.size()) {
        int line = (int)(g_caret / BYTES_PER_LINE);
        if (line >= g_topLine && line < g_topLine + L.linesVisible) {
            int row = line - g_topLine;
            int y   = MARGIN + row * L.lineH;
            int i   = (int)(g_caret % BYTES_PER_LINE);
            int hx  = L.hexX + i * 3 * L.charW + g_nibble * L.charW;

            RECT r = { hx, y, hx + L.charW, y + L.lineH };
            FillRect(mem, &r, brCaret);

            int v = (g_data[g_caret] >> (g_nibble == 0 ? 4 : 0)) & 0x0F;
            wchar_t hc = (wchar_t)(v < 10 ? (L'0' + v) : (L'A' + v - 10));
            SetTextColor(mem, RGB(255, 255, 255));
            TextOutW(mem, hx, y, &hc, 1);
        }
    }

    DeleteObject(brCurLine);
    DeleteObject(brCaret);
    DeleteObject(brSel);
    DeleteObject(brMatch);

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);

    SelectObject(mem, oldFnt);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

// ---------------------------------------------------------------------------
// Find logic
// ---------------------------------------------------------------------------
static bool ParseFindPattern(const std::wstring& text, bool hexMode,
                             std::vector<BYTE>& out)
{
    out.clear();
    if (hexMode) {
        int hi = -1;
        for (wchar_t c : text) {
            if (iswspace(c)) continue;
            int v = HexDigit(c);
            if (v < 0) return false;
            if (hi < 0) hi = v;
            else { out.push_back((BYTE)((hi << 4) | v)); hi = -1; }
        }
        if (hi >= 0) return false;
    } else {
        for (wchar_t c : text) {
            if (c > 0xFF) return false;
            out.push_back((BYTE)c);
        }
    }
    return !out.empty();
}

static size_t FindForward(size_t from) {
    if (g_findPattern.empty() || g_data.size() < g_findPattern.size())
        return (size_t)-1;
    size_t last = g_data.size() - g_findPattern.size();
    for (size_t i = from; i <= last; ++i) {
        if (memcmp(&g_data[i], g_findPattern.data(), g_findPattern.size()) == 0)
            return i;
    }
    for (size_t i = 0; i < from && i <= last; ++i) {
        if (memcmp(&g_data[i], g_findPattern.data(), g_findPattern.size()) == 0)
            return i;
    }
    return (size_t)-1;
}

static size_t FindBackward(size_t from) {
    if (g_findPattern.empty() || g_data.size() < g_findPattern.size())
        return (size_t)-1;
    size_t last = g_data.size() - g_findPattern.size();
    if (from > last) from = last;
    for (size_t i = from + 1; i-- > 0; ) {
        if (memcmp(&g_data[i], g_findPattern.data(), g_findPattern.size()) == 0)
            return i;
    }
    for (size_t i = last + 1; i-- > from + 1; ) {
        if (memcmp(&g_data[i], g_findPattern.data(), g_findPattern.size()) == 0)
            return i;
    }
    return (size_t)-1;
}

static void DoFind(bool forward) {
    if (g_data.empty() || !g_hFindDlg) return;

    wchar_t buf[512] = L"";
    GetDlgItemTextW(g_hFindDlg, IDC_FIND_EDIT, buf, 512);
    bool hexMode = IsDlgButtonChecked(g_hFindDlg, IDC_FIND_HEX) == BST_CHECKED;

    std::vector<BYTE> pat;
    if (!ParseFindPattern(buf, hexMode, pat)) {
        MessageBoxW(g_hFindDlg, L"Invalid pattern.", L"Find",
                    MB_OK | MB_ICONWARNING);
        return;
    }
    g_findPattern = pat;

    size_t found = (size_t)-1;
    if (forward) {
        size_t from = g_caret + 1;
        if (from > g_data.size()) from = 0;
        found = FindForward(from);
    } else {
        size_t from = (g_caret > 0) ? g_caret - 1 : 0;
        found = FindBackward(from);
    }

    if (found == (size_t)-1) {
        g_findMatchStart = (size_t)-1;
        g_findMatchLen   = 0;
        InvalidateRect(g_hHex, NULL, FALSE);
        MessageBoxW(g_hFindDlg, L"Not found.", L"Find", MB_OK | MB_ICONINFORMATION);
        return;
    }

    g_findMatchStart = found;
    g_findMatchLen   = g_findPattern.size();
    MoveCaret(found, 0, false);
    InvalidateRect(g_hHex, NULL, FALSE);
}

static LRESULT CALLBACK FindProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hf = (HFONT)SendMessageW(g_hMain, WM_GETFONT, 0, 0);
        if (!hf) hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
            10, 10, 300, 24, hwnd, (HMENU)IDC_FIND_EDIT, g_hInst, NULL);

        CreateWindowExW(0, L"BUTTON", L"Hex bytes",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
            10, 44, 120, 22, hwnd, (HMENU)IDC_FIND_HEX, g_hInst, NULL);
        CreateWindowExW(0, L"BUTTON", L"ASCII text",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            140, 44, 120, 22, hwnd, (HMENU)IDC_FIND_ASCII, g_hInst, NULL);
        CheckDlgButton(hwnd, IDC_FIND_HEX, BST_CHECKED);

        CreateWindowExW(0, L"BUTTON", L"Find Next",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
            10, 80, 100, 28, hwnd, (HMENU)IDC_FIND_NEXT, g_hInst, NULL);
        CreateWindowExW(0, L"BUTTON", L"Find Prev",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            120, 80, 100, 28, hwnd, (HMENU)IDC_FIND_PREV, g_hInst, NULL);

        for (HWND c = GetWindow(hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
            SendMessageW(c, WM_SETFONT, (WPARAM)hf, TRUE);

        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_FIND_NEXT: DoFind(true);  return 0;
        case IDC_FIND_PREV: DoFind(false); return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_hFindDlg = NULL;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void ShowFindDialog() {
    if (g_hFindDlg) {
        SetForegroundWindow(g_hFindDlg);
        SetFocus(GetDlgItem(g_hFindDlg, IDC_FIND_EDIT));
        return;
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = FindProc;
    wc.hInstance     = g_hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"HexFindClass";
    RegisterClassExW(&wc);

    g_hFindDlg = CreateWindowExW(WS_EX_TOOLWINDOW, L"HexFindClass", L"Find",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 340, 160,
                                 g_hMain, NULL, g_hInst, NULL);
    if (g_hFindDlg) {
        ShowWindow(g_hFindDlg, SW_SHOW);
        UpdateWindow(g_hFindDlg);
    }
}

// ---------------------------------------------------------------------------
// Hex view window proc
// ---------------------------------------------------------------------------
static LRESULT CALLBACK HexProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        UpdateScrollBar(hwnd);
        return 0;

    case WM_SIZE:
        UpdateScrollBar(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        PaintHex(hwnd);
        return 0;

    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_VSCROLL: {
        HexLayout L; GetLayout(hwnd, L);
        int total  = TotalLines();
        int maxTop = total - L.linesVisible;
        if (maxTop < 0) maxTop = 0;

        int pos = g_topLine;
        switch (LOWORD(wParam)) {
        case SB_LINEUP:   pos -= 1; break;
        case SB_LINEDOWN: pos += 1; break;
        case SB_PAGEUP:   pos -= L.linesVisible; break;
        case SB_PAGEDOWN: pos += L.linesVisible; break;
        case SB_TOP:      pos  = 0; break;
        case SB_BOTTOM:   pos  = maxTop; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si = { sizeof(si) };
            si.fMask = SIF_TRACKPOS;
            GetScrollInfo(hwnd, SB_VERT, &si);
            pos = si.nTrackPos;
            break;
        }
        }
        if (pos < 0) pos = 0;
        if (pos > maxTop) pos = maxTop;
        g_topLine = pos;

        UpdateScrollBar(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (TotalLines() == 0) return 0;
        int delta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        HexLayout L; GetLayout(hwnd, L);
        int maxTop = TotalLines() - L.linesVisible;
        if (maxTop < 0) maxTop = 0;

        g_topLine -= delta * 3;
        if (g_topLine < 0) g_topLine = 0;
        if (g_topLine > maxTop) g_topLine = maxTop;

        UpdateScrollBar(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (g_data.empty()) return 0;
        SetFocus(hwnd);

        HexLayout L; GetLayout(hwnd, L);
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int row = (y - MARGIN) / L.lineH;
        if (row < 0) row = 0;

        int    line = g_topLine + row;
        size_t off  = (size_t)line * BYTES_PER_LINE;
        if (off >= g_data.size()) return 0;

        if (x >= L.asciiX) {
            int i = (x - L.asciiX) / L.charW;
            if (i < 0) i = 0;
            if (i > BYTES_PER_LINE - 1) i = BYTES_PER_LINE - 1;
            size_t p = off + i;
            if (p >= g_data.size()) p = g_data.size() - 1;
            MoveCaret(p, 0, true);
        }
        else if (x >= L.hexX) {
            int rx = x - L.hexX;
            int i  = rx / (3 * L.charW);
            if (i < 0) i = 0;
            if (i > BYTES_PER_LINE - 1) i = BYTES_PER_LINE - 1;
            int nib = ((rx % (3 * L.charW)) >= L.charW) ? 1 : 0;
            size_t p = off + i;
            if (p >= g_data.size()) p = g_data.size() - 1;
            MoveCaret(p, nib, false);
        }
        return 0;
    }

    case WM_KEYDOWN: {
        if (g_data.empty()) break;
        size_t sz = g_data.size();
        bool moved = true;

        switch (wParam) {
        case VK_LEFT:
            if (g_asciiMode) {
                if (g_caret > 0) --g_caret;
            } else {
                if (g_nibble == 1) g_nibble = 0;
                else if (g_caret > 0) { --g_caret; g_nibble = 1; }
            }
            break;

        case VK_RIGHT:
            if (g_asciiMode) {
                if (g_caret + 1 < sz) ++g_caret;
            } else {
                if (g_nibble == 0) g_nibble = 1;
                else if (g_caret + 1 < sz) { ++g_caret; g_nibble = 0; }
            }
            break;

        case VK_UP:
            g_caret = (g_caret >= BYTES_PER_LINE) ? g_caret - BYTES_PER_LINE : 0;
            break;

        case VK_DOWN:
            g_caret = (g_caret + BYTES_PER_LINE < sz) ? g_caret + BYTES_PER_LINE : sz - 1;
            break;

        case VK_HOME:
            g_caret = (g_caret / BYTES_PER_LINE) * BYTES_PER_LINE;
            break;

        case VK_END: {
            size_t s = (g_caret / BYTES_PER_LINE) * BYTES_PER_LINE;
            size_t e = s + BYTES_PER_LINE - 1;
            g_caret = (e < sz) ? e : sz - 1;
            break;
        }

        case VK_PRIOR: {
            HexLayout L; GetLayout(hwnd, L);
            size_t d = (size_t)L.linesVisible * BYTES_PER_LINE;
            g_caret = (g_caret > d) ? g_caret - d : 0;
            break;
        }

        case VK_NEXT: {
            HexLayout L; GetLayout(hwnd, L);
            size_t d = (size_t)L.linesVisible * BYTES_PER_LINE;
            g_caret = (g_caret + d < sz) ? g_caret + d : sz - 1;
            break;
        }

        default:
            moved = false;
        }

        if (moved) EnsureVisible();
        return 0;
    }

    case WM_CHAR: {
        if (g_data.empty()) return 0;
        wchar_t ch = (wchar_t)wParam;

        if (g_asciiMode) {
            if (ch >= 0x20 && ch < 0x7F) {
                g_data[g_caret] = (BYTE)ch;
                MarkModified();
                if (g_caret + 1 < g_data.size()) ++g_caret;
                EnsureVisible();
            }
        } else {
            int v = HexDigit(ch);
            if (v >= 0) {
                BYTE b = g_data[g_caret];
                if (g_nibble == 0) b = (BYTE)((b & 0x0F) | (v << 4));
                else               b = (BYTE)((b & 0xF0) | v);
                g_data[g_caret] = b;
                MarkModified();

                if (g_nibble == 0) {
                    g_nibble = 1;
                } else {
                    g_nibble = 0;
                    if (g_caret + 1 < g_data.size()) ++g_caret;
                }
                EnsureVisible();
            }
        }
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------
static bool LoadFile(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_hMain, L"Cannot open file.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    LARGE_INTEGER li;
    GetFileSizeEx(h, &li);
    if (li.QuadPart > 0xFFFFFFFFull) {
        CloseHandle(h);
        MessageBoxW(g_hMain, L"Files larger than 4GB are not supported.",
                    L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    g_data.resize((size_t)li.QuadPart);
    if (li.QuadPart > 0) {
        DWORD read = 0;
        if (!ReadFile(h, g_data.data(), (DWORD)li.QuadPart, &read, NULL)
            || read != (DWORD)li.QuadPart) {
            CloseHandle(h);
            g_data.clear();
            MessageBoxW(g_hMain, L"Failed to read file.", L"Error", MB_OK | MB_ICONERROR);
            return false;
        }
    }
    CloseHandle(h);

    g_filePath = path;
    g_modified = false;
    g_topLine  = 0;
    g_caret    = 0;
    g_nibble   = 0;
    g_asciiMode= false;
    g_findMatchStart = (size_t)-1;
    g_findMatchLen   = 0;

    SetTitle();
    UpdateScrollBar(g_hHex);
    InvalidateRect(g_hHex, NULL, FALSE);
    UpdateStatus();
    return true;
}

static bool SaveToFile(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_hMain, L"Cannot write file.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    DWORD written = 0;
    if (!g_data.empty()) {
        if (!WriteFile(h, g_data.data(), (DWORD)g_data.size(), &written, NULL)
            || written != (DWORD)g_data.size()) {
            CloseHandle(h);
            MessageBoxW(g_hMain, L"Failed to write file.", L"Error", MB_OK | MB_ICONERROR);
            return false;
        }
    }
    CloseHandle(h);

    g_filePath = path;
    g_modified = false;
    SetTitle();
    return true;
}

static bool DoSaveAs() {
    wchar_t path[MAX_PATH] = L"";
    if (!g_filePath.empty()) lstrcpynW(path, g_filePath.c_str(), MAX_PATH);

    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner   = g_hMain;
    ofn.lpstrFilter = L"All Files\0*.*\0Binary Files\0*.bin\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrDefExt = L"bin";
    ofn.Flags       = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

    if (!GetSaveFileNameW(&ofn)) return false;
    return SaveToFile(path);
}

static bool DoSave() {
    if (g_filePath.empty()) return DoSaveAs();
    return SaveToFile(g_filePath);
}

static bool MaybeSave() {
    if (!g_modified) return true;
    int r = MessageBoxW(g_hMain, L"File has been modified. Save changes?",
                        L"Hex Editor", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDYES) return DoSave();
    if (r == IDNO)  return true;
    return false;
}

static void DoOpen() {
    if (!MaybeSave()) return;

    wchar_t path[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner   = g_hMain;
    ofn.lpstrFilter = L"All Files\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameW(&ofn)) LoadFile(path);
}

// ---------------------------------------------------------------------------
// Main window proc
// ---------------------------------------------------------------------------
static void LayoutChildren(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);

    int statusH = 0;
    if (g_hStatus) {
        SendMessageW(g_hStatus, WM_SIZE, 0, 0);
        RECT rs; GetWindowRect(g_hStatus, &rs);
        statusH = rs.bottom - rs.top;
    }
    if (g_hHex) {
        MoveWindow(g_hHex, 0, 0, rc.right, rc.bottom - statusH, TRUE);
    }
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HDC hdc = GetDC(hwnd);
        g_hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        HFONT old = (HFONT)SelectObject(hdc, g_hFont);

        TEXTMETRICW tm;
        GetTextMetricsW(hdc, &tm);
        g_lineH = tm.tmHeight;

        SIZE sz;
        GetTextExtentPoint32W(hdc, L"0", 1, &sz);
        g_charW = sz.cx;

        SelectObject(hdc, old);
        ReleaseDC(hwnd, hdc);

        g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, NULL,
                                    WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                    0, 0, 0, 0, hwnd, (HMENU)IDC_STATUS, g_hInst, NULL);
        int parts[] = { 320, -1 };
        SendMessageW(g_hStatus, SB_SETPARTS, 2, (LPARAM)parts);

        g_hHex = CreateWindowExW(WS_EX_CLIENTEDGE, L"HexViewClass", NULL,
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
                                 0, 0, 100, 100, hwnd, (HMENU)IDC_HEXVIEW, g_hInst, NULL);
        SendMessageW(g_hHex, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        SetTitle();
        UpdateStatus();
        return 0;
    }

    case WM_SIZE:
        LayoutChildren(hwnd);
        return 0;

    case WM_SETFOCUS:
        if (g_hHex) SetFocus(g_hHex);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_OPEN:   DoOpen();         return 0;
        case IDM_SAVE:   DoSave();         return 0;
        case IDM_SAVEAS: DoSaveAs();       return 0;
        case IDM_FIND:   ShowFindDialog(); return 0;
        case IDM_EXIT:   SendMessageW(hwnd, WM_CLOSE, 0, 0); return 0;
        case IDM_ABOUT:
            MessageBoxW(hwnd,
                L"Win32 Hex Editor\n\n"
                L"\u2022 By Pbcoding.\n"
                L"\u2022 Luogu: uid1210725.\n"
				L"\u2022 Github: pbcoding123.\n",
                L"About", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;

    case WM_CLOSE:
        if (MaybeSave()) DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    g_hInst = hInst;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wcHex = { sizeof(wcHex) };
    wcHex.style         = CS_HREDRAW | CS_VREDRAW;
    wcHex.lpfnWndProc   = HexProc;
    wcHex.hInstance     = hInst;
    wcHex.hCursor       = LoadCursor(NULL, IDC_IBEAM);
    wcHex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcHex.lpszClassName = L"HexViewClass";
    RegisterClassExW(&wcHex);

    WNDCLASSEXW wcMain = { sizeof(wcMain) };
    wcMain.style         = CS_HREDRAW | CS_VREDRAW;
    wcMain.lpfnWndProc   = MainProc;
    wcMain.hInstance     = hInst;
    wcMain.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcMain.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcMain.lpszClassName = L"HexEditMainClass";
    wcMain.lpszMenuName  = NULL;
    RegisterClassExW(&wcMain);

    HMENU hMenu = CreateMenu();
    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, IDM_OPEN,   L"&Open...\tCtrl+O");
    AppendMenuW(hFile, MF_STRING, IDM_SAVE,   L"&Save\tCtrl+S");
    AppendMenuW(hFile, MF_STRING, IDM_SAVEAS, L"Save &As...");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_EXIT,   L"E&xit");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");

    HMENU hEdit = CreatePopupMenu();
    AppendMenuW(hEdit, MF_STRING, IDM_FIND, L"&Find...\tCtrl+F");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEdit, L"&Edit");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, IDM_ABOUT, L"&About...");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    g_hMain = CreateWindowExW(0, L"HexEditMainClass", L"Hex Editor",
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 760, 620,
                              NULL, hMenu, hInst, NULL);
    if (!g_hMain) return 1;

    ACCEL accel[] = {
        { FCONTROL | FVIRTKEY, 'O', IDM_OPEN },
        { FCONTROL | FVIRTKEY, 'S', IDM_SAVE },
        { FCONTROL | FVIRTKEY, 'F', IDM_FIND },
    };
    g_hAccel = CreateAcceleratorTableW(accel, 3);

    ShowWindow(g_hMain, nCmdShow);
    UpdateWindow(g_hMain);
    LayoutChildren(g_hMain);
    SetFocus(g_hHex);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(g_hMain, g_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}
