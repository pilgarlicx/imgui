// ImGui Platform Binding for: Windows (standard windows API for 32 and 64 bits applications)
// This needs to be used along with a Renderer (e.g. DirectX11, OpenGL3, Vulkan..)

#include "imgui.h"
#include "imgui_impl_win32.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

// CHANGELOG
//  2018-XX-XX: Platform: Added support for multiple windows via the ImGuiPlatformIO interface.
//  2018-03-20: Misc: Setup io.BackendFlags ImGuiBackendFlags_HasMouseCursors and ImGuiBackendFlags_HasSetMousePos flags + honor ImGuiConfigFlags_NoSetMouseCursor flag.
//  2018-02-20: Inputs: Added support for mouse cursors (ImGui::GetMouseCursor() value and WM_SETCURSOR message handling).
//  2018-02-06: Inputs: Added mapping for ImGuiKey_Space.
//  2018-02-06: Inputs: Honoring the io.WantSetMousePos by repositioning the mouse (when using navigation and ImGuiConfigFlags_NavMoveMouse is set).
//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.
//  2018-01-20: Inputs: Added Horizontal Mouse Wheel support.
//  2018-01-08: Inputs: Added mapping for ImGuiKey_Insert.
//  2018-01-05: Inputs: Added WM_LBUTTONDBLCLK double-click handlers for window classes with the CS_DBLCLKS flag.
//  2017-10-23: Inputs: Added WM_SYSKEYDOWN / WM_SYSKEYUP handlers so e.g. the VK_MENU key can be read.
//  2017-10-23: Inputs: Using Win32 ::SetCapture/::GetCapture() to retrieve mouse positions outside the client area when dragging. 
//  2016-11-12: Inputs: Only call Win32 ::SetCursor(NULL) when io.MouseDrawCursor is set.

// Win32 Data
static HWND                 g_hWnd = 0;
static INT64                g_Time = 0;
static INT64                g_TicksPerSecond = 0;
static ImGuiMouseCursor     g_LastMouseCursor = ImGuiMouseCursor_Count_;

// Forward Declarations
static void ImGui_ImplWin32_InitPlatformInterface();
static void ImGui_ImplWin32_ShutdownPlatformInterface();

// Functions
bool    ImGui_ImplWin32_Init(void* hwnd)
{
    if (!::QueryPerformanceFrequency((LARGE_INTEGER *)&g_TicksPerSecond))
        return false;
    if (!::QueryPerformanceCounter((LARGE_INTEGER *)&g_Time))
        return false;

    // Setup back-end capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
    io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;    // We can create multi-viewports on the Platform side (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // We can set io.MouseHoveredViewport correctly (optional, not easy)

    // Our mouse update function expect PlatformHandle to be filled for the main viewport
    g_hWnd = (HWND)hwnd;
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = (void*)g_hWnd;
    if (io.ConfigFlags & ImGuiConfigFlags_EnableViewports)
        ImGui_ImplWin32_InitPlatformInterface();

    // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array that we will update during the application lifetime.
    io.KeyMap[ImGuiKey_Tab] = VK_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
    io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
    io.KeyMap[ImGuiKey_Home] = VK_HOME;
    io.KeyMap[ImGuiKey_End] = VK_END;
    io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
    io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    io.KeyMap[ImGuiKey_Space] = VK_SPACE;
    io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
    io.KeyMap[ImGuiKey_A] = 'A';
    io.KeyMap[ImGuiKey_C] = 'C';
    io.KeyMap[ImGuiKey_V] = 'V';
    io.KeyMap[ImGuiKey_X] = 'X';
    io.KeyMap[ImGuiKey_Y] = 'Y';
    io.KeyMap[ImGuiKey_Z] = 'Z';

    io.ImeWindowHandle = g_hWnd;    

    return true;
}

void    ImGui_ImplWin32_Shutdown()
{
    ImGui_ImplWin32_ShutdownPlatformInterface();
    g_hWnd = (HWND)0;
}

static bool ImGui_ImplWin32_UpdateMouseCursor()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoSetMouseCursor)
        return false;

    ImGuiMouseCursor imgui_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None)
    {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        ::SetCursor(NULL);
    }
    else
    {
        // Hardware cursor type
        LPTSTR win32_cursor = IDC_ARROW;
        switch (imgui_cursor)
        {
        case ImGuiMouseCursor_Arrow:        win32_cursor = IDC_ARROW; break;
        case ImGuiMouseCursor_TextInput:    win32_cursor = IDC_IBEAM; break;
        case ImGuiMouseCursor_ResizeAll:    win32_cursor = IDC_SIZEALL; break;
        case ImGuiMouseCursor_ResizeEW:     win32_cursor = IDC_SIZEWE; break;
        case ImGuiMouseCursor_ResizeNS:     win32_cursor = IDC_SIZENS; break;
        case ImGuiMouseCursor_ResizeNESW:   win32_cursor = IDC_SIZENESW; break;
        case ImGuiMouseCursor_ResizeNWSE:   win32_cursor = IDC_SIZENWSE; break;
        }
        ::SetCursor(::LoadCursor(NULL, win32_cursor));
    }
    return true;
}

