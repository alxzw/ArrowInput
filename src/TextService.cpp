#include "TextService.h"
#include "DisplayAttribute.h"
#include "KeyEditSession.h"

namespace arrowinput {

namespace {

void DebugLogKeyEvent(const wchar_t* prefix, WPARAM key)
{
    wchar_t line[160] = {};
    StringCchPrintfW(
        line,
        ARRAYSIZE(line),
        L"%s vk=0x%02X shift=0x%04X space=0x%04X",
        prefix,
        static_cast<unsigned int>(key),
        static_cast<unsigned int>(GetKeyState(VK_SHIFT) & 0xFFFF),
        static_cast<unsigned int>(GetKeyState(VK_SPACE) & 0xFFFF));
    DebugLog(line);
}

bool IsFullShapePreservedKeyGuid(REFGUID guid)
{
    return IsEqualGUID(guid, kFullShapePreservedKeyGuid) ||
           IsEqualGUID(guid, kFullShapeLeftShiftPreservedKeyGuid) ||
           IsEqualGUID(guid, kFullShapeRightShiftPreservedKeyGuid);
}

class FocusCleanupEditSession final : public ITfEditSession {
public:
    FocusCleanupEditSession(ITfContext* context, ITfRange* range)
        : ref_count_(1),
          context_(context),
          range_(range)
    {
        if (context_) {
            context_->AddRef();
        }
        if (range_) {
            range_->AddRef();
        }
    }

    ~FocusCleanupEditSession()
    {
        if (range_) {
            range_->Release();
        }
        if (context_) {
            context_->Release();
        }
    }

    IFACEMETHODIMP QueryInterface(REFIID iid, void** result) override
    {
        if (!result) {
            return E_POINTER;
        }
        *result = nullptr;
        if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_ITfEditSession)) {
            *result = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return ++ref_count_;
    }

    IFACEMETHODIMP_(ULONG) Release() override
    {
        ULONG count = --ref_count_;
        if (count == 0) {
            delete this;
        }
        return count;
    }

    IFACEMETHODIMP DoEditSession(TfEditCookie edit_cookie) override
    {
        if (!range_) {
            return E_UNEXPECTED;
        }
        HRESULT hr = range_->SetText(edit_cookie, 0, L"", 0);
        DebugLog(SUCCEEDED(hr) ? L"focus cleanup edit session: preedit cleared" : L"focus cleanup edit session: clear failed");
        return hr;
    }

private:
    std::atomic<ULONG> ref_count_;
    ITfContext* context_;
    ITfRange* range_;
};

}  // namespace

TextService::TextService()
    : ref_count_(1),
      thread_mgr_(nullptr),
      client_id_(TF_CLIENTID_NULL),
      key_event_sink_advised_(false),
      thread_focus_sink_cookie_(TF_INVALID_COOKIE),
      full_shape_key_preserved_(false),
      chinese_mode_(true),
      full_shape_(false),
      shift_pending_toggle_(false),
      shift_combined_key_(false),
      suppress_space_keydown_(false),
      shift_down_(false),
      space_down_(false),
      full_shape_hotkey_used_(false),
      single_quote_open_(true),
      double_quote_open_(true),
      config_reload_pending_(false),
      config_write_time_({}),
      composition_range_(nullptr),
      active_context_(nullptr)
{
    AddRefServer();
    candidate_window_.SetClickCallback(&TextService::CandidateClickThunk, this);
    candidate_window_.SetPageCallback(&TextService::CandidatePageThunk, this);
    candidate_window_.SetHoverCallback(&TextService::CandidateHoverThunk, this);
}

TextService::~TextService()
{
    Deactivate();
    ReleaseServer();
}

