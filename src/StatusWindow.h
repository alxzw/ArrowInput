#pragma once

#include "ArrowInput.h"

namespace arrowinput {

class StatusWindow {
public:
    StatusWindow();
    ~StatusWindow();

    void Show(const std::wstring& text, const POINT& anchor);
    void Hide();

private:
    static constexpr wchar_t kWindowClassName[] = L"ArrowInputStatusWindow";
    static constexpr UINT_PTR kHideTimerId = 1;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    bool EnsureWindow();
    void RegisterWindowClass();
    void Paint(HWND hwnd);

    HWND hwnd_;
    std::wstring text_;
};

}  // namespace arrowinput
