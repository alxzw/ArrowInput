#include "StatusWindow.h"

namespace arrowinput {

StatusWindow::StatusWindow()
    : hwnd_(nullptr)
{
}

StatusWindow::~StatusWindow()
{
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void StatusWindow::RegisterWindowClass()
{
    static bool registered = false;
    if (registered) {
        return;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = StatusWindow::WindowProc;
    wc.hInstance = g_module;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClassName;
    RegisterClassExW(&wc);
    registered = true;
}

bool StatusWindow::EnsureWindow()
{
    if (hwnd_) {
        return true;
    }

    RegisterWindowClass();
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kWindowClassName,
        L"",
        WS_POPUP,
        0,
        0,
        96,
        30,
        nullptr,
        nullptr,
        g_module,
        this);

    return hwnd_ != nullptr;
}

void StatusWindow::Show(const std::wstring& text, const POINT& anchor)
{
    if (text.empty()) {
        Hide();
        return;
    }
    if (!EnsureWindow()) {
        return;
    }

    text_ = text;

    HDC dc = GetDC(hwnd_);
    SIZE text_size = {};
    GetTextExtentPoint32W(dc, text_.c_str(), static_cast<int>(text_.size()), &text_size);
    ReleaseDC(hwnd_, dc);

    const int width = max(76, text_size.cx + 24);
    const int height = max(30, text_size.cy + 14);
    int x = anchor.x + 8;
    int y = anchor.y + 6;

    HMONITOR monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor && GetMonitorInfoW(monitor, &monitor_info)) {
        const RECT& work = monitor_info.rcWork;
        if (x + width > work.right) {
            x = work.right - width;
        }
        if (y + height > work.bottom) {
            y = anchor.y - height - 6;
        }
        if (x < work.left) {
            x = work.left;
        }
        if (y < work.top) {
            y = work.top;
        }
    }

    SetTimer(hwnd_, kHideTimerId, 900, nullptr);
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void StatusWindow::Hide()
{
    text_.clear();
    if (hwnd_) {
        KillTimer(hwnd_, kHideTimerId);
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void StatusWindow::Paint(HWND hwnd)
{
    PAINTSTRUCT ps = {};
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc = {};
    GetClientRect(hwnd, &rc);

    HBRUSH background = CreateSolidBrush(RGB(32, 38, 48));
    FillRect(dc, &rc, background);
    DeleteObject(background);

    HPEN border = CreatePen(PS_SOLID, 1, RGB(110, 140, 190));
    HGDIOBJ old_pen = SelectObject(dc, border);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(border);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    RECT text_rect = rc;
    text_rect.left += 10;
    text_rect.right -= 10;
    DrawTextW(dc, text_.c_str(), static_cast<int>(text_.size()), &text_rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK StatusWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    StatusWindow* window = reinterpret_cast<StatusWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = reinterpret_cast<StatusWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }

    switch (message) {
    case WM_PAINT:
        if (window) {
            window->Paint(hwnd);
            return 0;
        }
        break;
    case WM_TIMER:
        if (window && wparam == kHideTimerId) {
            window->Hide();
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace arrowinput
