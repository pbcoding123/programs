#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ID_STATIC_TIP     101
#define ID_EDIT_PWD       102
#define ID_BTN_UNLOCK     103
#define ID_STATIC_ERROR   104

HINSTANCE g_hInst = NULL;
HWND g_hLockWnd = NULL;
HWND g_hEditPwd = NULL, g_hBtnUnlock = NULL, g_hTxtError = NULL;
HFONT g_hFontTitle = NULL, g_hFontInput = NULL;
HBRUSH g_hBgBrush = NULL, g_hEditBrush = NULL, g_hBtnBrush = NULL;
HHOOK g_hKeyboardHook = NULL;

wchar_t g_szTitle[256] = {0};
wchar_t g_szBtnText[64] = {0};
wchar_t g_szErrorText[256] = {0};
wchar_t g_szCorrectPwd[256] = {0};

bool g_bUnlocked = false;
int g_ScrW = 0, g_ScrH = 0;
UINT_PTR g_dwTimerID = 0;

#define COLOR_BG        RGB(32, 32, 32)
#define COLOR_TEXT      RGB(240, 240, 240)
#define COLOR_EDIT_BG   RGB(45, 45, 48)
#define COLOR_BTN_BG    RGB(0, 120, 215)

static void SafeWcsCpy(wchar_t* dst, size_t size, const wchar_t* src) {
    if (!dst || size == 0) return;
    if (!src) { dst[0] = L'\0'; return; }
    wcsncpy(dst, src, size - 1);
    dst[size - 1] = L'\0';
}

void SetSystemPolicies(BOOL bDisable) {
    DWORD dwValue = bDisable ? 1 : 0;

    const wchar_t* pKeySystem   = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
    const wchar_t* pKeyExplorer = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";

    const wchar_t* pKeys[] = {
        pKeySystem, pKeySystem, pKeySystem, pKeySystem, pKeyExplorer
    };
    const wchar_t* pValues[] = {
        L"DisableChangePassword",
        L"DisableLockWorkstation",
        L"DisableSwitchUserOption",
        L"DisableTaskmgr",
        L"NoLogoff"
    };

    for (int i = 0; i < 5; i++) {
        HKEY hKey = NULL;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, pKeys[i], 0, NULL, 0,
                            KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, pValues[i], 0, REG_DWORD,
                           (const BYTE*)&dwValue, sizeof(DWORD));
            RegCloseKey(hKey);
        }
    }
}

static void OnExitRestorePolicies() {
    SetSystemPolicies(FALSE);
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* /*info*/) {
    SetSystemPolicies(FALSE);
    return EXCEPTION_EXECUTE_HANDLER;
}

int Base64Decode(const char* input, char* output) {
    if (!input || !output) return -1;
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int T[256] = {0};
    for (int i = 0; i < 64; i++) T[(unsigned char)base64_chars[i]] = i + 1;

    int inputLen = strlen(input);
    int outputLen = 0;
    int i = 0, j = 0;
    unsigned char char_array_4[4], char_array_3[3];

    while (inputLen-- && *input && *input != '=') {
        if (!T[(unsigned char)*input]) { input++; continue; }
        char_array_4[i++] = *(input++);
        if (i == 4) {
            for (i = 0; i < 4; i++) char_array_4[i] = T[char_array_4[i]] - 1;
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            for (i = 0; i < 3; i++) output[outputLen++] = char_array_3[i];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 4; j++) char_array_4[j] = 0;
        for (j = 0; j < 4; j++) char_array_4[j] = T[char_array_4[j]] - 1;
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        for (j = 0; j < (i - 1); j++) output[outputLen++] = char_array_3[j];
    }
    output[outputLen] = '\0';
    return outputLen;
}

void LoadConfig() {
    SafeWcsCpy(g_szTitle,     256, L"System Locked!");
    SafeWcsCpy(g_szBtnText,    64, L"Unlock");
    SafeWcsCpy(g_szErrorText, 256, L"Wrong Password!");
    SafeWcsCpy(g_szCorrectPwd,256, L"secret123");

    FILE* fp = fopen("lock.cfg", "r");
    if (!fp) return;

    char buffer[512] = {0};
    int lineIndex = 0;

    while (fgets(buffer, sizeof(buffer), fp)) {
        int len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r' || buffer[len - 1] == ' ')) {
            buffer[--len] = '\0';
        }

        if (len == 0 || buffer[0] == '#') continue;

        char decoded[256] = {0};
        if (Base64Decode(buffer, decoded) > 0) {
            wchar_t wDecoded[256] = {0};
            MultiByteToWideChar(CP_UTF8, 0, decoded, -1, wDecoded, 256);

            if (lineIndex == 0)      SafeWcsCpy(g_szTitle,      256, wDecoded);
            else if (lineIndex == 1) SafeWcsCpy(g_szBtnText,     64, wDecoded);
            else if (lineIndex == 2) SafeWcsCpy(g_szErrorText,  256, wDecoded);
            else if (lineIndex == 3) SafeWcsCpy(g_szCorrectPwd, 256, wDecoded);

            lineIndex++;
            if (lineIndex >= 4) break;
        }
    }
    fclose(fp);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
        if (p->vkCode == VK_LWIN || p->vkCode == VK_RWIN) return 1;
        if (p->vkCode == VK_ESCAPE && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) return 1;
        if ((p->vkCode == VK_TAB || p->vkCode == VK_ESCAPE) && (p->flags & LLKHF_ALTDOWN)) return 1;
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

