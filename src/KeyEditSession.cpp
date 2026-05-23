#include "KeyEditSession.h"
#include "DisplayAttribute.h"

namespace arrowinput {

namespace {

HRESULT SetSelectionText(ITfContext* context, TfEditCookie edit_cookie, const std::wstring& commit)
{
    TF_SELECTION selection = {};
    ULONG fetched = 0;
    HRESULT hr = context->GetSelection(edit_cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
    if (FAILED(hr)) {
        DebugLogHresult(L"fallback GetSelection failed", hr);
        return hr;
    }
    if (fetched == 0 || !selection.range) {
        return E_FAIL;
    }

    hr = selection.range->SetText(edit_cookie, 0, commit.c_str(), static_cast<LONG>(commit.size()));
    if (FAILED(hr)) {
        DebugLogHresult(L"fallback SetText failed", hr);
    } else {
        hr = selection.range->Collapse(edit_cookie, TF_ANCHOR_END);
        if (FAILED(hr)) {
            DebugLogHresult(L"fallback Collapse failed", hr);
        } else {
            selection.style.ase = TF_AE_NONE;
            selection.style.fInterimChar = FALSE;
            hr = context->SetSelection(edit_cookie, 1, &selection);
            if (FAILED(hr)) {
                DebugLogHresult(L"fallback SetSelection failed", hr);
            }
        }
    }

    ITfRange* range = selection.range;
    range->Release();
    return hr;
}

HRESULT ClearDisplayAttribute(ITfContext* context, TfEditCookie edit_cookie, ITfRange* range)
{
    ITfProperty* property = nullptr;
    HRESULT hr = context->GetProperty(GUID_PROP_ATTRIBUTE, &property);
    if (FAILED(hr)) {
        DebugLogHresult(L"GetProperty for clear attribute failed", hr);
        return hr;
    }

    hr = property->Clear(edit_cookie, range);
    if (FAILED(hr)) {
        DebugLogHresult(L"Clear display attribute failed", hr);
    }
    property->Release();
    return hr;
}

HRESULT ApplyPreeditDisplayAttribute(ITfContext* context, TfEditCookie edit_cookie, ITfRange* range)
{
    TfGuidAtom atom = TF_INVALID_GUIDATOM;
    HRESULT hr = GetPreeditDisplayAttributeAtom(&atom);
    if (FAILED(hr)) {
        DebugLogHresult(L"GetPreeditDisplayAttributeAtom failed", hr);
        return hr;
    }

    ITfProperty* property = nullptr;
    hr = context->GetProperty(GUID_PROP_ATTRIBUTE, &property);
    if (FAILED(hr)) {
        DebugLogHresult(L"GetProperty GUID_PROP_ATTRIBUTE failed", hr);
        return hr;
    }

    VARIANT value = {};
    value.vt = VT_I4;
    value.lVal = static_cast<LONG>(atom);
    hr = property->SetValue(edit_cookie, range, &value);
    if (FAILED(hr)) {
        DebugLogHresult(L"SetValue display attribute failed", hr);
    } else {
        DebugLog(L"SetValue display attribute succeeded");
    }

    property->Release();
    return hr;
}

HRESULT CollapseRangeToEnd(ITfContext* context, TfEditCookie edit_cookie, ITfRange* range)
{
    ITfRange* caret_range = nullptr;
    HRESULT hr = range->Clone(&caret_range);
    if (FAILED(hr)) {
        DebugLogHresult(L"CollapseRangeToEnd Clone failed", hr);
        return hr;
    }

    hr = caret_range->Collapse(edit_cookie, TF_ANCHOR_END);
    if (FAILED(hr)) {
        DebugLogHresult(L"CollapseRangeToEnd Collapse failed", hr);
        caret_range->Release();
        return hr;
    }

    TF_SELECTION selection = {};
    selection.range = caret_range;
    selection.style.ase = TF_AE_NONE;
    selection.style.fInterimChar = FALSE;
    hr = context->SetSelection(edit_cookie, 1, &selection);
    if (FAILED(hr)) {
        DebugLogHresult(L"CollapseRangeToEnd SetSelection failed", hr);
    }
    caret_range->Release();
    return hr;
}

HRESULT InsertNewPreedit(ITfContext* context, TfEditCookie edit_cookie, ITfRange** composition_range, const std::wstring& text)
{
    ITfInsertAtSelection* insert_at_selection = nullptr;
    HRESULT hr = context->QueryInterface(IID_PPV_ARGS(&insert_at_selection));
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit QI InsertAtSelection failed", hr);
        return hr;
    }

    ITfRange* inserted_range = nullptr;
    hr = insert_at_selection->InsertTextAtSelection(
        edit_cookie,
        0,
        text.c_str(),
        static_cast<LONG>(text.size()),
        &inserted_range);
    insert_at_selection->Release();

    if (FAILED(hr)) {
        DebugLogHresult(L"preedit InsertTextAtSelection failed", hr);
        return hr;
    }

    if (*composition_range) {
        (*composition_range)->Release();
    }
    *composition_range = inserted_range;
    ApplyPreeditDisplayAttribute(context, edit_cookie, *composition_range);
    return CollapseRangeToEnd(context, edit_cookie, *composition_range);
}

HRESULT ReplacePreedit(ITfContext* context, TfEditCookie edit_cookie, ITfRange** composition_range, const std::wstring& text)
{
    if (!composition_range) {
        return E_POINTER;
    }

    if (!*composition_range) {
        if (text.empty()) {
            return S_OK;
        }
        return InsertNewPreedit(context, edit_cookie, composition_range, text);
    }

    HRESULT hr = (*composition_range)->SetText(edit_cookie, 0, text.c_str(), static_cast<LONG>(text.size()));
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit SetText failed", hr);
        return hr;
    }

