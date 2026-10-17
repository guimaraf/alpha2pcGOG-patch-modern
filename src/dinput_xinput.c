#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* WASAPI GUIDs defined statically for standalone compilation without libuuid dependency */
static const GUID MY_CLSID_MMDeviceEnumerator = 
    { 0xbcde0395, 0xe52f, 0x467c, { 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e } };
static const GUID MY_IID_IMMDeviceEnumerator = 
    { 0xa95664d2, 0x9614, 0x4f35, { 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6 } };
static const GUID MY_IID_IAudioSessionManager = 
    { 0xbfa971f1, 0x4d37, 0x482e, { 0xa3, 0x4a, 0x32, 0xe4, 0x6b, 0x19, 0xe4, 0x56 } };

/* XInput definitions */
#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD    30

/*
 * Accurate SFA2 WinMM Button Bitmask matching gameplay polling at 0x0054dac7:
 * Button 0 (1 << 0): sets dl |= 0x40  --> LK (Light Kick)
 * Button 1 (1 << 1): sets al |= 0x20  --> MP (Medium Punch)
 * Button 2 (1 << 2): sets cl |= 0x08  --> HP (Heavy Punch)
 * Button 3 (1 << 3): sets dl |= 0x80  --> MK (Medium Kick)
 * Button 4 (1 << 4): sets al |= 0x10  --> LP (Light Punch)
 * Button 5 (1 << 5): sets cl |= 0x04  --> HK (Heavy Kick)
 * Button 8 (1 << 8): sets ch |= 0x08  --> START (bit 11 = 0x800 in pl_sw_buff0)
 * Button 9 (1 << 9): sets dh |= 0x01  --> SELECT / COIN (bit 8 = 0x100 in pl_sw_buff0)
 */
#define SFA2_BTN_LK    (1 << 0)
#define SFA2_BTN_MP    (1 << 1)
#define SFA2_BTN_HP    (1 << 2)
#define SFA2_BTN_MK    (1 << 3)
#define SFA2_BTN_LP    (1 << 4)
#define SFA2_BTN_HK    (1 << 5)
#define SFA2_BTN_START (1 << 8)
#define SFA2_BTN_COIN  (1 << 9)

typedef struct {
    WORD  wButtons;
    BYTE  bLeftTrigger;
    BYTE  bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
} XINPUT_GAMEPAD_DATA;

typedef struct {
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD_DATA Gamepad;
} XINPUT_STATE_DATA;

typedef DWORD (WINAPI *PFN_XInputGetState)(DWORD dwUserIndex, XINPUT_STATE_DATA* pState);
typedef MMRESULT (WINAPI *PFN_joyGetPosEx)(UINT uJoyID, LPJOYINFOEX pji);

/* Function pointer to original dinput.dll DirectInputCreateA */
typedef HRESULT (WINAPI *PFN_DirectInputCreateA)(HINSTANCE hinst, DWORD dwVersion, LPVOID *ppDI, void *punkOuter);
typedef HRESULT (WINAPI *PFN_DirectInputCreateW)(HINSTANCE hinst, DWORD dwVersion, LPVOID *ppDI, void *punkOuter);
typedef HRESULT (WINAPI *PFN_DirectInputCreateEx)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, void *punkOuter);

static HMODULE g_hRealDInput = NULL;
static PFN_DirectInputCreateA g_pfnDirectInputCreateA = NULL;
static PFN_DirectInputCreateW g_pfnDirectInputCreateW = NULL;
static PFN_DirectInputCreateEx g_pfnDirectInputCreateEx = NULL;

static HMODULE g_hXInput = NULL;
static PFN_XInputGetState g_pfnXInputGetState = NULL;

static PFN_joyGetPosEx g_pfnRealJoyGetPosEx = NULL;

/* Configuration options */
static int g_cfgXInputEnabled = 1;
static int g_cfgDeadzone = 7849;
static int g_cfgEnableMacros = 1;      /* LB = 3x Punch, LT = 3x Kick */
static int g_cfgSwapTriggersBumpers = 0; /* 0: RB=HP, RT=HK. 1: RT=HP, RB=HK */
static int g_cfgPlayer1Index = 0;      /* XInput Index for Player 1 */
static int g_cfgPlayer2Index = 1;      /* XInput Index for Player 2 */
static int g_cfgForce60FPS = 1;        /* Lock 60 FPS and disable engine frame skips */
static int g_cfgEnableAltEnter = 1;    /* Allow Alt+Enter to toggle fullscreen / windowed */
static int g_cfgWindowedWidth = 960;   /* Windowed width (4:3 aspect ratio) */
static int g_cfgWindowedHeight = 720;  /* Windowed height (4:3 aspect ratio) */
static int g_cfgContinuousBGMOnKO = 1; /* Keep BGM playing continuously on K.O. (Arcade CPS2 behavior) */
static int g_cfgDuckBGMOnSuperKO = 1;  /* Duck BGM volume on Super KO finish */
static int g_cfgSuperKOBGMVolumePercent = 30; /* Volume percentage on Super KO (default: 30%) */
static int g_cfgSuperKOFadeDurationMS = 250;  /* Fade duration in ms when screen flashes (default: 250ms) */
static int g_cfgLogEnabled = 0;

