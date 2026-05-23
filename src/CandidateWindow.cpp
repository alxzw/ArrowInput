#include "CandidateWindow.h"

#include <windowsx.h>

namespace arrowinput {

CandidateWindow::CandidateWindow()
    : hwnd_(nullptr),
      font_(nullptr),
      owns_font_(false),
      selected_index_(0),
      page_start_(0),
      page_size_(5),
      total_candidates_(0),
      hover_index_(-1),
      click_callback_(nullptr),
      page_callback_(nullptr),
      hover_callback_(nullptr),
      click_context_(nullptr),
      page_context_(nullptr),
      hover_context_(nullptr)
{
}

CandidateWindow::~CandidateWindow()
{
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (font_ && owns_font_) {
        DeleteObject(font_);
    }
    font_ = nullptr;
    owns_font_ = false;
}

void CandidateWindow::SetClickCallback(ClickCallback callback, void* context)
{
    click_callback_ = callback;
    click_context_ = context;
}

void CandidateWindow::SetPageCallback(PageCallback callback, void* context)
{
    page_callback_ = callback;
    page_context_ = context;
}

void CandidateWindow::SetHoverCallback(HoverCallback callback, void* context)
{
    hover_callback_ = callback;
    hover_context_ = context;
}

void CandidateWindow::RegisterWindowClass()
{
    static bool registered = false;
    if (registered) {
        return;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = CandidateWindow::WindowProc;
    wc.hInstance = g_module;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClassName;
    RegisterClassExW(&wc);
    registered = true;
}

bool CandidateWindow::EnsureWindow()
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
        120,
        32,
        nullptr,
        nullptr,
        g_module,
        this);

    return hwnd_ != nullptr;
}

bool CandidateWindow::EnsureFont()
{
    if (font_) {
        return true;
    }

    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0)) {
        font_ = CreateFontIndirectW(&metrics.lfMessageFont);
        owns_font_ = font_ != nullptr;
    }
    if (!font_) {
        font_ = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        owns_font_ = false;
    }
    return font_ != nullptr;
}