    if (text.empty()) {
        ClearDisplayAttribute(context, edit_cookie, *composition_range);
        (*composition_range)->Release();
        *composition_range = nullptr;
        return S_OK;
    }

    ApplyPreeditDisplayAttribute(context, edit_cookie, *composition_range);
    return CollapseRangeToEnd(context, edit_cookie, *composition_range);
}

HRESULT CommitText(ITfContext* context, TfEditCookie edit_cookie, ITfRange** composition_range, const std::wstring& commit)
{
    if (commit.empty()) {
        return ReplacePreedit(context, edit_cookie, composition_range, L"");
    }

    if (composition_range && *composition_range) {
        ClearDisplayAttribute(context, edit_cookie, *composition_range);
        HRESULT hr = (*composition_range)->SetText(edit_cookie, 0, commit.c_str(), static_cast<LONG>(commit.size()));
        if (FAILED(hr)) {
            DebugLogHresult(L"commit SetText over preedit failed", hr);
            return hr;
        }
        hr = CollapseRangeToEnd(context, edit_cookie, *composition_range);
        (*composition_range)->Release();
        *composition_range = nullptr;
        return hr;
    }

    ITfInsertAtSelection* insert_at_selection = nullptr;
    HRESULT hr = context->QueryInterface(IID_PPV_ARGS(&insert_at_selection));
    if (FAILED(hr)) {
        return hr;
    }

    hr = insert_at_selection->InsertTextAtSelection(
        edit_cookie,
        TF_IAS_NOQUERY,
        commit.c_str(),
        static_cast<LONG>(commit.size()),
        nullptr);

    insert_at_selection->Release();
    if (SUCCEEDED(hr)) {
        return S_OK;
    }

    DebugLogHresult(L"InsertTextAtSelection failed", hr);
    return SetSelectionText(context, edit_cookie, commit);
}

POINT GetCandidateAnchor(ITfContext* context, TfEditCookie edit_cookie, ITfRange* composition_range)
{
    POINT anchor = {};
    GetCursorPos(&anchor);

    ITfContextView* view = nullptr;
    if (FAILED(context->GetActiveView(&view)) || !view) {
        return anchor;
    }

    RECT rect = {};
    BOOL clipped = FALSE;
    ITfRange* range = composition_range;
    if (!range) {
        TF_SELECTION selection = {};
        ULONG fetched = 0;
        if (SUCCEEDED(context->GetSelection(edit_cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) &&
            fetched > 0 && selection.range) {
            range = selection.range;
        }
    } else {
        range->AddRef();
    }

    if (range) {
        if (SUCCEEDED(view->GetTextExt(edit_cookie, range, &rect, &clipped))) {
            anchor.x = rect.left;
            anchor.y = rect.bottom;
        }
        range->Release();
    }

    view->Release();
    return anchor;
}

}  // namespace

KeyEditSession::KeyEditSession(
    ITfContext* context,
    Engine* engine,
    ITfRange** composition_range,
    CandidateWindow* candidate_window,
    UINT key)
    : ref_count_(1),
      context_(context),
      engine_(engine),
      composition_range_(composition_range),
      candidate_window_(candidate_window),
      key_(key),
      select_candidate_(false),
      direct_commit_(false),
      commit_current_candidate_(false),
      highlight_candidate_(false),
      candidate_index_(0)
{
    if (context_) {
        context_->AddRef();
    }
}

KeyEditSession::KeyEditSession(
    ITfContext* context,
    Engine* engine,
    ITfRange** composition_range,
    CandidateWindow* candidate_window,
    size_t candidate_index)
    : ref_count_(1),
      context_(context),
      engine_(engine),
      composition_range_(composition_range),
      candidate_window_(candidate_window),
      key_(0),
      select_candidate_(true),
      direct_commit_(false),
      commit_current_candidate_(false),
      highlight_candidate_(false),
      candidate_index_(candidate_index)
{
    if (context_) {
        context_->AddRef();
    }
}

KeyEditSession::KeyEditSession(
    ITfContext* context,
    Engine* engine,
    ITfRange** composition_range,
    CandidateWindow* candidate_window,
    size_t candidate_index,
    bool highlight_candidate)
    : ref_count_(1),
      context_(context),
      engine_(engine),
      composition_range_(composition_range),
      candidate_window_(candidate_window),
      key_(0),
      select_candidate_(false),
      direct_commit_(false),
      commit_current_candidate_(false),
      highlight_candidate_(highlight_candidate),
      candidate_index_(candidate_index)
{
    if (context_) {
        context_->AddRef();
    }
}

KeyEditSession::KeyEditSession(
    ITfContext* context,
    Engine* engine,
    ITfRange** composition_range,
    CandidateWindow* candidate_window,
    const std::wstring& commit_suffix,
    bool commit_current_candidate)
    : ref_count_(1),
      context_(context),
      engine_(engine),
      composition_range_(composition_range),
      candidate_window_(candidate_window),
      key_(0),
      select_candidate_(false),
      direct_commit_(false),
      commit_current_candidate_(commit_current_candidate),
      highlight_candidate_(false),
      candidate_index_(0),
      commit_text_(commit_suffix)
{
    if (context_) {
        context_->AddRef();
    }
}

KeyEditSession::KeyEditSession(
    ITfContext* context,
    ITfRange** composition_range,
    CandidateWindow* candidate_window,
    const std::wstring& commit_text)
    : ref_count_(1),
      context_(context),
      engine_(nullptr),
      composition_range_(composition_range),
      candidate_window_(candidate_window),
      key_(0),
      select_candidate_(false),
      direct_commit_(true),
      commit_current_candidate_(false),
      highlight_candidate_(false),
      candidate_index_(0),
      commit_text_(commit_text)
{
    if (context_) {
        context_->AddRef();
    }
}

KeyEditSession::~KeyEditSession()
{
    if (context_) {
        context_->Release();
    }
}

HRESULT KeyEditSession::QueryInterface(REFIID iid, void** result)
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

ULONG KeyEditSession::AddRef()
{
    return ++ref_count_;
}

ULONG KeyEditSession::Release()
{
    ULONG count = --ref_count_;
    if (count == 0) {
        delete this;
    }
    return count;
}

HRESULT KeyEditSession::DoEditSession(TfEditCookie edit_cookie)
{
    if (!context_) {
        return E_UNEXPECTED;
    }

    if (direct_commit_) {
        HRESULT hr = CommitText(context_, edit_cookie, composition_range_, commit_text_);
        if (candidate_window_) {
            candidate_window_->Hide();
        }
        DebugLog(SUCCEEDED(hr) ? L"edit session: direct text committed" : L"edit session: direct commit failed");
        return hr;
    }

    if (!engine_) {
        return E_UNEXPECTED;
    }

    const KeyResult result = highlight_candidate_
                                 ? engine_->HighlightCandidate(candidate_index_)
                                 : (commit_current_candidate_
                                        ? engine_->CommitCurrentCandidateWithSuffix(commit_text_)
                                        : (select_candidate_ ? engine_->SelectCandidate(candidate_index_) : engine_->HandleVirtualKey(key_)));
    if (!result.commit.empty()) {
        HRESULT hr = CommitText(context_, edit_cookie, composition_range_, result.commit);
        if (candidate_window_) {
            candidate_window_->Hide();
        }
        DebugLog(SUCCEEDED(hr) ? L"edit session: committed text" : L"edit session: commit failed");
        return hr;
    }

    if (result.clear_composition) {
        if (candidate_window_) {
            candidate_window_->Hide();
        }
        return ReplacePreedit(context_, edit_cookie, composition_range_, L"");
    }

    if (result.composition_changed) {
        HRESULT hr = ReplacePreedit(context_, edit_cookie, composition_range_, result.composition);
        if (candidate_window_) {
            POINT anchor = GetCandidateAnchor(context_, edit_cookie, composition_range_ ? *composition_range_ : nullptr);
            candidate_window_->Show(
                result.candidates,
                result.selected_candidate,
                result.page_start,
                result.page_size,
                result.total_candidates,
                anchor);
        }
        DebugLog(SUCCEEDED(hr) ? L"edit session: preedit updated" : L"edit session: preedit update failed");
        return hr;
    }

    if (result.candidates_changed) {
        if (candidate_window_) {
            POINT anchor = GetCandidateAnchor(context_, edit_cookie, composition_range_ ? *composition_range_ : nullptr);
            candidate_window_->Show(
                result.candidates,
                result.selected_candidate,
                result.page_start,
                result.page_size,
                result.total_candidates,
                anchor);
        }
        DebugLog(L"edit session: candidates updated");
        return S_OK;
    }

    DebugLog(L"edit session: no document change");
    return S_OK;
}

}  // namespace arrowinput
