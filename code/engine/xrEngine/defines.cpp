#include "stdafx.h"

#ifdef DEBUG
ECORE_API BOOL bDebug = FALSE;

#endif

// Video
//. u32			psCurrentMode		= 1024;
u32 psCurrentVidMode[2] = { 1024, 768 };
u32 psCurrentBPP = 32;
// ODE collision spaces are not safe to mutate from the main/render thread while
// the physics worker is traversing them. Keep physics on the main frame sequence;
// sound and networking can still use the secondary thread.
Flags32 psDeviceFlags = { rsFullscreen | rsDetails | mtSound | mtNetwork |
                          rsDrawStatic | rsDrawDynamic | rsRefresh60hz };

u32 psScreenMode = smFullscreen;
u32 psFpsLimit = flRefresh;

bool ScreenModeBorderless() {
    return !psDeviceFlags.is(rsFullscreen) && psScreenMode == smBorderless;
}

bool FindVidMode(bool bLargest, u32& dwWidth, u32& dwHeight) {
    if (!vid_mode_token)
        return false;
    bool found = false;
    u64 best = 0;
    for (xr_token* tok = vid_mode_token; tok->name; tok++) {
        u32 w = 0, h = 0;
        if (2 != sscanf(tok->name, "%ux%u", &w, &h))
            continue;
        const u64 area = u64(w) * h;
        if (!found || (bLargest ? area > best : area < best)) {
            dwWidth = w;
            dwHeight = h;
            best = area;
            found = true;
        }
    }
    return found;
}

bool BorderlessWindowRect(HWND hWnd, RECT& rect) {
    if (!hWnd)
        return false;
    MONITORINFO info;
    info.cbSize = sizeof(info);
    HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
    if (!monitor || !GetMonitorInfo(monitor, &info))
        return false;
    rect = info.rcMonitor;
    return rect.right > rect.left && rect.bottom > rect.top;
}

bool BorderlessResolution(HWND hWnd, u32& dwWidth, u32& dwHeight) {
    RECT rect;
    if (BorderlessWindowRect(hWnd, rect)) {
        dwWidth = u32(rect.right - rect.left);
        dwHeight = u32(rect.bottom - rect.top);
        return true;
    }
    return FindVidMode(true, dwWidth, dwHeight);
}

static u32 s_display_refresh_rate = 60;

void UpdateDisplayRefreshRate(HWND hWnd) {
    u32 rate = 0;
    MONITORINFOEX info;
    info.cbSize = sizeof(info);
    HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
    DEVMODE mode;
    std::memset(&mode, 0, sizeof(mode));
    mode.dmSize = sizeof(mode);
    if (monitor && GetMonitorInfo(monitor, &info) &&
        EnumDisplaySettings(info.szDevice, ENUM_CURRENT_SETTINGS, &mode))
        rate = mode.dmDisplayFrequency;
    // Some drivers report 0 or 1 for "the hardware default"
    s_display_refresh_rate = rate > 1 ? rate : 60;
}

u32 DisplayRefreshRate() { return s_display_refresh_rate; }

u32 FpsLimitRate(u32 limit) {
    switch (limit) {
    case fl30:
        return 30;
    case fl60:
        return 60;
    case flHalfRefresh:
        return s_display_refresh_rate / 2;
    case flRefresh:
        return s_display_refresh_rate;
    case flDoubleRefresh:
        return s_display_refresh_rate * 2;
    default:
        return 0;
    }
}

// textures
int psTextureLOD = 1;
