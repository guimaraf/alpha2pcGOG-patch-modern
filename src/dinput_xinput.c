#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static int g_cfgContinuousBGMOnKO = 1; /* Keep BGM playing continuously on K.O. (Arcade CPS2 behavior) */
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

    g_cfgContinuousBGMOnKO = GetPrivateProfileIntA("Audio", "ContinuousBGMOnKO", 1, iniPath);

    g_cfgLogEnabled = GetPrivateProfileIntA("Debug", "EnableLog", 0, iniPath);

    LogMsg("Config loaded: XInput=%d, Deadzone=%d, Macros=%d, SwapTB=%d, P1Index=%d, P2Index=%d, Force60FPS=%d, ContinuousBGM=%d\n",
           g_cfgXInputEnabled, g_cfgDeadzone, g_cfgEnableMacros, g_cfgSwapTriggersBumpers,
           g_cfgPlayer1Index, g_cfgPlayer2Index, g_cfgForce60FPS, g_cfgContinuousBGMOnKO);
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

/* Hooked joyGetPosEx implementation */
MMRESULT WINAPI MyJoyGetPosEx(UINT uJoyID, LPJOYINFOEX pji) {
    if (!pji || pji->dwSize < sizeof(JOYINFOEX)) {
        return MMSYSERR_INVALPARAM;
    }

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

    /* 1. Neutralize K.O. Fade Out function at 0x00430ee9 */
    BYTE* pKOFadeFunc = (BYTE*)0x00430ee9;
    if (VirtualProtect(pKOFadeFunc, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        *pKOFadeFunc = 0xC3; /* ret */
        VirtualProtect(pKOFadeFunc, 1, oldProtect, &oldProtect);
        LogMsg("Applied audio patch: neutralized K.O. BGM Fade Out at 0x00430ee9 (RET)\n");
    } else {
        LogMsg("Failed to patch K.O. BGM Fade Out at 0x00430ee9\n");
    }

    /* 2. NOP out win pose Fade In call at 0x00430494 (12 bytes) */
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

