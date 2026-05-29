#pragma once

#include "ArrowInput.h"
#include "CandidateWindow.h"
#include "Config.h"
#include "Engine.h"
#include "StatusWindow.h"

namespace arrowinput {

class TextService final
    : public ITfTextInputProcessorEx,
      public ITfKeyEventSink,
      public ITfThreadFocusSink,
      public ITfDisplayAttributeProvider {
public:
    TextService();
    ~TextService();

    IFACEMETHODIMP QueryInterface(REFIID iid, void** result) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Activate(ITfThreadMgr* thread_mgr, TfClientId client_id) override;
    IFACEMETHODIMP Deactivate() override;
    IFACEMETHODIMP ActivateEx(ITfThreadMgr* thread_mgr, TfClientId client_id, DWORD flags) override;

    IFACEMETHODIMP OnSetFocus(BOOL foreground) override;
    IFACEMETHODIMP OnTestKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
    IFACEMETHODIMP OnKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
    IFACEMETHODIMP OnTestKeyUp(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
    IFACEMETHODIMP OnKeyUp(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
    IFACEMETHODIMP OnSetThreadFocus() override;
    IFACEMETHODIMP OnKillThreadFocus() override;
    IFACEMETHODIMP OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) override;
    IFACEMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** enum_info) override;
    IFACEMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** info) override;

private:
    HRESULT AdviseKeyEventSink();
    void UnadviseKeyEventSink();
    HRESULT AdviseThreadFocusSink();
    void UnadviseThreadFocusSink();
    HRESULT PreserveFullShapeKey(ITfKeystrokeMgr* keystroke_mgr);
    void UnpreserveFullShapeKey(ITfKeystrokeMgr* keystroke_mgr);
    void ClearCompositionRange();
    void RequestFocusCleanupEditSession(ITfContext* context, ITfRange* range);
    void ClearTransientFocusState(const wchar_t* log_message);
    void CommitCandidateFromWindow(size_t index);
    void PageCandidateFromWindow(bool next_page);
    void HighlightCandidateFromWindow(size_t index);
    void CancelComposition(ITfContext* context, const wchar_t* success_log, const wchar_t* failure_log);
    void CancelCompositionForShortcut(ITfContext* context);
    bool ExitToSystemInputMethod();
    void ReloadConfigIfChanged();
    void LogDictionaryStats(const wchar_t* prefix);
    bool ReadConfigWriteTime(FILETIME* write_time) const;
    void ShowStatus(ITfContext* context);
    POINT GetStatusAnchor(ITfContext* context);
    static void CandidateClickThunk(void* context, size_t index);
    static void CandidatePageThunk(void* context, bool next_page);
    static void CandidateHoverThunk(void* context, size_t index);
    bool ShouldEatKey(WPARAM key) const;
    bool ControlOrAltDown() const;
    bool ShiftKey(WPARAM key) const;
    bool ToggleInputModeKey(WPARAM key) const;
    bool ExitInputMethodKey(WPARAM key) const;
    bool ToggleFullShapeKey(WPARAM key) const;
    std::wstring ChinesePunctuationForKey(WPARAM key) const;
    void AdvanceChinesePunctuationState(WPARAM key);
    std::wstring FullShapeTextForKey(WPARAM key) const;
    static bool IsHandledVirtualKey(WPARAM key);

    std::atomic<ULONG> ref_count_;
    ITfThreadMgr* thread_mgr_;
    TfClientId client_id_;
    bool key_event_sink_advised_;
    DWORD thread_focus_sink_cookie_;
    bool full_shape_key_preserved_;
    bool chinese_mode_;
    bool full_shape_;
    bool shift_pending_toggle_;
    bool shift_combined_key_;
    bool suppress_space_keydown_;
    bool shift_down_;
    bool space_down_;
    bool full_shape_hotkey_used_;
    bool single_quote_open_;
    bool double_quote_open_;
    bool config_reload_pending_;
    FILETIME config_write_time_;
    ITfRange* composition_range_;
    ITfContext* active_context_;
    CandidateWindow candidate_window_;
    StatusWindow status_window_;
    Config config_;
    Engine engine_;
};

}  // namespace arrowinput