static void LogMsg(const char* fmt, ...) {
    if (!g_cfgLogEnabled) return;
    FILE* f = fopen("sfa2_patch.log", "a");
    if (!f) return;
    va_list va;
    va_start(va, fmt);
    vfprintf(f, fmt, va);
    va_end(va);
    fclose(f);
}

static void LoadConfig(void) {
    char iniPath[MAX_PATH];
    GetModuleFileNameA(NULL, iniPath, MAX_PATH);
    char* p = strrchr(iniPath, '\\');
    if (p) *(p + 1) = '\0';
    strcat(iniPath, "sfa2_patch.ini");

    g_cfgXInputEnabled = GetPrivateProfileIntA("XInput", "Enabled", 1, iniPath);
    g_cfgDeadzone = GetPrivateProfileIntA("XInput", "Deadzone", 7849, iniPath);
    g_cfgEnableMacros = GetPrivateProfileIntA("XInput", "EnableMacros", 1, iniPath);
    g_cfgSwapTriggersBumpers = GetPrivateProfileIntA("XInput", "SwapTriggersBumpers", 0, iniPath);

    g_cfgPlayer1Index = GetPrivateProfileIntA("Controllers", "Player1_Index", 0, iniPath);
    g_cfgPlayer2Index = GetPrivateProfileIntA("Controllers", "Player2_Index", 1, iniPath);

    g_cfgForce60FPS = GetPrivateProfileIntA("Graphics", "Force60FPS", 1, iniPath);
    g_cfgEnableAltEnter = GetPrivateProfileIntA("Graphics", "EnableAltEnter", 1, iniPath);
    g_cfgWindowedWidth = GetPrivateProfileIntA("Graphics", "WindowedWidth", 960, iniPath);
    if (g_cfgWindowedWidth < 320) g_cfgWindowedWidth = 320;
    g_cfgWindowedHeight = GetPrivateProfileIntA("Graphics", "WindowedHeight", 720, iniPath);
    if (g_cfgWindowedHeight < 240) g_cfgWindowedHeight = 240;

    g_cfgContinuousBGMOnKO = GetPrivateProfileIntA("Audio", "ContinuousBGMOnKO", 1, iniPath);
    g_cfgDuckBGMOnSuperKO = GetPrivateProfileIntA("Audio", "DuckBGMOnSuperKO", 1, iniPath);
    g_cfgSuperKOBGMVolumePercent = GetPrivateProfileIntA("Audio", "SuperKOBGMVolumePercent", 30, iniPath);
    if (g_cfgSuperKOBGMVolumePercent < 0) g_cfgSuperKOBGMVolumePercent = 0;
    if (g_cfgSuperKOBGMVolumePercent > 100) g_cfgSuperKOBGMVolumePercent = 100;

    g_cfgSuperKOFadeDurationMS = GetPrivateProfileIntA("Audio", "SuperKOFadeDurationMS", 250, iniPath);
    if (g_cfgSuperKOFadeDurationMS < 0) g_cfgSuperKOFadeDurationMS = 0;
    if (g_cfgSuperKOFadeDurationMS > 5000) g_cfgSuperKOFadeDurationMS = 5000;

    g_cfgLogEnabled = GetPrivateProfileIntA("Debug", "EnableLog", 0, iniPath);

    LogMsg("Config loaded: XInput=%d, Deadzone=%d, Macros=%d, SwapTB=%d, P1Index=%d, P2Index=%d, Force60FPS=%d, AltEnter=%d (%dx%d), ContinuousBGM=%d, DuckSuper=%d, DuckVol=%d%%, FadeDur=%dms\n",
           g_cfgXInputEnabled, g_cfgDeadzone, g_cfgEnableMacros, g_cfgSwapTriggersBumpers,
           g_cfgPlayer1Index, g_cfgPlayer2Index, g_cfgForce60FPS, g_cfgEnableAltEnter,
           g_cfgWindowedWidth, g_cfgWindowedHeight, g_cfgContinuousBGMOnKO,
           g_cfgDuckBGMOnSuperKO, g_cfgSuperKOBGMVolumePercent, g_cfgSuperKOFadeDurationMS);
}

static void InitXInput(void) {
    const char* dllNames[] = {
        "xinput1_4.dll",
        "xinput1_3.dll",
        "xinput9_1_0.dll"
    };
    for (int i = 0; i < 3; i++) {
        g_hXInput = LoadLibraryA(dllNames[i]);
        if (g_hXInput) {
            g_pfnXInputGetState = (PFN_XInputGetState)GetProcAddress(g_hXInput, (LPCSTR)100);
            if (!g_pfnXInputGetState) {
                g_pfnXInputGetState = (PFN_XInputGetState)GetProcAddress(g_hXInput, "XInputGetState");
            }
            if (g_pfnXInputGetState) {
                LogMsg("Initialized XInput using %s\n", dllNames[i]);
                return;
            }
            FreeLibrary(g_hXInput);
            g_hXInput = NULL;
        }
    }
    LogMsg("Failed to load any XInput DLL\n");
}