// This code supports multiple OS Windows mapped into different ImGui viewports, 
// So it is a little more complicated than your typical binding code (which only needs to set io.MousePos in your WM_MOUSEMOVE handler)
// This is what imgui needs from the back-end to support multiple windows:
// - io.MousePos               = mouse position (e.g. io.MousePos == viewport->Pos when we are on the upper-left of our viewport)
// - io.MousePosViewport       = viewport which mouse position is based from (generally the focused/active/capturing viewport)
// - io.MouseHoveredWindow     = viewport which mouse is hovering, **regardless of it being the active/focused window**, **regardless of another window holding mouse captured**. [Optional]
// This function overwrite the value of io.MousePos normally updated by the WM_MOUSEMOVE handler. 
// We keep the WM_MOUSEMOVE handling code so that WndProc function can be copied as-in in applications which do not need multiple OS windows support.
static void ImGui_ImplWin32_UpdateMousePos()
{
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
    io.MousePosViewport = 0;
    io.MouseHoveredViewport = 0;

    POINT pos;
    if (!::GetCursorPos(&pos))
        return;

    // Our back-end can tell which window is under the mouse cursor (not every back-end can), so pass that info to imgui
    HWND hovered_hwnd = ::WindowFromPoint(pos);
    if (hovered_hwnd)
        if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle((void*)hovered_hwnd))
            io.MouseHoveredViewport = viewport->ID;

    // Convert mouse from screen position to window client position
    HWND focused_hwnd = ::GetActiveWindow();
    if (focused_hwnd != 0 && ::ScreenToClient(focused_hwnd, &pos))
        if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle((void*)focused_hwnd))
        {
            io.MousePos = ImVec2(viewport->Pos.x + (float)pos.x, viewport->Pos.y + (float)pos.y);
            io.MousePosViewport = viewport->ID;
        }
}

void    ImGui_ImplWin32_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    RECT rect;
    ::GetClientRect(g_hWnd, &rect);
    io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

    // Setup time step
    INT64 current_time;
    ::QueryPerformanceCounter((LARGE_INTEGER *)&current_time);
    io.DeltaTime = (float)(current_time - g_Time) / g_TicksPerSecond;
    g_Time = current_time;

    // Read keyboard modifiers inputs
    io.KeyCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    io.KeyShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
    io.KeyAlt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
    io.KeySuper = false;
    // io.KeysDown : filled by WM_KEYDOWN/WM_KEYUP events
    // io.MousePos : filled by WM_MOUSEMOVE events
    // io.MouseDown : filled by WM_*BUTTON* events
    // io.MouseWheel : filled by WM_MOUSEWHEEL events

    // Set OS mouse position if requested (only used when ImGuiConfigFlags_NavEnableSetMousePos is enabled by user)
    if (io.WantSetMousePos)
    {
        POINT pos = { (int)io.MousePos.x, (int)io.MousePos.y };
        ::ClientToScreen(g_hWnd, &pos);
        ::SetCursorPos(pos.x, pos.y);
    }

    // Update OS mouse cursor with the cursor requested by imgui
    ImGuiMouseCursor mouse_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    if (g_LastMouseCursor != mouse_cursor)
    {
        g_LastMouseCursor = mouse_cursor;
        ImGui_ImplWin32_UpdateMouseCursor();
    }

    ImGui_ImplWin32_UpdateMousePos();

    // Start the frame. This call will update the io.WantCaptureMouse, io.WantCaptureKeyboard flag that you can use to dispatch inputs (or not) to your application.
    ImGui::NewFrame();
}

