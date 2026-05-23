#pragma once

#include "ArrowInput.h"
#include "Candidate.h"

namespace arrowinput {

class CandidateWindow {
public:
    using ClickCallback = void (*)(void* context, size_t index);
    using PageCallback = void (*)(void* context, bool next_page);
    using HoverCallback = void (*)(void* context, size_t index);

    CandidateWindow();
    ~CandidateWindow();

    void SetClickCallback(ClickCallback callback, void* context);
    void SetPageCallback(PageCallback callback, void* context);
    void SetHoverCallback(HoverCallback callback, void* context);
    void Show(
        const std::vector<Candidate>& candidates,
        size_t selected_index,
        size_t page_start,
        size_t page_size,
        size_t total_candidates,
        const POINT& anchor);
    void Hide();

private:
    static constexpr wchar_t kWindowClassName[] = L"ArrowInputCandidateWindow";

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    bool EnsureWindow();
    bool EnsureFont();
    void RegisterWindowClass();
    void Paint(HWND hwnd);
    void HandleClick(int x, int y);
    void HandleMouseMove(int x, int y);
    void HandleMouseWheel(short delta);
    void ResetHover();
    int HitTest(int x, int y) const;
    std::wstring CandidateLabel(size_t index) const;

    HWND hwnd_;
    HFONT font_;
    bool owns_font_;
    std::vector<Candidate> candidates_;
    std::vector<RECT> hit_rects_;
    size_t selected_index_;
    size_t page_start_;
    size_t page_size_;
    size_t total_candidates_;
    int hover_index_;
    ClickCallback click_callback_;
    PageCallback page_callback_;
    HoverCallback hover_callback_;
    void* click_context_;
    void* page_context_;
    void* hover_context_;
};

}  // namespace arrowinput