/*
 * Application Title & Audio Session Management:
 * Renames "Win SF Alpha2 Application" to "Street Fighter Alpha 2 PC" across:
 * 1. Executable memory strings (0x007e6b94 & 0x007e6bc0) used by CreateWindowExA.
 * 2. Main Window title and Windows Taskbar (SetWindowTextA).
 * 3. Windows Volume Mixer (WASAPI IAudioSessionControl::SetDisplayName).
 */
static void ApplyTitlePatches(void) {
    const char newTitle[] = "Street Fighter Alpha 2 PC";
    DWORD oldProtect;

    /* 1. Fullscreen Window Name at 0x007e6b94 (28 bytes buffer in ALPHA2.EXE) */
    char* pTitle1 = (char*)0x007e6b94;
    if (VirtualProtect(pTitle1, 28, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memset(pTitle1, 0, 28);
        memcpy(pTitle1, newTitle, strlen(newTitle));
        VirtualProtect(pTitle1, 28, oldProtect, &oldProtect);
        LogMsg("Applied title patch at 0x007e6b94: '%s'\n", newTitle);
    }

    /* 2. Windowed Mode Window Name at 0x007e6bc0 (28 bytes buffer in ALPHA2.EXE) */
    char* pTitle2 = (char*)0x007e6bc0;
    if (VirtualProtect(pTitle2, 28, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memset(pTitle2, 0, 28);
        memcpy(pTitle2, newTitle, strlen(newTitle));
        VirtualProtect(pTitle2, 28, oldProtect, &oldProtect);
        LogMsg("Applied title patch at 0x007e6bc0: '%s'\n", newTitle);
    }
}

static int UpdateAudioSessionDisplayName(const WCHAR* wszName) {
    HRESULT hr;
    IMMDeviceEnumerator* pEnum = NULL;
    IMMDevice* pDevice = NULL;
    IAudioSessionManager* pMgr = NULL;
    IAudioSessionControl* pControl = NULL;
    int success = 0;

    CoInitialize(NULL);
    hr = CoCreateInstance(&MY_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER,
                          &MY_IID_IMMDeviceEnumerator, (void**)&pEnum);
    if (SUCCEEDED(hr) && pEnum) {
        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(pEnum, eRender, eMultimedia, &pDevice);
        if (SUCCEEDED(hr) && pDevice) {
            hr = IMMDevice_Activate(pDevice, &MY_IID_IAudioSessionManager, CLSCTX_INPROC_SERVER,
                                    NULL, (void**)&pMgr);
            if (SUCCEEDED(hr) && pMgr) {
                hr = IAudioSessionManager_GetAudioSessionControl(pMgr, NULL, 0, &pControl);
                if (SUCCEEDED(hr) && pControl) {
                    hr = IAudioSessionControl_SetDisplayName(pControl, wszName, NULL);
                    if (SUCCEEDED(hr)) {
                        success = 1;
                        LogMsg("Successfully updated Windows Volume Mixer session name to: %ls\n", wszName);
                    }
                    IAudioSessionControl_Release(pControl);
                }
                IAudioSessionManager_Release(pMgr);
            }
            IMMDevice_Release(pDevice);
        }
        IMMDeviceEnumerator_Release(pEnum);
    }
    return success;
}

/*
 * Window Mode Toggle (Alt + Enter)
 * Smoothly toggles between Borderless Fullscreen and Windowed mode with a 4:3 aspect ratio.
 */
static int g_bIsWindowed = 0;
static DWORD g_lastToggleTick = 0;

static void ToggleWindowedMode(void) {
    DWORD now = timeGetTime();
    if (now - g_lastToggleTick < 400) return; /* Debounce 400ms */
    g_lastToggleTick = now;

    HWND hWnd = *(HWND*)0x00bebf64;
    if (!hWnd || !IsWindow(hWnd)) {
        hWnd = FindWindowA("WinStzero2Class", NULL);
    }
    if (!hWnd || !IsWindow(hWnd)) return;

    if (!g_bIsWindowed) {
        /* Switch to Windowed Mode */
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;

        RECT rc = { 0, 0, g_cfgWindowedWidth, g_cfgWindowedHeight };
        AdjustWindowRect(&rc, style, FALSE);
        int winW = rc.right - rc.left;
        int winH = rc.bottom - rc.top;

        HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi;
        ZeroMemory(&mi, sizeof(mi));
        mi.cbSize = sizeof(mi);
        GetMonitorInfoA(hMon, &mi);

        int screenW = mi.rcWork.right - mi.rcWork.left;
        int screenH = mi.rcWork.bottom - mi.rcWork.top;
        int winX = mi.rcWork.left + (screenW - winW) / 2;
        int winY = mi.rcWork.top + (screenH - winH) / 2;

        SetWindowLongA(hWnd, GWL_STYLE, style);
        SetWindowPos(hWnd, HWND_NOTOPMOST, winX, winY, winW, winH, 
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        ClipCursor(NULL);

        /* Synchronize Capcom engine internal windowed flag */
        volatile WORD* pEngineMode = (volatile WORD*)0x00bf00e8;
        if (pEngineMode) *pEngineMode &= ~0x8000;

        g_bIsWindowed = 1;
        LogMsg("Switched to Windowed mode: %dx%d at (%d, %d)\n", winW, winH, winX, winY);
    } else {
        /* Switch back to Borderless Fullscreen */
        DWORD style = WS_POPUP | WS_VISIBLE;

        HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi;
        ZeroMemory(&mi, sizeof(mi));
        mi.cbSize = sizeof(mi);
        GetMonitorInfoA(hMon, &mi);

        int scrX = mi.rcMonitor.left;
        int scrY = mi.rcMonitor.top;
        int scrW = mi.rcMonitor.right - mi.rcMonitor.left;
        int scrH = mi.rcMonitor.bottom - mi.rcMonitor.top;

        SetWindowLongA(hWnd, GWL_STYLE, style);
        SetWindowPos(hWnd, HWND_TOP, scrX, scrY, scrW, scrH, 
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        /* Synchronize Capcom engine internal fullscreen flag */
        volatile WORD* pEngineMode = (volatile WORD*)0x00bf00e8;
        if (pEngineMode) *pEngineMode |= 0x8000;

        g_bIsWindowed = 0;
        LogMsg("Switched to Fullscreen mode: %dx%d at (%d, %d)\n", scrW, scrH, scrX, scrY);
    }

    SetWindowTextA(hWnd, "Street Fighter Alpha 2 PC");
}

static WNDPROC g_pfnOrigWndProc = NULL;

static LRESULT CALLBACK ModWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_cfgEnableAltEnter && (uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP) && wParam == VK_RETURN) {
        if (uMsg == WM_SYSKEYDOWN) {
            ToggleWindowedMode();
        }
        return 0; /* Consumed: avoids Windows default system beep */
    }
    if (g_pfnOrigWndProc) {
        return CallWindowProcA(g_pfnOrigWndProc, hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

/* DirectSound volume and buffer helpers */
static LONG PercentToDirectSoundVolume(int percent, LONG baseVolume) {
    if (percent <= 0) return -10000;
    if (percent >= 100) return baseVolume;
    double ratio = (double)percent / 100.0;
    double db = 2000.0 * log10(ratio);
    LONG vol = baseVolume + (LONG)db;
    if (vol < -10000) vol = -10000;
    if (vol > 0) vol = 0;
    return vol;
}

static void* GetBgmBuffer(void) {
    DWORD mode = *(volatile DWORD*)0x009cdc00;
    if (mode & 0x80) {
        void* pObj = *(void**)0x009cdbd0;
        if (!pObj || (DWORD)pObj < 0x10000) return NULL;
        return *(void**)((BYTE*)pObj + 0x10);
    } else {
        return *(void**)0x009cdbf0;
    }
}

typedef HRESULT (WINAPI *PFN_SetVolume)(void* pThis, LONG lVolume);

static void SetBgmVolume(LONG lVolume) {
    void* pDSB = GetBgmBuffer();
    if (!pDSB || (DWORD)pDSB < 0x10000) return;
    void** vtbl = *(void***)pDSB;
    if (!vtbl || (DWORD)vtbl < 0x10000) return;
    PFN_SetVolume pfnSetVolume = (PFN_SetVolume)vtbl[15];
    if (pfnSetVolume && (DWORD)pfnSetVolume >= 0x10000) {
        pfnSetVolume(pDSB, lVolume);
    }
}

/* Detect whether the round ended with a Super Combo Finish */
static int IsSuperComboFinish(void) {
    /* 1. Capcom global Super Finish explosion flag */
    volatile BYTE* pSuperFlag = (volatile BYTE*)0x00bdc5ae;
    if (pSuperFlag && *pSuperFlag == 0xFF) {
        return 1;
    }
    /* 2. Winner object finish code (0x255 == 6 is Super Combo Finish) */
    void** ppWinner = (void**)0x009d4c88;
    if (ppWinner && *ppWinner && (DWORD)*ppWinner >= 0x10000) {
        BYTE* pWinner = (BYTE*)*ppWinner;
        if (pWinner[0x255] == 6) {
            return 1;
        }
    }
    return 0;
}

/* Ducking State Machine */
enum DuckState {
    DUCK_STATE_IDLE = 0,
    DUCK_STATE_FADING_OUT,
    DUCK_STATE_HOLD,
    DUCK_STATE_FADING_IN
};

static int g_duckState = DUCK_STATE_IDLE;
static DWORD g_duckStartTime = 0;
static LONG g_duckStartVolume = 0;
static LONG g_duckTargetVolume = 0;
#define DUCK_FADE_IN_DURATION_MS  600

static void StartBgmDucking(void) {
    if (g_duckState != DUCK_STATE_IDLE) return;

    LONG baseVol = *(volatile LONG*)0x00c14584;
    if (baseVol < -10000 || baseVol > 0) baseVol = 0;

    g_duckStartVolume = baseVol;
    g_duckTargetVolume = PercentToDirectSoundVolume(g_cfgSuperKOBGMVolumePercent, baseVol);
    g_duckStartTime = timeGetTime();

    if (g_cfgSuperKOFadeDurationMS <= 0) {
        SetBgmVolume(g_duckTargetVolume);
        g_duckState = DUCK_STATE_HOLD;
    } else {
        g_duckState = DUCK_STATE_FADING_OUT;
    }

    LogMsg("Super Finish flash started! Ducking BGM from %ld mB to %ld mB (%d%%) over %d ms\n",
           g_duckStartVolume, g_duckTargetVolume, g_cfgSuperKOBGMVolumePercent, g_cfgSuperKOFadeDurationMS);
}

/*
 * Hooked Super Finish Spawner at 0x004e31d5
 * Called on the EXACT frame the yellow or blue screen flash / explosion begins.
 */
typedef void* (__cdecl *PFN_AllocObject)(int a1, int a2, int a3);
static PFN_AllocObject g_pfnRealAllocObject = (PFN_AllocObject)0x00549877;

static void* __cdecl Hooked_SpawnSuperFinishObject(int a1, int a2, int a3) {
    LogMsg("Super Finish explosion spawned at 0x004e31d5! Screen flash beginning.\n");
    if (g_cfgContinuousBGMOnKO && g_cfgDuckBGMOnSuperKO) {
        StartBgmDucking();
    }
    return g_pfnRealAllocObject(a1, a2, a3);
}

static void UpdateBgmDucking(void) {
    if (!g_cfgContinuousBGMOnKO || !g_cfgDuckBGMOnSuperKO) return;

    /* Check if Capcom's internal fade is active (e.g. match end fade to victory stats) */
    volatile BYTE* pCapcomFade = (volatile BYTE*)0x00cae530;
    if (pCapcomFade && *pCapcomFade != 0) {
        if (g_duckState != DUCK_STATE_IDLE) {
            g_duckState = DUCK_STATE_IDLE;
            volatile BYTE* pSuperFlag = (volatile BYTE*)0x00bdc5ae;
            if (pSuperFlag) *pSuperFlag = 0;
        }
        return;
    }

    /* Check if next round is starting (battle state resets to 0 in 0x00be05b4) */
    void* pCtx = *(void**)0x00be05b4;
    if (pCtx && (DWORD)pCtx >= 0x10000) {
        WORD battleState = *(volatile WORD*)((BYTE*)pCtx + 0x1c);
        if (battleState == 0 && (g_duckState == DUCK_STATE_HOLD || g_duckState == DUCK_STATE_FADING_OUT)) {
            LONG baseVol = *(volatile LONG*)0x00c14584;
            if (baseVol < -10000 || baseVol > 0) baseVol = 0;

            g_duckStartVolume = g_duckTargetVolume;
            g_duckTargetVolume = baseVol;
            g_duckStartTime = timeGetTime();
            g_duckState = DUCK_STATE_FADING_IN;

            /* Reset super finish flag for the upcoming round */
            volatile BYTE* pSuperFlag = (volatile BYTE*)0x00bdc5ae;
            if (pSuperFlag) *pSuperFlag = 0;
            LogMsg("New round starting! Fading BGM volume back in to %ld mB\n", g_duckTargetVolume);
        }
    }

    /* If idle, check if a Super Finish explosion has just begun (fallback to hook) */
    if (g_duckState == DUCK_STATE_IDLE) {
        if (IsSuperComboFinish()) {
            StartBgmDucking();
        } else {
            return;
        }
    }

    DWORD now = timeGetTime();
    DWORD elapsed = now - g_duckStartTime;

    if (g_duckState == DUCK_STATE_FADING_OUT) {
        DWORD duration = (DWORD)g_cfgSuperKOFadeDurationMS;
        if (duration == 0 || elapsed >= duration) {
            SetBgmVolume(g_duckTargetVolume);
            g_duckState = DUCK_STATE_HOLD;
        } else {
            float t = (float)elapsed / (float)duration;
            LONG curVol = g_duckStartVolume + (LONG)(t * (float)(g_duckTargetVolume - g_duckStartVolume));
            SetBgmVolume(curVol);
        }
    } else if (g_duckState == DUCK_STATE_HOLD) {
        SetBgmVolume(g_duckTargetVolume);
    } else if (g_duckState == DUCK_STATE_FADING_IN) {
        if (elapsed >= DUCK_FADE_IN_DURATION_MS) {
            SetBgmVolume(g_duckTargetVolume);
            g_duckState = DUCK_STATE_IDLE;
        } else {
            float t = (float)elapsed / (float)DUCK_FADE_IN_DURATION_MS;
            LONG curVol = g_duckStartVolume + (LONG)(t * (float)(g_duckTargetVolume - g_duckStartVolume));
            SetBgmVolume(curVol);
        }
    }
}

/* Hooked joyGetPosEx implementation */
MMRESULT WINAPI MyJoyGetPosEx(UINT uJoyID, LPJOYINFOEX pji) {
    if (!pji || pji->dwSize < sizeof(JOYINFOEX)) {
        return MMSYSERR_INVALPARAM;
    }

    /* Update selective Super KO BGM volume ducking */
    UpdateBgmDucking();

    /* Initialize to neutral state */
    pji->dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNBUTTONS;
    pji->dwXpos = 0x8000;
    pji->dwYpos = 0x8000;
    pji->dwZpos = 0x8000;
    pji->dwRpos = 0x8000;
    pji->dwUpos = 0x8000;
    pji->dwVpos = 0x8000;
    pji->dwButtons = 0;
    pji->dwButtonNumber = 0;
    pji->dwPOV = 0xFFFF;

    /* Enforce 60 FPS and prevent Capcom's engine from dynamically dropping frames */
    if (g_cfgForce60FPS) {
        volatile WORD* pFrameDelay   = (volatile WORD*)0x00bf5560;
        volatile WORD* pQualityLevel = (volatile WORD*)0x00bfe782;
        volatile WORD* pAutoSkipFlag = (volatile WORD*)0x00bebf76;
        *pFrameDelay   = 0x11;        /* 17ms frame delay = locked 60 FPS */
        *pQualityLevel = 0;           /* Full Quality 1/1 (no frame skip) */
        *pAutoSkipFlag &= ~0x4000;    /* Disable Auto Frame-Skip flag */
    }

    /* Initialize Window title, subclassing, and WASAPI Volume Mixer name once HWND is created */
    static int s_windowInitialized = 0;
    static int s_audioSessionAttempts = 0;
    HWND hWnd = *(HWND*)0x00bebf64;
    if (!hWnd || !IsWindow(hWnd)) {
        hWnd = FindWindowA("WinStzero2Class", NULL);
    }
    if (hWnd && IsWindow(hWnd)) {
        if (!s_windowInitialized) {
            s_windowInitialized = 1;
            SetWindowTextA(hWnd, "Street Fighter Alpha 2 PC");
            g_pfnOrigWndProc = (WNDPROC)SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)ModWndProc);
            LogMsg("Window initialized and subclassed (HWND: %p). Title set to 'Street Fighter Alpha 2 PC'\n", hWnd);
        }
        if (s_audioSessionAttempts < 10) {
            if (UpdateAudioSessionDisplayName(L"Street Fighter Alpha 2 PC")) {
                s_audioSessionAttempts = 999;
            } else {
                s_audioSessionAttempts++;
            }
        }
    }

    /* Hotkey polling fallback for Alt + Enter */
    if (g_cfgEnableAltEnter) {
        if ((GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState(VK_RETURN) & 0x8000)) {
            ToggleWindowedMode();
        }
    }

    /* If XInput mod is disabled, pass through directly */
    if (!g_cfgXInputEnabled || !g_pfnXInputGetState) {
        if (g_pfnRealJoyGetPosEx) return g_pfnRealJoyGetPosEx(uJoyID, pji);
        return JOYERR_UNPLUGGED;
    }

    /* Strict input separation: map uJoyID to configured XInput controller index */
    DWORD targetXInputIndex = 0xFFFFFFFF;
    if (uJoyID == 0) {
        targetXInputIndex = (DWORD)g_cfgPlayer1Index;
    } else if (uJoyID == 1) {
        targetXInputIndex = (DWORD)g_cfgPlayer2Index;
    } else {
        /* uJoyID >= 2 is not used for players */
        return JOYERR_UNPLUGGED;
    }

    XINPUT_STATE_DATA state;
    ZeroMemory(&state, sizeof(state));
    DWORD res = g_pfnXInputGetState(targetXInputIndex, &state);

    if (res != ERROR_SUCCESS) {
        /*
         * Controller for this player is NOT connected.
         * Crucial: DO NOT fall back to real_joyGetPosEx. This prevents
         * input bleeding from multi-endpoint USB devices into Player 2.
         */
        return JOYERR_UNPLUGGED;
    }

    /* Controller is connected: map inputs */
    WORD wBtns = state.Gamepad.wButtons;
    SHORT sLX  = state.Gamepad.sThumbLX;
    SHORT sLY  = state.Gamepad.sThumbLY;
    BYTE  bLT  = state.Gamepad.bLeftTrigger;
    BYTE  bRT  = state.Gamepad.bRightTrigger;

    /* Directions: Left / Right (thresholds in SFA2 are < 0x3000 and > 0xc000) */
    if ((wBtns & XINPUT_GAMEPAD_DPAD_LEFT) || (sLX < -g_cfgDeadzone)) {
        pji->dwXpos = 0x1000; /* LEFT */
    } else if ((wBtns & XINPUT_GAMEPAD_DPAD_RIGHT) || (sLX > g_cfgDeadzone)) {
        pji->dwXpos = 0xF000; /* RIGHT */
    }

    /* Directions: Up / Down (SFA2 expects Up < center, Down > center) */
    if ((wBtns & XINPUT_GAMEPAD_DPAD_UP) || (sLY > g_cfgDeadzone)) {
        pji->dwYpos = 0x1000; /* UP */
    } else if ((wBtns & XINPUT_GAMEPAD_DPAD_DOWN) || (sLY < -g_cfgDeadzone)) {
        pji->dwYpos = 0xF000; /* DOWN */
    }

    DWORD buttons = 0;

    /*
     * Top Row: Punches
     * X = Light Punch (LP)
     * Y = Medium Punch (MP)
     */
    if (wBtns & XINPUT_GAMEPAD_X) buttons |= SFA2_BTN_LP;
    if (wBtns & XINPUT_GAMEPAD_Y) buttons |= SFA2_BTN_MP;

    /*
     * Bottom Row: Kicks
     * A = Light Kick (LK)
     * B = Medium Kick (MK)
     */
    if (wBtns & XINPUT_GAMEPAD_A) buttons |= SFA2_BTN_LK;
    if (wBtns & XINPUT_GAMEPAD_B) buttons |= SFA2_BTN_MK;

    /*
     * Heavy Attacks: Bumpers & Triggers
     */
    int hp_active = 0;
    int hk_active = 0;

    if (!g_cfgSwapTriggersBumpers) {
        /* Default layout: RB = Heavy Punch, RT = Heavy Kick */
        if (wBtns & XINPUT_GAMEPAD_RIGHT_SHOULDER) hp_active = 1;
        if (bRT > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) hk_active = 1;
    } else {
        /* Swapped: RT = Heavy Punch, RB = Heavy Kick */
        if (bRT > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) hp_active = 1;
        if (wBtns & XINPUT_GAMEPAD_RIGHT_SHOULDER) hk_active = 1;
    }

    if (hp_active) buttons |= SFA2_BTN_HP;
    if (hk_active) buttons |= SFA2_BTN_HK;

    /*
     * Macros
     */
    if (g_cfgEnableMacros) {
        /* LB = Triple Punch (LP + MP + HP) */
        if (wBtns & XINPUT_GAMEPAD_LEFT_SHOULDER) {
            buttons |= (SFA2_BTN_LP | SFA2_BTN_MP | SFA2_BTN_HP);
        }
        /* LT = Triple Kick (LK + MK + HK) */
        if (bLT > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
            buttons |= (SFA2_BTN_LK | SFA2_BTN_MK | SFA2_BTN_HK);
        }
    }

    /*
     * System buttons:
     * START button -> SFA2 START (Button 8 / 0x800)
     * BACK / VIEW  -> SFA2 COIN / SELECT (Button 9 / 0x100)
     */
    if (wBtns & XINPUT_GAMEPAD_START) buttons |= SFA2_BTN_START;
    if (wBtns & XINPUT_GAMEPAD_BACK)  buttons |= SFA2_BTN_COIN;

    pji->dwButtons = buttons;
    return JOYERR_NOERROR;
}

/* Install IAT Hook on main module */
static void InstallIATHook(void) {
    HMODULE hMain = GetModuleHandleA(NULL);
    if (!hMain) return;

    /* Get real joyGetPosEx from system winmm.dll */
    HMODULE hWinMM = GetModuleHandleA("winmm.dll");
    if (!hWinMM) hWinMM = LoadLibraryA("winmm.dll");
    if (hWinMM) {
        g_pfnRealJoyGetPosEx = (PFN_joyGetPosEx)GetProcAddress(hWinMM, "joyGetPosEx");
    }

    /* Primary: direct pointer at 0x00cb0684 in ALPHA2.EXE */
    void** pKnownIatEntry = (void**)0x00cb0684;
    DWORD oldProtect;
    if (VirtualProtect(pKnownIatEntry, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        if (!g_pfnRealJoyGetPosEx && *pKnownIatEntry) {
            g_pfnRealJoyGetPosEx = (PFN_joyGetPosEx)*pKnownIatEntry;
        }
        *pKnownIatEntry = (void*)MyJoyGetPosEx;
        VirtualProtect(pKnownIatEntry, sizeof(void*), oldProtect, &oldProtect);
        LogMsg("Hooked joyGetPosEx directly at 0x00cb0684\n");
    }

    /* Secondary: dynamic PE IAT scan across all descriptors */
    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)hMain;
    if (pDos->e_magic != IMAGE_DOS_SIGNATURE) return;

    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)((BYTE*)hMain + pDos->e_lfanew);
    if (pNt->Signature != IMAGE_NT_SIGNATURE) return;

    DWORD importRva = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRva) return;

    PIMAGE_IMPORT_DESCRIPTOR pImport = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hMain + importRva);
    while (pImport->Name) {
        char* dllName = (char*)((BYTE*)hMain + pImport->Name);
        if (_stricmp(dllName, "winmm.dll") == 0) {
            PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((BYTE*)hMain + pImport->FirstThunk);
            PIMAGE_THUNK_DATA pOrigThunk = (PIMAGE_THUNK_DATA)((BYTE*)hMain + 
                (pImport->OriginalFirstThunk ? pImport->OriginalFirstThunk : pImport->FirstThunk));

            while (pOrigThunk->u1.AddressOfData) {
                if (!(pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                    PIMAGE_IMPORT_BY_NAME pImportByName = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hMain + pOrigThunk->u1.AddressOfData);
                    if (strcmp((char*)pImportByName->Name, "joyGetPosEx") == 0) {
                        if (VirtualProtect(&pThunk->u1.Function, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                            if (!g_pfnRealJoyGetPosEx) {
                                g_pfnRealJoyGetPosEx = (PFN_joyGetPosEx)pThunk->u1.Function;
                            }
                            pThunk->u1.Function = (DWORD_PTR)MyJoyGetPosEx;
                            VirtualProtect(&pThunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
                            LogMsg("Hooked joyGetPosEx via dynamic PE IAT traversal at %p\n", &pThunk->u1.Function);
                        }
                        break;
                    }
                }
                pThunk++;
                pOrigThunk++;
            }
        }
        pImport++;
    }
}

/* Load the real system dinput.dll */
static void LoadRealDInput(void) {
    if (g_hRealDInput) return;
    char sysDir[MAX_PATH];
    GetSystemDirectoryA(sysDir, MAX_PATH);
    strcat(sysDir, "\\dinput.dll");
    g_hRealDInput = LoadLibraryA(sysDir);
    if (g_hRealDInput) {
        g_pfnDirectInputCreateA = (PFN_DirectInputCreateA)GetProcAddress(g_hRealDInput, "DirectInputCreateA");
        g_pfnDirectInputCreateW = (PFN_DirectInputCreateW)GetProcAddress(g_hRealDInput, "DirectInputCreateW");
        g_pfnDirectInputCreateEx = (PFN_DirectInputCreateEx)GetProcAddress(g_hRealDInput, "DirectInputCreateEx");
        LogMsg("Loaded system dinput.dll from %s\n", sysDir);
    } else {
        LogMsg("Error: could not load system dinput.dll\n");
    }
}

/* Exported DirectInput wrappers */
HRESULT WINAPI DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPVOID *ppDI, void *punkOuter) {
    LoadRealDInput();
    if (g_pfnDirectInputCreateA) {
        return g_pfnDirectInputCreateA(hinst, dwVersion, ppDI, punkOuter);
    }
    return E_FAIL;
}

HRESULT WINAPI DirectInputCreateW(HINSTANCE hinst, DWORD dwVersion, LPVOID *ppDI, void *punkOuter) {
    LoadRealDInput();
    if (g_pfnDirectInputCreateW) {
        return g_pfnDirectInputCreateW(hinst, dwVersion, ppDI, punkOuter);
    }
    return E_FAIL;
}

HRESULT WINAPI DirectInputCreateEx(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, void *punkOuter) {
    LoadRealDInput();
    if (g_pfnDirectInputCreateEx) {
        return g_pfnDirectInputCreateEx(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    }
    return E_FAIL;
}

/*
 * Apply Audio Patches for Continuous BGM on K.O. (Arcade CPS2 behavior)
 *
 * Technical details:
 * 1. At 0x00430ee9, Capcom's battle engine calls BGM_Dispatch(6, 2) (Fade Out over 2 seconds)
 *    when health drops to 0 on K.O. We patch the function entry with 'RET' (0xC3), completely
 *    preventing the music from ducking/muting during K.O.
 * 2. At 0x00430494, during the round winner's victory pose (State 5), the engine executes
 *    BGM_Dispatch(5, 1) (Fade In over 1 second). We NOP out this 12-byte call so volume stays
 *    steadily at 100% without any volume ducking or oscillation.
 * 3. Match end (final round) still cleanly fades out via 0x0042f6c2 / 0x0042f406 when transitioning
 *    to the victory quote / stats screen.
 */
static void ApplyAudioPatches(void) {
    if (!g_cfgContinuousBGMOnKO) return;

    DWORD oldProtect;

    /* 1. Hook Super Finish Spawner at 0x004e31d5 to trigger volume ducking on the screen flash */
    BYTE* pSpawnCall = (BYTE*)0x004e31d5;
    if (VirtualProtect(pSpawnCall, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        pSpawnCall[0] = 0xE8; /* CALL rel32 */
        *(DWORD*)(pSpawnCall + 1) = (DWORD)Hooked_SpawnSuperFinishObject - ((DWORD)0x004e31d5 + 5);
        VirtualProtect(pSpawnCall, 5, oldProtect, &oldProtect);
        LogMsg("Applied audio patch: hooked Super Finish spawner at 0x004e31d5 -> %p\n", Hooked_SpawnSuperFinishObject);
    } else {
        LogMsg("Failed to hook Super Finish spawner at 0x004e31d5\n");
    }

    /* 2. Neutralize original K.O. Fade Out function at 0x00430ee9 with RET */
    BYTE* pKOFadeFunc = (BYTE*)0x00430ee9;
    if (VirtualProtect(pKOFadeFunc, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        *pKOFadeFunc = 0xC3; /* ret */
        VirtualProtect(pKOFadeFunc, 1, oldProtect, &oldProtect);
        LogMsg("Applied audio patch: neutralized K.O. BGM Fade Out at 0x00430ee9 (RET)\n");
    } else {
        LogMsg("Failed to patch K.O. BGM Fade Out at 0x00430ee9\n");
    }

    /* 3. NOP out win pose Fade In call at 0x00430494 (12 bytes) */
    BYTE* pWinPoseFadeIn = (BYTE*)0x00430494;
    if (VirtualProtect(pWinPoseFadeIn, 12, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memset(pWinPoseFadeIn, 0x90, 12); /* 12 NOPs */
        VirtualProtect(pWinPoseFadeIn, 12, oldProtect, &oldProtect);
        LogMsg("Applied audio patch: NOP'd win pose BGM Fade In at 0x00430494 (12 NOPs)\n");
    } else {
        LogMsg("Failed to patch win pose BGM Fade In at 0x00430494\n");
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        LoadConfig();
        LogMsg("SFA2 XInput & Modernization Mod loaded.\n");
        ApplyTitlePatches();
        InitXInput();
        InstallIATHook();
        ApplyAudioPatches();
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        if (g_hRealDInput) {
            FreeLibrary(g_hRealDInput);
            g_hRealDInput = NULL;
        }
        if (g_hXInput) {
            FreeLibrary(g_hXInput);
            g_hXInput = NULL;
        }
    }
    return TRUE;
}