HRESULT TextService::QueryInterface(REFIID iid, void** result)
{
    if (!result) {
        return E_POINTER;
    }
    *result = nullptr;

    if (IsEqualIID(iid, IID_IUnknown) ||
        IsEqualIID(iid, IID_ITfTextInputProcessor) ||
        IsEqualIID(iid, IID_ITfTextInputProcessorEx)) {
        *result = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (IsEqualIID(iid, IID_ITfKeyEventSink)) {
        *result = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(iid, IID_ITfThreadFocusSink)) {
        *result = static_cast<ITfThreadFocusSink*>(this);
    } else if (IsEqualIID(iid, IID_ITfDisplayAttributeProvider)) {
        *result = static_cast<ITfDisplayAttributeProvider*>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

ULONG TextService::AddRef()
{
    return ++ref_count_;
}

ULONG TextService::Release()
{
    ULONG count = --ref_count_;
    if (count == 0) {
        delete this;
    }
    return count;
}

HRESULT TextService::Activate(ITfThreadMgr* thread_mgr, TfClientId client_id)
{
    return ActivateEx(thread_mgr, client_id, 0);
}

HRESULT TextService::ActivateEx(ITfThreadMgr* thread_mgr, TfClientId client_id, DWORD)
{
    if (!thread_mgr) {
        return E_INVALIDARG;
    }

    thread_mgr_ = thread_mgr;
    thread_mgr_->AddRef();
    client_id_ = client_id;
    config_ = LoadConfig();
    engine_.Reset(config_);
    LogDictionaryStats(L"dictionary activated");
    ReadConfigWriteTime(&config_write_time_);
    config_reload_pending_ = false;
    chinese_mode_ = true;
    full_shape_ = config_.full_shape;
    shift_pending_toggle_ = false;
    shift_combined_key_ = false;
    suppress_space_keydown_ = false;
    shift_down_ = false;
    space_down_ = false;
    full_shape_hotkey_used_ = false;
    DebugLog(L"text service activated");

    HRESULT hr = AdviseKeyEventSink();
    if (FAILED(hr)) {
        return hr;
    }
    return AdviseThreadFocusSink();
}

HRESULT TextService::Deactivate()
{
    UnadviseThreadFocusSink();
    UnadviseKeyEventSink();
    candidate_window_.Hide();
    status_window_.Hide();
    ClearCompositionRange();
    if (active_context_) {
        active_context_->Release();
        active_context_ = nullptr;
    }

    if (thread_mgr_) {
        thread_mgr_->Release();
        thread_mgr_ = nullptr;
    }
    client_id_ = TF_CLIENTID_NULL;
    return S_OK;
}

HRESULT TextService::OnSetFocus(BOOL foreground)
{
    if (!foreground) {
        ClearTransientFocusState(L"context focus lost");
    }
    return S_OK;
}

HRESULT TextService::OnTestKeyDown(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten)
{
    if (!eaten) {
        return E_POINTER;
    }
    ReloadConfigIfChanged();
    *eaten = ShouldEatKey(wparam) ? TRUE : FALSE;
    if (wparam == VK_SPACE || ToggleInputModeKey(wparam)) {
        DebugLogKeyEvent(*eaten ? L"test key down eaten diagnostic" : L"test key down passed diagnostic", wparam);
    }
    if (*eaten) {
        DebugLog(L"test key down eaten");
    }
    return S_OK;
}

HRESULT TextService::OnKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten)
{
    if (!eaten) {
        return E_POINTER;
    }
    ReloadConfigIfChanged();

    if (ControlOrAltDown()) {
        if (engine_.HasComposition()) {
            CancelCompositionForShortcut(context);
        }
        shift_pending_toggle_ = false;
        shift_combined_key_ = false;
        suppress_space_keydown_ = false;
        full_shape_hotkey_used_ = false;
        *eaten = FALSE;
        return S_OK;
    }

    const bool shift_key = ToggleInputModeKey(wparam);
    const bool space_key = wparam == VK_SPACE;
    const bool was_down = (lparam & (1 << 30)) != 0;
    if (shift_key) {
        shift_down_ = true;
    }
    if (space_key) {
        space_down_ = true;
    }

    const bool full_shape_hotkey = !ControlOrAltDown() && !engine_.HasComposition() &&
        ((space_key && shift_down_) || (shift_key && space_down_));
    if (full_shape_hotkey) {
        *eaten = TRUE;
        shift_pending_toggle_ = false;
        shift_combined_key_ = true;
        suppress_space_keydown_ = false;
        if (!full_shape_hotkey_used_) {
            full_shape_hotkey_used_ = true;
            full_shape_ = !full_shape_;
            DebugLogKeyEvent(L"full shape hotkey handled diagnostic", wparam);
            DebugLog(full_shape_ ? L"full shape switched: on" : L"full shape switched: off");
            ShowStatus(context);
        } else {
            DebugLogKeyEvent(L"full shape hotkey repeat suppressed diagnostic", wparam);
        }
        return S_OK;
    }

    *eaten = ShouldEatKey(wparam) ? TRUE : FALSE;
    if (wparam == VK_SPACE || ToggleInputModeKey(wparam)) {
        DebugLogKeyEvent(*eaten ? L"key down eaten diagnostic" : L"key down passed diagnostic", wparam);
    }

    if (*eaten) {
        if (suppress_space_keydown_ && wparam == VK_SPACE) {
            suppress_space_keydown_ = false;
            DebugLog(L"key down eaten: suppressed shift+space");
            return S_OK;
        }

        if (wparam == VK_TAB && engine_.HasComposition() && (GetKeyState(VK_SHIFT) & 0x8000) != 0) {
            wparam = VK_LEFT;
            DebugLog(L"key down eaten: shift tab as previous candidate");
        }

        if (ToggleInputModeKey(wparam)) {
            if (!was_down) {
                shift_pending_toggle_ = true;
                shift_combined_key_ = false;
                full_shape_hotkey_used_ = false;
            }
            return S_OK;
        }

        const std::wstring punctuation = ChinesePunctuationForKey(wparam);
        if (!punctuation.empty() && engine_.HasComposition()) {
            shift_pending_toggle_ = false;
            shift_combined_key_ = false;
            suppress_space_keydown_ = false;
            DebugLog(L"key down eaten: commit candidate with punctuation");
            if (active_context_ != context) {
                if (active_context_) {
                    active_context_->Release();
                }
                active_context_ = context;
                if (active_context_) {
                    active_context_->AddRef();
                }
            }

            auto* session = new (std::nothrow) KeyEditSession(
                context,
                &engine_,
                &composition_range_,
                &candidate_window_,
                punctuation,
                true);
            if (!session) {
                return E_OUTOFMEMORY;
            }

            HRESULT edit_result = S_OK;
            HRESULT hr = context->RequestEditSession(client_id_, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
            if (hr == TF_E_SYNCHRONOUS || FAILED(hr)) {
                hr = context->RequestEditSession(client_id_, session, TF_ES_ASYNC | TF_ES_READWRITE, &edit_result);
            }
            session->Release();
            if (FAILED(hr)) {
                DebugLog(L"request punctuated candidate edit session failed");
                return hr;
            }
            if (FAILED(edit_result)) {
                DebugLog(L"punctuated candidate edit session result failed");
            }
            AdvanceChinesePunctuationState(wparam);
            return S_OK;
        }

        if (!punctuation.empty() && !engine_.HasComposition()) {
            shift_pending_toggle_ = false;
            shift_combined_key_ = false;
            suppress_space_keydown_ = false;
            DebugLog(L"key down eaten: chinese punctuation");
            if (active_context_ != context) {
                if (active_context_) {
                    active_context_->Release();
                }
                active_context_ = context;
                if (active_context_) {
                    active_context_->AddRef();
                }
            }

            auto* session = new (std::nothrow) KeyEditSession(context, &composition_range_, &candidate_window_, punctuation);
            if (!session) {
                return E_OUTOFMEMORY;
            }

            HRESULT edit_result = S_OK;
            HRESULT hr = context->RequestEditSession(client_id_, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
            if (hr == TF_E_SYNCHRONOUS || FAILED(hr)) {
                hr = context->RequestEditSession(client_id_, session, TF_ES_ASYNC | TF_ES_READWRITE, &edit_result);
            }
            session->Release();
            if (FAILED(hr)) {
                DebugLog(L"request punctuation edit session failed");
                return hr;
            }
            if (FAILED(edit_result)) {
                DebugLog(L"punctuation edit session result failed");
            }
            AdvanceChinesePunctuationState(wparam);
            return S_OK;
        }

        const std::wstring full_shape_text = FullShapeTextForKey(wparam);
        if (!full_shape_text.empty() && !engine_.HasComposition()) {
            shift_pending_toggle_ = false;
            shift_combined_key_ = false;
            suppress_space_keydown_ = false;
            DebugLog(L"key down eaten: full shape text");
            if (active_context_ != context) {
                if (active_context_) {
                    active_context_->Release();
                }
                active_context_ = context;
                if (active_context_) {
                    active_context_->AddRef();
                }
            }

            auto* session = new (std::nothrow) KeyEditSession(context, &composition_range_, &candidate_window_, full_shape_text);
            if (!session) {
                return E_OUTOFMEMORY;
            }

            HRESULT edit_result = S_OK;
            HRESULT hr = context->RequestEditSession(client_id_, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
            if (hr == TF_E_SYNCHRONOUS || FAILED(hr)) {
                hr = context->RequestEditSession(client_id_, session, TF_ES_ASYNC | TF_ES_READWRITE, &edit_result);
            }
            session->Release();
            if (FAILED(hr)) {
                DebugLog(L"request full shape edit session failed");
                return hr;
            }
            if (FAILED(edit_result)) {
                DebugLog(L"full shape edit session result failed");
            }
            return S_OK;
        }

        DebugLog(L"key down eaten");
        shift_pending_toggle_ = false;
        shift_combined_key_ = false;
        suppress_space_keydown_ = false;
        if (active_context_ != context) {
            if (active_context_) {
                active_context_->Release();
            }
            active_context_ = context;
            if (active_context_) {
                active_context_->AddRef();
            }
        }
        auto* session = new (std::nothrow) KeyEditSession(context, &engine_, &composition_range_, &candidate_window_, static_cast<UINT>(wparam));
        if (!session) {
            return E_OUTOFMEMORY;
        }

        HRESULT edit_result = S_OK;
        HRESULT hr = context->RequestEditSession(client_id_, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
        if (hr == TF_E_SYNCHRONOUS || FAILED(hr)) {
            hr = context->RequestEditSession(client_id_, session, TF_ES_ASYNC | TF_ES_READWRITE, &edit_result);
        }
        session->Release();
        if (FAILED(hr)) {
            DebugLog(L"request edit session failed");
            return hr;
        }
        if (FAILED(edit_result)) {
            DebugLog(L"edit session result failed");
        }
    }
    return S_OK;
}

void TextService::CandidateClickThunk(void* context, size_t index)
{
    if (!context) {
        return;
    }
    static_cast<TextService*>(context)->CommitCandidateFromWindow(index);
}

void TextService::CandidatePageThunk(void* context, bool next_page)
{
    if (!context) {
        return;
    }
    static_cast<TextService*>(context)->PageCandidateFromWindow(next_page);
}

void TextService::CandidateHoverThunk(void* context, size_t index)
{
    if (!context) {
        return;
    }
    static_cast<TextService*>(context)->HighlightCandidateFromWindow(index);
}

void TextService::CommitCandidateFromWindow(size_t index)
{
    if (!active_context_) {
        return;
    }

    auto* session = new (std::nothrow) KeyEditSession(active_context_, &engine_, &composition_range_, &candidate_window_, index);
    if (!session) {
        return;
    }

    HRESULT edit_result = S_OK;
    HRESULT hr = active_context_->RequestEditSession(client_id_, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
    if (hr == TF_E_SYNCHRONOUS || FAILED(hr)) {
        hr = active_context_->RequestEditSession(client_id_, session, TF_ES_ASYNC | TF_ES_READWRITE, &edit_result);
    }
    session->Release();
    if (FAILED(hr) || FAILED(edit_result)) {
        DebugLog(L"candidate click edit session failed");
    }
}

void TextService::HighlightCandidateFromWindow(size_t index)
{
    if (!active_context_ || !engine_.HasComposition()) {
        return;
    }

    auto* session = new (std::nothrow) KeyEditSession(
        active_context_,
        &engine_,
        &composition_range_,
        &candidate_window_,
        index,
        true);
    if (!session) {
        return;
    }

    HRESULT edit_result = S_OK;
    HRESULT hr = active_context_->RequestEditSession(client_id_, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
    if (hr == TF_E_SYNCHRONOUS || FAILED(hr)) {
        hr = active_context_->RequestEditSession(client_id_, session, TF_ES_ASYNC | TF_ES_READWRITE, &edit_result);
    }
    session->Release();
    if (FAILED(hr) || FAILED(edit_result)) {
        DebugLog(L"candidate hover edit session failed");
    }
}

void TextService::PageCandidateFromWindow(bool next_page)
{
    if (!active_context_ || !engine_.HasComposition()) {
        return;
    }

    auto* session = new (std::nothrow) KeyEditSession(
        active_context_,
        &engine_,
        &composition_range_,
        &candidate_window_,
        static_cast<UINT>(next_page ? VK_NEXT : VK_PRIOR));
    if (!session) {
        return;
    }

    HRESULT edit_result = S_OK;
    HRESULT hr = active_context_->RequestEditSession(client_id_, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
    if (hr == TF_E_SYNCHRONOUS || FAILED(hr)) {
        hr = active_context_->RequestEditSession(client_id_, session, TF_ES_ASYNC | TF_ES_READWRITE, &edit_result);
    }
    session->Release();
    if (FAILED(hr) || FAILED(edit_result)) {
        DebugLog(L"candidate wheel page edit session failed");
    }
}

void TextService::CancelComposition(ITfContext* context, const wchar_t* success_log, const wchar_t* failure_log)
{
    if (!context || !engine_.HasComposition()) {
        return;
    }

    auto* session =
        new (std::nothrow) KeyEditSession(context, &engine_, &composition_range_, &candidate_window_, static_cast<UINT>(VK_ESCAPE));
    if (!session) {
        return;
    }

    HRESULT edit_result = S_OK;
    HRESULT hr = context->RequestEditSession(client_id_, session, TF_ES_SYNC | TF_ES_READWRITE, &edit_result);
    if (hr == TF_E_SYNCHRONOUS || FAILED(hr)) {
        hr = context->RequestEditSession(client_id_, session, TF_ES_ASYNC | TF_ES_READWRITE, &edit_result);
    }
    session->Release();
    if (FAILED(hr) || FAILED(edit_result)) {
        DebugLog(failure_log);
    } else {
        DebugLog(success_log);
    }
}

void TextService::CancelCompositionForShortcut(ITfContext* context)
{
    CancelComposition(context, L"shortcut composition canceled", L"shortcut composition cancel failed");
}

void TextService::ReloadConfigIfChanged()
{
    FILETIME write_time = {};
    if (!ReadConfigWriteTime(&write_time)) {
        return;
    }
    if (!config_reload_pending_ && CompareFileTime(&write_time, &config_write_time_) == 0) {
        return;
    }

    if (engine_.HasComposition()) {
        config_reload_pending_ = true;
        DebugLog(L"config change detected; reload delayed during composition");
        return;
    }

    const bool old_capture_keys = config_.capture_keys;
    const bool old_full_shape_config = config_.full_shape;
    config_ = LoadConfig();
    engine_.Reset(config_);
    LogDictionaryStats(L"dictionary hot reloaded");
    config_write_time_ = write_time;
    config_reload_pending_ = false;
    if (old_full_shape_config != config_.full_shape) {
        full_shape_ = config_.full_shape;
    }

    DebugLog(old_capture_keys != config_.capture_keys ? L"config hot reloaded: capture changed" : L"config hot reloaded");
}

void TextService::LogDictionaryStats(const wchar_t* prefix)
{
    const DictionaryStats stats = engine_.GetDictionaryStats();
    wchar_t line[512] = {};
    StringCchPrintfW(
        line,
        ARRAYSIZE(line),
        L"%s: type=%s status=%s configured=%u loaded=%u codes=%llu candidates=%llu path=%s",
        prefix ? prefix : L"dictionary",
        stats.source_type.c_str(),
        stats.status.c_str(),
        stats.configured ? 1u : 0u,
        stats.loaded ? 1u : 0u,
        static_cast<unsigned long long>(stats.code_count),
        static_cast<unsigned long long>(stats.candidate_count),
        config_.dictionary_path.c_str());
    DebugLog(line);
}

bool TextService::ReadConfigWriteTime(FILETIME* write_time) const
{
    if (!write_time || config_.config_path.empty()) {
        return false;
    }

    HANDLE file = CreateFileW(
        config_.config_path.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    const BOOL ok = GetFileTime(file, nullptr, nullptr, write_time);
    CloseHandle(file);
    return ok != FALSE;
}

HRESULT TextService::OnTestKeyUp(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten)
{
    if (!eaten) {
        return E_POINTER;
    }
    *eaten = (ToggleInputModeKey(wparam) && shift_pending_toggle_) ||
              (wparam == VK_SPACE && shift_down_) ? TRUE : FALSE;
    if (wparam == VK_SPACE || ToggleInputModeKey(wparam)) {
        DebugLogKeyEvent(*eaten ? L"test key up eaten diagnostic" : L"test key up passed diagnostic", wparam);
    }
    return S_OK;
}

HRESULT TextService::OnKeyUp(ITfContext* context, WPARAM wparam, LPARAM, BOOL* eaten)
{
    if (!eaten) {
        return E_POINTER;
    }
    const bool shift_key = ToggleInputModeKey(wparam);
    const bool space_key = wparam == VK_SPACE;
    const bool full_shape_on_space_up = space_key && shift_down_ && !engine_.HasComposition() && !full_shape_hotkey_used_;
    *eaten = (shift_key && shift_pending_toggle_) || full_shape_on_space_up ? TRUE : FALSE;
    if (wparam == VK_SPACE || ToggleInputModeKey(wparam)) {
        DebugLogKeyEvent(*eaten ? L"key up eaten diagnostic" : L"key up passed diagnostic", wparam);
    }
    if (full_shape_on_space_up) {
        full_shape_hotkey_used_ = true;
        shift_pending_toggle_ = false;
        shift_combined_key_ = true;
        full_shape_ = !full_shape_;
        space_down_ = false;
        DebugLogKeyEvent(L"full shape hotkey handled on space up diagnostic", wparam);
        DebugLog(full_shape_ ? L"full shape switched: on" : L"full shape switched: off");
        ShowStatus(context);
        return S_OK;
    }

    if (shift_key && *eaten) {
        if (shift_combined_key_) {
            shift_pending_toggle_ = false;
            shift_combined_key_ = false;
            suppress_space_keydown_ = false;
            shift_down_ = false;
            full_shape_hotkey_used_ = false;
            DebugLog(L"input mode switch skipped: shift combined key");
            return S_OK;
        }
        shift_pending_toggle_ = false;
        shift_combined_key_ = false;
        suppress_space_keydown_ = false;
        shift_down_ = false;
        full_shape_hotkey_used_ = false;
        chinese_mode_ = !chinese_mode_;
        candidate_window_.Hide();
        DebugLog(chinese_mode_ ? L"input mode switched: chinese" : L"input mode switched: english");
        ShowStatus(context);
        return S_OK;
    }

    if (shift_key) {
        shift_down_ = false;
        shift_pending_toggle_ = false;
        shift_combined_key_ = false;
        full_shape_hotkey_used_ = false;
    }
    if (space_key) {
        space_down_ = false;
    }
    return S_OK;
}

HRESULT TextService::OnSetThreadFocus()
{
    DebugLog(L"thread focus set");
    return S_OK;
}

HRESULT TextService::OnKillThreadFocus()
{
    ClearTransientFocusState(L"thread focus lost");
    return S_OK;
}

HRESULT TextService::OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten)
{
    if (!eaten) {
        return E_POINTER;
    }
    DebugLog(L"preserved key callback");
    if (IsFullShapePreservedKeyGuid(guid) && !engine_.HasComposition()) {
        shift_pending_toggle_ = false;
        shift_combined_key_ = false;
        suppress_space_keydown_ = false;
        full_shape_ = !full_shape_;
        *eaten = TRUE;
        DebugLog(full_shape_ ? L"full shape switched by preserved key: on" : L"full shape switched by preserved key: off");
        ShowStatus(context);
        return S_OK;
    }
    *eaten = FALSE;
    return S_OK;
}

HRESULT TextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** enum_info)
{
    DebugLog(L"text service display attribute: enum requested");
    return CreateDisplayAttributeEnumerator(enum_info);
}

HRESULT TextService::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** info)
{
    if (!info) {
        return E_POINTER;
    }
    *info = nullptr;

    if (!IsEqualGUID(guid, kPreeditDisplayAttributeGuid)) {
        DebugLog(L"text service display attribute: unknown guid requested");
        return E_INVALIDARG;
    }

    DebugLog(L"text service display attribute: info requested");
    auto* attribute_info = new (std::nothrow) DisplayAttributeInfo();
    if (!attribute_info) {
        return E_OUTOFMEMORY;
    }
    *info = attribute_info;
    return S_OK;
}

HRESULT TextService::AdviseKeyEventSink()
{
    if (!thread_mgr_ || key_event_sink_advised_) {
        return S_OK;
    }

    ITfKeystrokeMgr* keystroke_mgr = nullptr;
    HRESULT hr = thread_mgr_->QueryInterface(IID_PPV_ARGS(&keystroke_mgr));
    if (FAILED(hr)) {
        return hr;
    }

    hr = keystroke_mgr->AdviseKeyEventSink(client_id_, static_cast<ITfKeyEventSink*>(this), TRUE);
    if (SUCCEEDED(hr)) {
        key_event_sink_advised_ = true;
        HRESULT preserve_hr = PreserveFullShapeKey(keystroke_mgr);
        if (FAILED(preserve_hr)) {
            DebugLogHresult(L"Preserve full shape key failed", preserve_hr);
        }
    }
    keystroke_mgr->Release();
    return hr;
}

void TextService::UnadviseKeyEventSink()
{
    if (!thread_mgr_ || !key_event_sink_advised_) {
        return;
    }

    ITfKeystrokeMgr* keystroke_mgr = nullptr;
    if (SUCCEEDED(thread_mgr_->QueryInterface(IID_PPV_ARGS(&keystroke_mgr)))) {
        UnpreserveFullShapeKey(keystroke_mgr);
        keystroke_mgr->UnadviseKeyEventSink(client_id_);
        keystroke_mgr->Release();
    }
    key_event_sink_advised_ = false;
}

HRESULT TextService::AdviseThreadFocusSink()
{
    if (!thread_mgr_ || thread_focus_sink_cookie_ != TF_INVALID_COOKIE) {
        return S_OK;
    }

    ITfSource* source = nullptr;
    HRESULT hr = thread_mgr_->QueryInterface(IID_PPV_ARGS(&source));
    if (FAILED(hr)) {
        return hr;
    }

    hr = source->AdviseSink(IID_ITfThreadFocusSink, static_cast<ITfThreadFocusSink*>(this), &thread_focus_sink_cookie_);
    source->Release();
    if (SUCCEEDED(hr)) {
        DebugLog(L"thread focus sink advised");
    } else {
        thread_focus_sink_cookie_ = TF_INVALID_COOKIE;
        DebugLogHresult(L"Advise thread focus sink failed", hr);
    }
    return hr;
}

void TextService::UnadviseThreadFocusSink()
{
    if (!thread_mgr_ || thread_focus_sink_cookie_ == TF_INVALID_COOKIE) {
        return;
    }

    ITfSource* source = nullptr;
    if (SUCCEEDED(thread_mgr_->QueryInterface(IID_PPV_ARGS(&source)))) {
        HRESULT hr = source->UnadviseSink(thread_focus_sink_cookie_);
        if (FAILED(hr)) {
            DebugLogHresult(L"Unadvise thread focus sink failed", hr);
        }
        source->Release();
    }
    thread_focus_sink_cookie_ = TF_INVALID_COOKIE;
}

HRESULT TextService::PreserveFullShapeKey(ITfKeystrokeMgr* keystroke_mgr)
{
    if (!keystroke_mgr || full_shape_key_preserved_) {
        return S_OK;
    }

    const wchar_t description[] = L"Toggle full shape";
    const struct {
        GUID guid;
        UINT modifier;
        const wchar_t* log_name;
    } keys[] = {
        {kFullShapePreservedKeyGuid, TF_MOD_SHIFT, L"generic shift"},
        {kFullShapeLeftShiftPreservedKeyGuid, TF_MOD_LSHIFT, L"left shift"},
        {kFullShapeRightShiftPreservedKeyGuid, TF_MOD_RSHIFT, L"right shift"},
    };

    bool any_succeeded = false;
    HRESULT last_hr = S_OK;
    for (const auto& key : keys) {
        TF_PRESERVEDKEY preserved_key = {};
        preserved_key.uVKey = VK_SPACE;
        preserved_key.uModifiers = key.modifier;

        HRESULT hr = keystroke_mgr->PreserveKey(
            client_id_,
            key.guid,
            &preserved_key,
            description,
            ARRAYSIZE(description) - 1);
        if (SUCCEEDED(hr)) {
            any_succeeded = true;
            wchar_t line[96] = {};
            StringCchPrintfW(line, ARRAYSIZE(line), L"full shape preserved key registered: %s", key.log_name);
            DebugLog(line);
        } else {
            last_hr = hr;
            wchar_t line[96] = {};
            StringCchPrintfW(line, ARRAYSIZE(line), L"Preserve full shape key failed: %s", key.log_name);
            DebugLogHresult(line, hr);
        }
    }

    if (any_succeeded) {
        full_shape_key_preserved_ = true;
        return S_OK;
    }
    return last_hr;
}

void TextService::UnpreserveFullShapeKey(ITfKeystrokeMgr* keystroke_mgr)
{
    if (!keystroke_mgr || !full_shape_key_preserved_) {
        return;
    }

    const struct {
        GUID guid;
        UINT modifier;
    } keys[] = {
        {kFullShapePreservedKeyGuid, TF_MOD_SHIFT},
        {kFullShapeLeftShiftPreservedKeyGuid, TF_MOD_LSHIFT},
        {kFullShapeRightShiftPreservedKeyGuid, TF_MOD_RSHIFT},
    };

    for (const auto& key : keys) {
        TF_PRESERVEDKEY preserved_key = {};
        preserved_key.uVKey = VK_SPACE;
        preserved_key.uModifiers = key.modifier;
        HRESULT hr = keystroke_mgr->UnpreserveKey(key.guid, &preserved_key);
        if (FAILED(hr)) {
            DebugLogHresult(L"Unpreserve full shape key failed", hr);
        }
    }
    full_shape_key_preserved_ = false;
}

void TextService::ClearCompositionRange()
{
    if (composition_range_) {
        composition_range_->Release();
        composition_range_ = nullptr;
    }
}

void TextService::RequestFocusCleanupEditSession(ITfContext* context, ITfRange* range)
{
    if (!context || !range) {
        return;
    }

    auto* session = new (std::nothrow) FocusCleanupEditSession(context, range);
    if (!session) {
        DebugLog(L"focus cleanup edit session allocation failed");
        return;
    }

    HRESULT edit_result = S_OK;
    HRESULT hr = context->RequestEditSession(client_id_, session, TF_ES_ASYNC | TF_ES_READWRITE, &edit_result);
    session->Release();
    if (FAILED(hr)) {
        DebugLogHresult(L"focus cleanup edit session request failed", hr);
    } else {
        DebugLog(L"focus cleanup edit session requested");
    }
}

void TextService::ClearTransientFocusState(const wchar_t* log_message)
{
    candidate_window_.Hide();
    status_window_.Hide();
    shift_pending_toggle_ = false;
    shift_combined_key_ = false;
    suppress_space_keydown_ = false;
    shift_down_ = false;
    space_down_ = false;
    full_shape_hotkey_used_ = false;
    if (engine_.HasComposition()) {
        RequestFocusCleanupEditSession(active_context_, composition_range_);
        engine_.Reset(config_);
        ClearCompositionRange();
    }
    if (active_context_) {
        active_context_->Release();
        active_context_ = nullptr;
    }
    DebugLog(log_message);
}

void TextService::ShowStatus(ITfContext* context)
{
    POINT anchor = GetStatusAnchor(context);
    std::wstring text = chinese_mode_ ? L"\x4E2D" : L"\x82F1";
    text += L" ";
    text += full_shape_ ? L"\x5168" : L"\x534A";
    status_window_.Show(text, anchor);
}

POINT TextService::GetStatusAnchor(ITfContext* context)
{
    UNREFERENCED_PARAMETER(context);

    POINT anchor = {};
    GetCursorPos(&anchor);

    GUITHREADINFO gui = {};
    gui.cbSize = sizeof(gui);
    if (GetGUIThreadInfo(0, &gui) && gui.hwndCaret) {
        POINT caret = {gui.rcCaret.right, gui.rcCaret.bottom};
        if (ClientToScreen(gui.hwndCaret, &caret)) {
            anchor = caret;
        }
    }
    return anchor;
}

bool TextService::ShouldEatKey(WPARAM key) const
{
    if (!config_.capture_keys || !IsHandledVirtualKey(key)) {
        return false;
    }

    if (ControlOrAltDown()) {
        return false;
    }

    if (ToggleFullShapeKey(key)) {
        return !engine_.HasComposition();
    }

    if (ToggleInputModeKey(key)) {
        return !engine_.HasComposition();
    }

    if (engine_.HasComposition()) {
        if (key == VK_DELETE) {
            return (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        }
        return true;
    }

    if (!FullShapeTextForKey(key).empty()) {
        return true;
    }

    return chinese_mode_ && ((key >= L'A' && key <= L'Z') || !ChinesePunctuationForKey(key).empty());
}

bool TextService::ControlOrAltDown() const
{
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0 ||
           (GetKeyState(VK_LCONTROL) & 0x8000) != 0 ||
           (GetKeyState(VK_RCONTROL) & 0x8000) != 0 ||
           (GetKeyState(VK_MENU) & 0x8000) != 0 ||
           (GetKeyState(VK_LMENU) & 0x8000) != 0 ||
           (GetKeyState(VK_RMENU) & 0x8000) != 0;
}

bool TextService::ToggleInputModeKey(WPARAM key) const
{
    return key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT;
}

bool TextService::ToggleFullShapeKey(WPARAM key) const
{
    if (key == VK_SPACE) {
        return shift_down_ || shift_pending_toggle_ || (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    }
    if (ToggleInputModeKey(key)) {
        return space_down_ || (GetKeyState(VK_SPACE) & 0x8000) != 0;
    }
    return false;
}

std::wstring TextService::ChinesePunctuationForKey(WPARAM key) const
{
    if (!chinese_mode_ || !config_.chinese_punctuation) {
        return L"";
    }

    const bool shifted = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    switch (key) {
    case VK_OEM_COMMA:
        return shifted ? L"\x300A" : L"\xFF0C";
    case VK_OEM_PERIOD:
        return shifted ? L"\x300B" : L"\x3002";
    case VK_OEM_1:
        return shifted ? L"\xFF1A" : L"\xFF1B";
    case VK_OEM_2:
        return shifted ? L"\xFF1F" : L"\x3001";
    case VK_OEM_3:
        return shifted ? L"\xFF5E" : L"\x00B7";
    case VK_OEM_5:
        return shifted ? L"\xFF5C" : L"\x3001";
    case VK_OEM_4:
        return shifted ? L"\x3016" : L"\x3010";
    case VK_OEM_6:
        return shifted ? L"\x3017" : L"\x3011";
    case VK_OEM_7:
        return shifted
                   ? (double_quote_open_ ? L"\x201C" : L"\x201D")
                   : (single_quote_open_ ? L"\x2018" : L"\x2019");
    case VK_OEM_MINUS:
        return shifted ? L"\x2014" : L"\xFF0D";
    case VK_OEM_PLUS:
        return shifted ? L"\xFF0B" : L"\xFF1D";
    case L'0':
        return shifted ? L"\xFF09" : L"";
    case L'1':
        return shifted ? L"\xFF01" : L"";
    case L'2':
        return shifted ? L"\xFF20" : L"";
    case L'3':
        return shifted ? L"\xFF03" : L"";
    case L'4':
        return shifted ? L"\xFFE5" : L"";
    case L'5':
        return shifted ? L"\xFF05" : L"";
    case L'6':
        return shifted ? L"\x2026\x2026" : L"";
    case L'7':
        return shifted ? L"\xFF06" : L"";
    case L'8':
        return shifted ? L"\xFF0A" : L"";
    case L'9':
        return shifted ? L"\xFF08" : L"";
    default:
        return L"";
    }
}

void TextService::AdvanceChinesePunctuationState(WPARAM key)
{
    if (key != VK_OEM_7) {
        return;
    }

    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
        double_quote_open_ = !double_quote_open_;
    } else {
        single_quote_open_ = !single_quote_open_;
    }
}

std::wstring TextService::FullShapeTextForKey(WPARAM key) const
{
    if (!full_shape_ || engine_.HasComposition()) {
        return L"";
    }

    const bool shifted = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (key >= L'A' && key <= L'Z') {
        const bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
        const wchar_t base = (shifted ^ caps) ? L'\xFF21' : L'\xFF41';
        return std::wstring(1, static_cast<wchar_t>(base + key - L'A'));
    }

    if (key >= L'0' && key <= L'9') {
        if (!shifted) {
            return std::wstring(1, static_cast<wchar_t>(L'\xFF10' + key - L'0'));
        }
        static const wchar_t shifted_digits[] = {
            L'\xFF09', L'\xFF01', L'\xFF20', L'\xFF03', L'\xFFE5',
            L'\xFF05', L'\x2026', L'\xFF06', L'\xFF0A', L'\xFF08'};
        return std::wstring(1, shifted_digits[key - L'0']);
    }

    if (key == VK_SPACE) {
        return shifted ? L"" : L"\x3000";
    }

    switch (key) {
    case VK_OEM_COMMA:
        return shifted ? L"\xFF1C" : L"\xFF0C";
    case VK_OEM_PERIOD:
        return shifted ? L"\xFF1E" : L"\xFF0E";
    case VK_OEM_1:
        return shifted ? L"\xFF1A" : L"\xFF1B";
    case VK_OEM_2:
        return shifted ? L"\xFF1F" : L"\xFF0F";
    case VK_OEM_3:
        return shifted ? L"\xFF5E" : L"\xFF40";
    case VK_OEM_4:
        return shifted ? L"\xFF5B" : L"\xFF3B";
    case VK_OEM_5:
        return shifted ? L"\xFF5C" : L"\xFF3C";
    case VK_OEM_6:
        return shifted ? L"\xFF5D" : L"\xFF3D";
    case VK_OEM_7:
        return shifted ? L"\xFF02" : L"\xFF07";
    case VK_OEM_MINUS:
        return shifted ? L"\xFF3F" : L"\xFF0D";
    case VK_OEM_PLUS:
        return shifted ? L"\xFF0B" : L"\xFF1D";
    default:
        return L"";
    }
}

bool TextService::IsHandledVirtualKey(WPARAM key)
{
    return (key >= L'A' && key <= L'Z') ||
           (key >= L'0' && key <= L'9') ||
           key == VK_SPACE ||
           key == VK_BACK ||
           key == VK_DELETE ||
           key == VK_ESCAPE ||
           key == VK_RETURN ||
           key == VK_TAB ||
           key == VK_LEFT ||
           key == VK_RIGHT ||
           key == VK_UP ||
           key == VK_DOWN ||
           key == VK_HOME ||
           key == VK_END ||
           key == VK_NEXT ||
           key == VK_PRIOR ||
           key == VK_SHIFT ||
           key == VK_LSHIFT ||
           key == VK_RSHIFT ||
           key == VK_OEM_COMMA ||
           key == VK_OEM_PERIOD ||
           key == VK_OEM_1 ||
           key == VK_OEM_2 ||
           key == VK_OEM_4 ||
           key == VK_OEM_6 ||
           key == VK_OEM_7 ||
           key == VK_OEM_3 ||
           key == VK_OEM_5 ||
           key == VK_OEM_MINUS ||
           key == VK_OEM_PLUS;
}

}  // namespace arrowinput