// Process Win32 mouse/keyboard inputs. 
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
// PS: In this Win32 handler, we use the capture API (GetCapture/SetCapture/ReleaseCapture) to be able to read mouse coordinations when dragging mouse outside of our window bounds.
// PS: We treat DBLCLK messages as regular mouse down messages, so this code will work on windows classes that have the CS_DBLCLKS flag set. Our own example app code doesn't set this flag.
IMGUI_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui::GetCurrentContext() == NULL)
        return 0;

    ImGuiIO& io = ImGui::GetIO();
    switch (msg)
    {
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
    {
        int button = 0;
        if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) button = 0;
        if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) button = 1;
        if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) button = 2;
        if (!ImGui::IsAnyMouseDown() && ::GetCapture() == NULL)
            ::SetCapture(hwnd);
        io.MouseDown[button] = true;
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        int button = 0;
        if (msg == WM_LBUTTONUP) button = 0;
        if (msg == WM_RBUTTONUP) button = 1;
        if (msg == WM_MBUTTONUP) button = 2;
        io.MouseDown[button] = false;
        if (!ImGui::IsAnyMouseDown() && ::GetCapture() == hwnd)
            ::ReleaseCapture();
        return 0;
    }
    case WM_MOUSEWHEEL:
        io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
        return 0;
    case WM_MOUSEHWHEEL:
        io.MouseWheelH += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
        return 0;
    case WM_MOUSEMOVE:
        io.MousePos.x = (signed short)(lParam);
        io.MousePos.y = (signed short)(lParam >> 16);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wParam < 256)
            io.KeysDown[wParam] = 1;
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wParam < 256)
            io.KeysDown[wParam] = 0;
        return 0;
    case WM_CHAR:
        // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
        if (wParam > 0 && wParam < 0x10000)
            io.AddInputCharacter((unsigned short)wParam);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && ImGui_ImplWin32_UpdateMouseCursor())
            return 1;
        return 0;
    }
    return 0;
}

// --------------------------------------------------------------------------------------------------------
// DPI handling
// Those in theory should be simple calls but Windows has multiple ways to handle DPI, and most of them
// require recent Windows versions at runtime or recent Windows SDK at compile-time. Neither we want to depend on.
// So we dynamically select and load those functions to avoid dependencies. This is the scheme successfully 
// used by GLFW (from which we borrowed some of the code here) and other applications aiming to be portable.
//---------------------------------------------------------------------------------------------------------
// FIXME-DPI: For now we just call SetProcessDpiAwareness(PROCESS_PER_MONITOR_AWARE) without requiring SDK 8.1 or 10.
// We may allow/aim calling the most-recent-available version, e.g. Windows 10 Creators Update has SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
// At this point ImGui_ImplWin32_EnableDpiAwareness() is just a helper called by main.cpp, we don't call it ourselves.
//---------------------------------------------------------------------------------------------------------

static BOOL IsWindowsVersionOrGreater(WORD major, WORD minor, WORD sp)
{
    OSVERSIONINFOEXW osvi = { sizeof(osvi), major, minor, 0, 0,{ 0 }, sp };
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR;
    ULONGLONG cond = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_MINORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    return VerifyVersionInfoW(&osvi, mask, cond);
}
#define IsWindows8Point1OrGreater()  IsWindowsVersionOrGreater(HIBYTE(0x0602), LOBYTE(0x0602), 0) // _WIN32_WINNT_WINBLUE
#define IsWindows10OrGreater()       IsWindowsVersionOrGreater(HIBYTE(0x0A00), LOBYTE(0x0A00), 0) // _WIN32_WINNT_WIN10

#ifndef DPI_ENUMS_DECLARED
typedef enum { PROCESS_DPI_UNAWARE = 0, PROCESS_SYSTEM_DPI_AWARE = 1, PROCESS_PER_MONITOR_DPI_AWARE = 2 } PROCESS_DPI_AWARENESS;
typedef enum { MDT_EFFECTIVE_DPI = 0, MDT_ANGULAR_DPI = 1, MDT_RAW_DPI = 2, MDT_DEFAULT = MDT_EFFECTIVE_DPI } MONITOR_DPI_TYPE;
#endif
typedef HRESULT(WINAPI * PFN_SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS);              // Shcore.lib+dll, Windows 8.1
typedef HRESULT(WINAPI * PFN_GetDpiForMonitor)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*); // Shcore.lib+dll, Windows 8.1

void ImGui_ImplWin32_EnableDpiAwareness()
{
    if (IsWindows8Point1OrGreater())
    {
        static HINSTANCE shcore_dll = ::LoadLibraryA("shcore.dll"); // Reference counted per-process
        if (PFN_SetProcessDpiAwareness SetProcessDpiAwarenessFn = (PFN_SetProcessDpiAwareness)::GetProcAddress(shcore_dll, "SetProcessDpiAwareness"))
            SetProcessDpiAwarenessFn(PROCESS_PER_MONITOR_DPI_AWARE);
    }
    else
    {
        SetProcessDPIAware();
    }
}