void CandidateWindow::Show(
    const std::vector<Candidate>& candidates,
    size_t selected_index,
    size_t page_start,
    size_t page_size,
    size_t total_candidates,
    const POINT& anchor)
{
    if (candidates.empty()) {
        Hide();
        return;
    }
    if (!EnsureWindow()) {
        return;
    }
    EnsureFont();

    candidates_ = candidates;
    selected_index_ = selected_index < candidates_.size() ? selected_index : 0;
    page_start_ = page_start;
    page_size_ = page_size > 0 ? page_size : 5;
    total_candidates_ = total_candidates;
    hover_index_ = -1;

    HDC dc = GetDC(hwnd_);
    HGDIOBJ old_font = font_ ? SelectObject(dc, font_) : nullptr;
    SIZE text_size = {};
    std::wstring text;
    for (size_t i = 0; i < candidates_.size(); ++i) {
        text += CandidateLabel(i);
        text += L"  ";
    }
    if (total_candidates_ > page_size_) {
        const size_t current_page = page_start_ / page_size_ + 1;
        const size_t total_pages = (total_candidates_ + page_size_ - 1) / page_size_;
        text += L" ";
        text += std::to_wstring(current_page);
        text += L"/";
        text += std::to_wstring(total_pages);
    }
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &text_size);
    if (old_font) {
        SelectObject(dc, old_font);
    }
    ReleaseDC(hwnd_, dc);

    int width = max(120, text_size.cx + 18);
    int height = max(30, text_size.cy + 12);
    int x = anchor.x;
    int y = anchor.y + 24;

    HMONITOR monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor && GetMonitorInfoW(monitor, &monitor_info)) {
        const RECT& work = monitor_info.rcWork;
        if (x + width > work.right) {
            x = work.right - width;
        }
        if (y + height > work.bottom) {
            y = anchor.y - height - 4;
        }
        if (x < work.left) {
            x = work.left;
        }
        if (y < work.top) {
            y = work.top;
        }
    }

    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void CandidateWindow::Hide()
{
    candidates_.clear();
    hit_rects_.clear();
    page_start_ = 0;
    page_size_ = 5;
    total_candidates_ = 0;
    hover_index_ = -1;
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void CandidateWindow::Paint(HWND hwnd)
{
    PAINTSTRUCT ps = {};
    HDC dc = BeginPaint(hwnd, &ps);
    EnsureFont();
    HGDIOBJ old_font = font_ ? SelectObject(dc, font_) : nullptr;
    RECT rc = {};
    GetClientRect(hwnd, &rc);

    HBRUSH background = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(dc, &rc, background);
    DeleteObject(background);

    HPEN border = CreatePen(PS_SOLID, 1, RGB(90, 120, 180));
    HGDIOBJ old_pen = SelectObject(dc, border);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(border);

    int x = 8;
    int y = 6;
    hit_rects_.clear();
    for (size_t i = 0; i < candidates_.size(); ++i) {
        std::wstring item = CandidateLabel(i);
        SIZE size = {};
        GetTextExtentPoint32W(dc, item.c_str(), static_cast<int>(item.size()), &size);
        RECT item_rect = {x - 3, y - 2, x + size.cx + 7, y + size.cy + 3};
        hit_rects_.push_back(item_rect);

        const bool highlighted = (static_cast<int>(i) == hover_index_) || (hover_index_ < 0 && i == selected_index_);
        if (highlighted) {
            HBRUSH highlight = CreateSolidBrush(RGB(35, 100, 210));
            FillRect(dc, &item_rect, highlight);
            DeleteObject(highlight);
            SetTextColor(dc, RGB(255, 255, 255));
        } else {
            SetTextColor(dc, RGB(20, 20, 20));
        }

        SetBkMode(dc, TRANSPARENT);
        if (candidates_[i].comment.empty()) {
            TextOutW(dc, x, y, item.c_str(), static_cast<int>(item.size()));
        } else {
            std::wstring main_text = std::to_wstring(i + 1) + L"." + candidates_[i].text;
            std::wstring comment_text = L" " + candidates_[i].comment;
            SIZE main_size = {};
            GetTextExtentPoint32W(dc, main_text.c_str(), static_cast<int>(main_text.size()), &main_size);
            TextOutW(dc, x, y, main_text.c_str(), static_cast<int>(main_text.size()));
            SetTextColor(dc, highlighted ? RGB(225, 235, 255) : RGB(110, 110, 110));
            TextOutW(dc, x + main_size.cx, y, comment_text.c_str(), static_cast<int>(comment_text.size()));
        }
        x += size.cx + 16;
    }

    if (total_candidates_ > page_size_) {
        const size_t current_page = page_start_ / page_size_ + 1;
        const size_t total_pages = (total_candidates_ + page_size_ - 1) / page_size_;
        std::wstring page_text = std::to_wstring(current_page) + L"/" + std::to_wstring(total_pages);
        SetTextColor(dc, RGB(100, 100, 100));
        SetBkMode(dc, TRANSPARENT);
        TextOutW(dc, x, y, page_text.c_str(), static_cast<int>(page_text.size()));
    }
    if (old_font) {
        SelectObject(dc, old_font);
    }
    EndPaint(hwnd, &ps);
}

void CandidateWindow::HandleClick(int x, int y)
{
    int index = HitTest(x, y);
    if (index >= 0 && click_callback_) {
        click_callback_(click_context_, static_cast<size_t>(index));
    }
}

void CandidateWindow::HandleMouseMove(int x, int y)
{
    int index = HitTest(x, y);
    if (index != hover_index_) {
        hover_index_ = index;
        if (index >= 0 && static_cast<size_t>(index) != selected_index_ && hover_callback_) {
            hover_callback_(hover_context_, static_cast<size_t>(index));
        }
        InvalidateRect(hwnd_, nullptr, TRUE);
    }

    TRACKMOUSEEVENT track = {};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = hwnd_;
    TrackMouseEvent(&track);
}

void CandidateWindow::HandleMouseWheel(short delta)
{
    if (!page_callback_ || total_candidates_ <= page_size_) {
        return;
    }
    page_callback_(page_context_, delta < 0);
}

void CandidateWindow::ResetHover()
{
    if (hover_index_ != -1) {
        hover_index_ = -1;
        if (hwnd_) {
            InvalidateRect(hwnd_, nullptr, TRUE);
        }
    }
}

int CandidateWindow::HitTest(int x, int y) const
{
    POINT point = {x, y};
    for (size_t i = 0; i < hit_rects_.size(); ++i) {
        if (PtInRect(&hit_rects_[i], point)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::wstring CandidateWindow::CandidateLabel(size_t index) const
{
    if (index >= candidates_.size()) {
        return L"";
    }

    std::wstring label = std::to_wstring(index + 1) + L"." + candidates_[index].text;
    if (!candidates_[index].comment.empty()) {
        label += L" ";
        label += candidates_[index].comment;
    }
    return label;
}

LRESULT CALLBACK CandidateWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    CandidateWindow* window = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = reinterpret_cast<CandidateWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }

    switch (message) {
    case WM_PAINT:
        if (window) {
            window->Paint(hwnd);
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        if (window) {
            window->HandleClick(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (window) {
            window->HandleMouseMove(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
        }
        break;
    case WM_MOUSEWHEEL:
        if (window) {
            window->HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(wparam));
            return 0;
        }
        break;
    case WM_MOUSELEAVE:
        if (window) {
            window->ResetHover();
            return 0;
        }
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace arrowinput