BOOL CreateLockScreenWindow();
LRESULT CALLBACK LockWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
void CheckAndKeepWindowOnTop();
void VerifyPassword(HWND hWnd);

BOOL CreateLockScreenWindow() {
    g_ScrW = GetSystemMetrics(SM_CXSCREEN);
    g_ScrH = GetSystemMetrics(SM_CYSCREEN);

    if (!g_hBgBrush)   g_hBgBrush   = CreateSolidBrush(COLOR_BG);
    if (!g_hEditBrush) g_hEditBrush = CreateSolidBrush(COLOR_EDIT_BG);
    if (!g_hBtnBrush)  g_hBtnBrush  = CreateSolidBrush(COLOR_BTN_BG);

    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = LockWndProc;
    wc.hInstance     = g_hInst;
    wc.lpszClassName = L"LockScreenClass";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_hBgBrush;
    wc.style         = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc)) return FALSE;

    g_hLockWnd = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"Locked",
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, g_ScrW, g_ScrH, NULL, NULL, g_hInst, NULL);
    if (g_hLockWnd == NULL) return FALSE;

    g_hFontTitle = CreateFontW(28, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    g_hFontInput = CreateFontW(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int cx = g_ScrW / 2;
    int cy = g_ScrH / 2;

    CreateWindowExW(0, L"STATIC", g_szTitle, WS_CHILD | WS_VISIBLE | SS_CENTER,
        cx - 200, cy - 150, 400, 50, g_hLockWnd, (HMENU)ID_STATIC_TIP, g_hInst, NULL);

    g_hEditPwd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_CENTER,
        cx - 150, cy - 60, 300, 40, g_hLockWnd, (HMENU)ID_EDIT_PWD, g_hInst, NULL);

    g_hBtnUnlock = CreateWindowExW(0, L"STATIC", g_szBtnText, WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOTIFY,
        cx - 75, cy + 10, 150, 40, g_hLockWnd, (HMENU)ID_BTN_UNLOCK, g_hInst, NULL);

    g_hTxtError = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
        cx - 200, cy + 70, 400, 30, g_hLockWnd, (HMENU)ID_STATIC_ERROR, g_hInst, NULL);

    HWND hTip = GetDlgItem(g_hLockWnd, ID_STATIC_TIP);
    SendMessageW(hTip, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);
    SendMessageW(g_hEditPwd, WM_SETFONT, (WPARAM)g_hFontInput, TRUE);
    SendMessageW(g_hBtnUnlock, WM_SETFONT, (WPARAM)g_hFontInput, TRUE);
    SendMessageW(g_hTxtError, WM_SETFONT, (WPARAM)g_hFontInput, TRUE);

    SetFocus(g_hEditPwd);
    return TRUE;
}

LRESULT CALLBACK LockWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_TIMER:
            if (wp == g_dwTimerID) CheckAndKeepWindowOnTop();
            break;

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wp;
            HWND hCtrl = (HWND)lp;
            int id = GetDlgCtrlID(hCtrl);

            SetTextColor(hdc, COLOR_TEXT);
            SetBkMode(hdc, TRANSPARENT);

            if (id == ID_BTN_UNLOCK) {
                SetBkColor(hdc, COLOR_BTN_BG);
                return (LRESULT)g_hBtnBrush;
            }
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, COLOR_TEXT);
            SetBkColor(hdc, COLOR_EDIT_BG);
            return (LRESULT)g_hEditBrush;
        }

        case WM_KEYDOWN:
            if (wp == VK_RETURN) VerifyPassword(hWnd);
            if (wp == VK_ESCAPE) return 0;
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == ID_BTN_UNLOCK) VerifyPassword(hWnd);
            break;

        case WM_CLOSE: return 0;
        case WM_QUERYENDSESSION: return TRUE;
        case WM_DESTROY:
            if (g_dwTimerID) KillTimer(hWnd, g_dwTimerID);
            PostQuitMessage(0);
            break;
        default: return DefWindowProc(hWnd, msg, wp, lp);
    }
    return 0;
}

void VerifyPassword(HWND hWnd) {
    wchar_t szPwd[256] = {0};
    GetWindowTextW(g_hEditPwd, szPwd, 256);

    if (wcscmp(szPwd, g_szCorrectPwd) == 0) {
        g_bUnlocked = true;
        PostQuitMessage(0);
    } else {
        SetWindowTextW(g_hEditPwd, L"");
        SetWindowTextW(g_hTxtError, g_szErrorText);
        SetFocus(g_hEditPwd);
    }
}

void CheckAndKeepWindowOnTop() {
    if (g_bUnlocked || !g_hLockWnd) return;
    SetWindowPos(g_hLockWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nCmd) {
    g_hInst = hInstance;

    atexit(OnExitRestorePolicies);
    SetUnhandledExceptionFilter(CrashHandler);

    LoadConfig();

    SetSystemPolicies(TRUE);

    g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hInst, 0);

    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (hTaskbar) ShowWindow(hTaskbar, SW_HIDE);

    if (!CreateLockScreenWindow()) {
        if (g_hKeyboardHook) UnhookWindowsHookEx(g_hKeyboardHook);
        if (hTaskbar) ShowWindow(hTaskbar, SW_SHOW);
        SetSystemPolicies(FALSE);
        return 1;
    }

    g_dwTimerID = SetTimer(g_hLockWnd, 1, 500, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_hKeyboardHook) UnhookWindowsHookEx(g_hKeyboardHook);
    if (hTaskbar) ShowWindow(hTaskbar, SW_SHOW);

    SetSystemPolicies(FALSE);
    return 0;
}