float ImGui_ImplWin32_GetDpiScaleForMonitor(void* monitor)
{
    UINT xdpi = 96, ydpi = 96;
    if (IsWindows8Point1OrGreater())
    {
        static HINSTANCE shcore_dll = ::LoadLibraryA("shcore.dll"); // Reference counted per-process
        if (PFN_GetDpiForMonitor GetDpiForMonitorFn = (PFN_GetDpiForMonitor)::GetProcAddress(shcore_dll, "GetDpiForMonitor"))
            GetDpiForMonitorFn((HMONITOR)monitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
    }
    else
    {
        const HDC dc = ::GetDC(NULL);
        xdpi = ::GetDeviceCaps(dc, LOGPIXELSX);
        ydpi = ::GetDeviceCaps(dc, LOGPIXELSY);
        ::ReleaseDC(NULL, dc);
    }
    IM_ASSERT(xdpi == ydpi); // Please contact me if you hit this assert!
    return xdpi / 96.0f;
}

float ImGui_ImplWin32_GetDpiScaleForHwnd(void* hwnd)
{
    HMONITOR monitor = ::MonitorFromWindow((HWND)hwnd, MONITOR_DEFAULTTONEAREST);
    return ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
}

float ImGui_ImplWin32_GetDpiScaleForRect(int x1, int y1, int x2, int y2)
{
    RECT viewport_rect = { (LONG)x1, (LONG)y1, (LONG)x2, (LONG)y2 };
    HMONITOR monitor = ::MonitorFromRect(&viewport_rect, MONITOR_DEFAULTTONEAREST);
    return ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
}

// --------------------------------------------------------------------------------------------------------
// Platform Windows
// --------------------------------------------------------------------------------------------------------

struct ImGuiViewportDataWin32
{
    HWND    Hwnd;
    DWORD   DwStyle;
    DWORD   DwExStyle;

    ImGuiViewportDataWin32() { Hwnd = NULL; DwStyle = DwExStyle = 0; }
    ~ImGuiViewportDataWin32() { IM_ASSERT(Hwnd == NULL); }
};

static void ImGui_ImplWin32_CreateWindow(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = IM_NEW(ImGuiViewportDataWin32)();
    viewport->PlatformUserData = data;

    ImGuiIO& io = ImGui::GetIO();
    bool no_decoration = (viewport->Flags & ImGuiViewportFlags_NoDecoration) != 0;
    bool no_task_bar = (io.ConfigFlags & ImGuiConfigFlags_NoTaskBarForViewports) != 0;
    if (no_decoration)
    {
        data->DwStyle = WS_POPUP;
        data->DwExStyle = no_task_bar ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;
    }
    else
    {
        data->DwStyle = WS_OVERLAPPEDWINDOW;
        data->DwExStyle = no_task_bar ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;
    }

    // Create window
    RECT rect = { (LONG)viewport->PlatformPos.x, (LONG)viewport->PlatformPos.y, (LONG)(viewport->PlatformPos.x + viewport->Size.x), (LONG)(viewport->PlatformPos.y + viewport->Size.y) };
    ::AdjustWindowRectEx(&rect, data->DwStyle, FALSE, data->DwExStyle);
    data->Hwnd = ::CreateWindowExA(
        data->DwExStyle, "ImGui Platform", "No Title Yet", data->DwStyle,       // Style, class name, window name
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,    // Window area
        g_hWnd, NULL, ::GetModuleHandle(NULL), NULL);                           // Parent window, Menu, Instance, Param
    viewport->PlatformRequestResize = false;
    viewport->PlatformHandle = data->Hwnd;
}

static void ImGui_ImplWin32_DestroyWindow(ImGuiViewport* viewport)
{
    if (ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData)
    {
        if (::GetCapture() == data->Hwnd)
        {
            // Transfer capture so if we started dragging from a window that later disappears, we'll still release the MOUSEUP event.
            ::ReleaseCapture();
            ::SetCapture(g_hWnd);
        }
        if (data->Hwnd)
            ::DestroyWindow(data->Hwnd);
        data->Hwnd = NULL;
        IM_DELETE(data);
    }
    viewport->PlatformUserData = viewport->PlatformHandle = NULL;
}

static void ImGui_ImplWin32_ShowWindow(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    IM_ASSERT(data->Hwnd != 0);
    if (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing)
        ::ShowWindow(data->Hwnd, SW_SHOWNA);
    else
        ::ShowWindow(data->Hwnd, SW_SHOW);
}

static ImVec2 ImGui_ImplWin32_GetWindowPos(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    IM_ASSERT(data->Hwnd != 0);
    POINT pos = { 0, 0 };
    ::ClientToScreen(data->Hwnd, &pos);
    return ImVec2((float)pos.x, (float)pos.y);
}

static void ImGui_ImplWin32_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    IM_ASSERT(data->Hwnd != 0);
    RECT rect = { (LONG)pos.x, (LONG)pos.y, (LONG)pos.x, (LONG)pos.y };
    ::AdjustWindowRectEx(&rect, data->DwStyle, FALSE, data->DwExStyle);
    ::SetWindowPos(data->Hwnd, NULL, rect.left, rect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

static ImVec2 ImGui_ImplWin32_GetWindowSize(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    IM_ASSERT(data->Hwnd != 0);
    RECT rect;
    ::GetClientRect(data->Hwnd, &rect);
    return ImVec2(float(rect.right - rect.left), float(rect.bottom - rect.top));
}

static void ImGui_ImplWin32_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    IM_ASSERT(data->Hwnd != 0);
    RECT rect = { 0, 0, (LONG)size.x, (LONG)size.y };
    ::AdjustWindowRectEx(&rect, data->DwStyle, FALSE, data->DwExStyle); // Client to Screen
    ::SetWindowPos(data->Hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

static void ImGui_ImplWin32_SetWindowTitle(ImGuiViewport* viewport, const char* title)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    IM_ASSERT(data->Hwnd != 0);
    ::SetWindowTextA(data->Hwnd, title);
}

static float ImGui_ImplWin32_GetWindowDpiScale(ImGuiViewport* viewport)
{
    ImGuiViewportDataWin32* data = (ImGuiViewportDataWin32*)viewport->PlatformUserData;
    if (data && data->Hwnd)
        return ImGui_ImplWin32_GetDpiScaleForHwnd(data->Hwnd);

    // The first frame a viewport is created we don't have a window yet
    return ImGui_ImplWin32_GetDpiScaleForRect(
        (int)(viewport->PlatformPos.x), (int)(viewport->PlatformPos.y),
        (int)(viewport->PlatformPos.x + viewport->Size.x), (int)(viewport->PlatformPos.y + viewport->Size.y));
}

static LRESULT CALLBACK ImGui_ImplWin32_WndProcHandler_PlatformWindow(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle((void*)hWnd))
    {
        switch (msg)
        {
        case WM_CLOSE:
            viewport->PlatformRequestClose = true;
            return 0;
        case WM_MOVE:
            viewport->PlatformRequestMove = true;
            break;
        case WM_SIZE:
            viewport->PlatformRequestResize = true;
            break;
        case WM_NCHITTEST:
            // Let mouse pass-through the window, this is used while e.g. dragging a window, we creates a temporary overlay but want the cursor to aim behind our overlay.
            if (viewport->Flags & ImGuiViewportFlags_NoInputs)
                return HTTRANSPARENT;
            break;
        }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void ImGui_ImplWin32_InitPlatformInterface()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = ImGui_ImplWin32_WndProcHandler_PlatformWindow;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = ::GetModuleHandle(NULL);
    wcex.hIcon = NULL;
    wcex.hCursor = NULL;
    wcex.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = _T("ImGui Platform");
    wcex.hIconSm = NULL;
    ::RegisterClassEx(&wcex);

    // Register platform interface (will be coupled with a renderer interface)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateWindow = ImGui_ImplWin32_CreateWindow;
    platform_io.Platform_DestroyWindow = ImGui_ImplWin32_DestroyWindow;
    platform_io.Platform_ShowWindow = ImGui_ImplWin32_ShowWindow;
    platform_io.Platform_SetWindowPos = ImGui_ImplWin32_SetWindowPos;
    platform_io.Platform_GetWindowPos = ImGui_ImplWin32_GetWindowPos;
    platform_io.Platform_SetWindowSize = ImGui_ImplWin32_SetWindowSize;
    platform_io.Platform_GetWindowSize = ImGui_ImplWin32_GetWindowSize;
    platform_io.Platform_SetWindowTitle = ImGui_ImplWin32_SetWindowTitle;
    platform_io.Platform_GetWindowDpiScale = ImGui_ImplWin32_GetWindowDpiScale;

    // Register main window handle
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGuiViewportDataWin32* data = IM_NEW(ImGuiViewportDataWin32)();
    data->Hwnd = g_hWnd;
    main_viewport->PlatformUserData = data;
    main_viewport->PlatformHandle = (void*)data->Hwnd;
}

static void ImGui_ImplWin32_ShutdownPlatformInterface()
{
    ::UnregisterClass(_T("ImGui Platform"), ::GetModuleHandle(NULL));
}
