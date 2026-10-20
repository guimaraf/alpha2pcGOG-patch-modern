# Street Fighter Alpha 2 (PC / GOG, 1998) - Modernization & XInput Patch

[![Latest Release](https://img.shields.io/github/v/release/guimaraf/alpha2pcGOG-patch-modern?color=brightgreen&label=Latest%20Release)](https://github.com/guimaraf/alpha2pcGOG-patch-modern/releases/latest)
[![Download Patch](https://img.shields.io/badge/Download-alpha2gogModernPatch.zip-blue?logo=github)](https://github.com/guimaraf/alpha2pcGOG-patch-modern/releases/latest)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-blue.svg)](https://www.microsoft.com/windows)
[![Compatibility](https://img.shields.io/badge/Release-GOG%20Classic-green.svg)](https://www.gog.com/)
[![Graphics](https://img.shields.io/badge/Renderer-Direct3D%2011-orange.svg)](#graphics-and-display)
[![Input](https://img.shields.io/badge/Input-Native%20XInput-purple.svg)](#controller-and-input-mapping)

A complete, zero-configuration modernization patch for the **1998 PC release of Street Fighter Alpha 2** (GOG version). 

This patch enables smooth 60 FPS gameplay on Windows 10 and Windows 11, translates legacy DirectDraw 3 graphics to Direct3D 11 with pixel-perfect 4:3 scaling, adds native XInput gamepad support with authentic Capcom 6-button arcade layouts, and bypasses the engine's legacy auto-frame-skipping logic.

---

## Features

* **Direct3D 11 Graphics Pipeline (DDrawCompat v0.7.2)**:
  * Eliminates the severe 1–15 FPS software emulation lag caused by modern WDDM drivers on Windows 10 and 11.
  * Borderless fullscreen at your native desktop resolution without altering desktop settings.
  * Preserves the original **4:3 aspect ratio** with clean pixel-art point scaling and pillarboxing.
  * Flawless **Alt-Tab** task switching without crashing or screen distortion.
* **Windowed & Fullscreen Toggle (`Alt + Enter`)**:
  * Toggle smoothly between Borderless Fullscreen and Windowed mode at any time using the universal **`Alt + Enter`** shortcut.
  * Windowed mode renders with an authentic **4:3 aspect ratio** (defaults to **960x720**, customizable in `sfa2_patch.ini`), featuring standard Windows caption bar and minimize controls.
* **Modernized Title Across Windows (`Street Fighter Alpha 2 PC`)**:
  * Completely eliminates Capcom's legacy 1998 placeholder `"Win SF Alpha2 Application"`:
    * **Window Title Bar**: Displays `"Street Fighter Alpha 2 PC"`.
    * **Windows Taskbar & Start Bar**: Properly displays `"Street Fighter Alpha 2 PC"`.
    * **Windows Volume Mixer & Sound Settings**: Integrates with WASAPI (`IAudioSessionControl`) so the application volume slider is labeled `"Street Fighter Alpha 2 PC"`.
* **Native XInput Controller Support (`dinput.dll` proxy)**:
  * Intercepts `joyGetPosEx` via memory IAT hooking with near-zero latency.
  * Full **D-Pad** support (unifies the digital pad with the left analog stick).
  * Independent analog triggers (fixes the legacy Z-axis trigger-canceling bug).
  * Out-of-the-box support for Xbox, 8BitDo (M30, Pro 2, Ultimate), DualShock/DualSense (via DS4Windows/Steam), and generic XInput devices.
  * **Strict 2-Player Controller Isolation**: Controller Index 0 is exclusively assigned to Player 1, and Index 1 to Player 2. When only one controller is plugged in, Player 2 is kept completely disconnected—preventing phantom inputs or accidental 2P challenger starts.
* **Arcade-Accurate Continuous BGM & Selective Super Combo Finish Ducking**:
  * In the 1998 PC release, Capcom implemented an intentional volume-ducking routine that faded out the stage BGM over 2 seconds whenever health reached 0, muting the music while the "K.O.!" voice played, before fading back in during the win pose.
  * **Normal & Special Move Finishes**: When the round ends with normal attacks or regular special moves (Shoryuken, Hadouken, Tatsumaki, throws, etc.), the music plays continuously and seamlessly at **100% volume** across the K.O. call, announcer voice, win pose, and straight into Round 2—matching original CPS2 arcade boards.
  * **Super Combo Finishes**: When a round ends with a Super Combo (Super Finish), the patch detects the lethal move type in real-time and hooks the Super Finish spawner (`0x004e31d5`), fading the BGM volume down to **30%** at the **exact moment the screen flashes with the yellow or blue explosion**. By the time the slow-motion tumble animation finishes and the announcer calls "K.O.!", the music is already at 30% volume. It smoothly restores back to 100% volume when the next round begins.
  * When the match is completely over (final round), the BGM cleanly fades out into the victory quotes / character stats screen.
* **Locked 60 FPS & Frame-Skip Bypass**:
  * Overrides Capcom's legacy Pentium II "Auto Frame-Skip" mechanism in memory, preventing the engine from dynamically dropping down to 30, 20, or 15 FPS.
  * Synchronizes presentation with the desktop window manager (DWM) for stutter-free frame pacing.
* **Non-Destructive Drop-in Installation**:
  * Modifies **zero bytes** of the original `ALPHA2.EXE` executable. Fully reversible at any time.

---

## Download & Installation (GOG Version)

1. Download **[`alpha2gogModernPatch.zip`](https://github.com/guimaraf/alpha2pcGOG-patch-modern/releases/latest)** from the **[Releases](https://github.com/guimaraf/alpha2pcGOG-patch-modern/releases/latest)** section.
2. Extract all contents directly into your **Street Fighter Alpha 2** game installation directory (where `ALPHA2.EXE` is located):
   * `ddraw.dll`
   * `DDrawCompat.ini`
   * `dinput.dll`
   * `sfa2_patch.ini`
   * `LEIAME.txt`
3. Launch the game normally via the GOG shortcut or by executing `ALPHA2.EXE`.

To uninstall, simply delete those files from the game folder.

---

## Controller & Input Mapping

The input wrapper maps modern gamepads to Capcom's CPS2 arcade layout:

| Controller Button (Xbox / 8BitDo M30) | Street Fighter Action | In-Engine Bitmask |
| :--- | :--- | :--- |
| **D-Pad / Left Stick** | Movement (Left, Right, Up, Down) | Directions |
| **X Button** | **Light Punch (LP)** | `Button 4` |
| **Y Button** | **Medium Punch (MP)** | `Button 1` |
| **RB (or Z button on M30)** | **Heavy Punch (HP)** | `Button 2` |
| **A Button** | **Light Kick (LK)** | `Button 0` |
| **B Button** | **Medium Kick (MK)** | `Button 3` |
| **RT (or C button on M30)** | **Heavy Kick (HK)** | `Button 5` |
| **LB (or L button on M30)** | **Triple Punch Macro** (LP + MP + HP) | `1 << 4 \| 1 << 1 \| 1 << 2` |
| **LT** | **Triple Kick Macro** (LK + MK + HK) | `1 << 0 \| 1 << 3 \| 1 << 5` |
| **START / Menu** | **Start** | `Button 8 (0x800)` |
| **BACK / View / Minus** | **Coin / Select** | `Button 9 (0x100)` |

*Note on 6-Button Pads (e.g., 8BitDo M30 2.4G / Bluetooth)*: When set to XInput mode, the top row (`X`, `Y`, `Z`) immediately acts as LP, MP, HP, and the bottom row (`A`, `B`, `C`) acts as LK, MK, HK.

---

## Frame Rate Monitoring & RivaTuner (RTSS)

You can monitor in-game performance using either the built-in OSD or RivaTuner Statistics Server:

### Option A: Built-in DDrawCompat Overlay (No External Tools)
* Press **`Shift + F12`** at any time during gameplay to toggle the native FPS, frametime, and rendering queue overlay.
* Press **`Shift + F11`** to open the real-time graphic settings overlay.

### Option B: RivaTuner Statistics Server (RTSS / MSI Afterburner)
Because `ALPHA2.EXE` is a legacy 1998 binary and Direct3D 11 is loaded dynamically via the wrapper, RTSS with default settings may not automatically inject.
1. Open **RivaTuner Statistics Server (RTSS)**.
2. Click the green **`+` (Add)** button in the lower-left corner.
3. Select `ALPHA2.EXE` from your game folder.
4. In the profile settings for `ALPHA2.EXE`:
   * Set **Application detection level** to **High**.
   * Ensure **Show On-Screen Display** is set to **ON**.
5. Launch the game; the RTSS overlay will appear with your configured framerate and hardware metrics.

---

## Configuration (`sfa2_patch.ini`)

Edit `sfa2_patch.ini` with any text editor to customize your experience:

```ini
[Graphics]
; Locks the engine to 60 FPS (17ms ticks) and disables Capcom's internal auto-frame-skip
Force60FPS = 1

; Enable Alt+Enter shortcut to toggle between Fullscreen and Windowed mode
EnableAltEnter = 1

; Windowed mode dimensions (standard 4:3 arcade aspect ratio)
WindowedWidth = 960
WindowedHeight = 720

[Controllers]
; Windows XInput controller index for Player 1 (default: 0)
Player1_Index = 0

; Windows XInput controller index for Player 2 (default: 1)
; Kept disconnected when only 1 controller is connected
Player2_Index = 1

[Audio]
; Keep stage BGM playing continuously during K.O. and round transitions (arcade behavior)
; 1 = Enabled (Recommended), 0 = Disabled (PC port default ducking behavior)
ContinuousBGMOnKO = 1

; Selective BGM volume ducking on Super Combo finish (1 = Enabled, 0 = Disabled)
; Triggered exactly when the screen flashes with the yellow or blue explosion.
; Normal & Special finishes (Shoryuken, Hadouken) stay at 100% volume.
DuckBGMOnSuperKO = 1

; Target BGM volume percent during Super Combo finish (default: 30)
SuperKOBGMVolumePercent = 30

; Duration of the fade down in milliseconds right as the screen flashes (default: 250)
SuperKOFadeDurationMS = 250

[XInput]
; Enable native XInput hook (1 = Enabled, 0 = Disabled)
Enabled = 1

; Analog stick deadzone (range: 0 to 32767, default: 7849)
Deadzone = 7849

; Enable Triple Punch (LB) and Triple Kick (LT) macros (1 = Yes, 0 = No)
EnableMacros = 1

; Swap heavy attack buttons between bumpers and triggers:
; 0 = Default (RB = Heavy Punch, RT = Heavy Kick)
; 1 = Swapped (RT = Heavy Punch, RB = Heavy Kick)
SwapTriggersBumpers = 0

[Debug]
; Write diagnostic info to sfa2_patch.log (1 = Enabled, 0 = Disabled)
EnableLog = 0
```

---

## Building from Source

The source code for the input wrapper is located in the [`src/`](src/) directory.

### Requirements:
* Clang or GCC targeting 32-bit Windows (`i686-w64-windows-gnu`).
* Windows SDK headers (`windows.h`, `mmsystem.h`, `objbase.h`, `math.h`).

### Build Command:
Run [`src/build.bat`](src/build.bat) or compile manually:
```bash
clang -target i686-w64-windows-gnu -shared -O2 -s src/dinput_xinput.c src/dinput.def -o dinput.dll -lwinmm -lm -lole32
```

---

## Technical Architecture & Reversing Notes

* **DirectSound Architecture & Independent Channels**: `ALPHA2.EXE` utilizes separate DirectSound secondary buffers (`IDirectSoundBuffer`) for Sound Effects (`[SoundManager + se_id*4 + 8]`) and Background Music (`[SoundManager + 0x4030]`). Sound effects and BGM do not collide or share physical channels.
* **Super Combo Screen Flash Detection & Instant Ducking**:
  * In the original PC port, when health hits 0, `0x00430a9b` called `0x00430ee9` (`BGM_Dispatch(6, 2)`), fading out music on *every* K.O. after the fall animation.
  * Our mod hooks the Super Finish Effect Spawner at `0x004e31d5` (the exact routine that initializes the yellow or blue palette-cycling explosion object 6) as well as inspecting `*(BYTE*)0x00bdc5ae == 0xFF` and `*(BYTE*)(winner_obj + 0x255) == 6`.
  * **On Super Combo lethal hit**: As soon as the screen flashes with the Super Finish explosion, our hook triggers a fast 250ms fade down to **30% volume** ($-1046\text{ mB}$ via DirectSound `SetVolume`). By the time the slow-motion tumble animation finishes and the announcer calls "K.O.!", the music is already at 30% volume.
  * **On Normal / Special move lethal hit**: Routine `0x004e31b0` is never called, `0x00bdc5ae` remains 0, and `0x00430ee9` is neutralized with `RET` (`0xC3`), keeping the stage music playing at **100% volume** uninterrupted.
  * When the next round begins (`battleState == 0` in `*(WORD*)(0x00be05b4 + 0x1c)`), the volume smoothly fades back up to 100% and resets the Super flag.
  * In the final match-deciding round, Capcom's natural match-end fade (`0x0042f406` / `0x00cae530 != 0`) is respected, cleanly transitioning to the victory screen.
* **WinMM Hooking**: `ALPHA2.EXE` imports `joyGetPosEx` from `WINMM.dll` with an IAT entry at `0x00cb0684`. The game never queries `JOY_RETURNPOV` or `JOY_RETURNZ`, which caused the lack of D-Pad and trigger support on modern gamepads. Our wrapper intercepts `joyGetPosEx` calls, queries `XInputGetState`, and feeds translated coordinate and button masks directly to the game.
* **Capcom In-Game Polling (`0x0054dac7`)**: Unlike the prototype routine at `0x0043d460`, the active gameplay routine maps `Button 4` to LP, `Button 1` to MP, `Button 2` to HP, `Button 0` to LK, `Button 3` to MK, `Button 5` to HK, `Button 8` to Start (`0x800`), and `Button 9` to Coin (`0x100`).
* **Frame-Skip Override**: Located at `0x0055ff18`, the engine dynamically altered `0x00bf5560` (frame delay) and `0x00bfe782` (skip factor) whenever timing exceeded 17ms. The wrapper continuously enforces full-rate rendering and clears the auto-skip flag (`*(WORD*)0x00bebf76 &= ~0x4000`).
* **Fullscreen & Windowed Architecture (`Alt + Enter`)**:
  * `ALPHA2.EXE` initializes DirectDraw in exclusive fullscreen cooperation (`DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN`), with rendering mode tracked at `*(WORD*)0x00bf00e8` (Bit `0x8000`: 1 = Fullscreen, 0 = Windowed).
  * Simply changing Win32 styles (`SetWindowLongA`) fails because DDrawCompat's D3D11 presentation pipeline enforces a borderless desktop overlay while DirectDraw remains in exclusive mode.
  * Our hook implements a clean two-way transition:
    * **Fullscreen to Windowed**: Disengages exclusive mode via `IDirectDraw::RestoreDisplayMode` and `IDirectDraw::SetCooperativeLevel(hWnd, DDSCL_NORMAL)`, clears Bit `0x8000` at `0x00bf00e8`, resizes the window with 4:3 frame metrics (`AdjustWindowRect`), creates/attaches an `IDirectDrawClipper` to the Primary Surface (`*(void**)0x00bf0040`), updates client screen bounds at `0x00bebdb0`, and invokes Capcom's internal clipper synchronizer (`0x0043ec60`).
    * **Windowed to Fullscreen**: Restores borderless style (`WS_POPUP`), re-engages `DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT`, sets Bit `0x8000` at `0x00bf00e8`, restores surfaces (`IDirectDrawSurface::Restore`), and re-synchronizes coordinates.

---

## Credits & License

* **DDrawCompat**: Developed by [narzoul](https://github.com/narzoul/DDrawCompat) (BSD License).
* **XInput Wrapper & Modernization**: Developed for the Street Fighter community.
* **Street Fighter Alpha 2**: © Capcom Co., Ltd. This patch contains no copyrighted game assets or proprietary binaries from Capcom.
