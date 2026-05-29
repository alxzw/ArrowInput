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

void ReleaseRange(ITfRange** range)
{
    if (range && *range) {
        (*range)->Release();
        *range = nullptr;
    }
}

HRESULT StorePreeditAnchor(TfEditCookie edit_cookie, ITfRange* source_range, ITfRange** preedit_anchor_range)
{
    if (!source_range || !preedit_anchor_range) {
        return E_POINTER;
    }

    ITfRange* anchor = nullptr;
    HRESULT hr = source_range->Clone(&anchor);
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit anchor Clone failed", hr);
        return hr;
    }

    hr = anchor->Collapse(edit_cookie, TF_ANCHOR_START);
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit anchor Collapse failed", hr);
        anchor->Release();
        return hr;
    }

    hr = anchor->SetGravity(edit_cookie, TF_GRAVITY_BACKWARD, TF_GRAVITY_BACKWARD);
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit anchor SetGravity failed", hr);
        anchor->Release();
        return hr;
    }

    ReleaseRange(preedit_anchor_range);
    *preedit_anchor_range = anchor;
    return S_OK;
}

HRESULT CreatePreeditRangeFromAnchor(
    TfEditCookie edit_cookie,
    ITfRange* preedit_anchor_range,
    size_t text_length,
    ITfRange** range_out)
{
    if (!preedit_anchor_range || !range_out) {
        return E_POINTER;
    }

    *range_out = nullptr;
    ITfRange* range = nullptr;
    HRESULT hr = preedit_anchor_range->Clone(&range);
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit range Clone from anchor failed", hr);
        return hr;
    }

    hr = range->Collapse(edit_cookie, TF_ANCHOR_START);
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit range Collapse failed", hr);
        range->Release();
        return hr;
    }

    if (text_length > 0) {
        LONG shifted = 0;
        const LONG wanted = static_cast<LONG>(text_length);
        hr = range->ShiftEnd(edit_cookie, wanted, &shifted, nullptr);
        if (FAILED(hr) || shifted != wanted) {
            if (FAILED(hr)) {
                DebugLogHresult(L"preedit range ShiftEnd failed", hr);
            } else {
                DebugLog(L"preedit range ShiftEnd shifted less than expected");
                hr = E_FAIL;
            }
            range->Release();
            return hr;
        }
    }

    hr = range->SetGravity(edit_cookie, TF_GRAVITY_BACKWARD, TF_GRAVITY_FORWARD);
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit range SetGravity failed", hr);
        range->Release();
        return hr;
    }

    *range_out = range;
    return S_OK;
}

HRESULT StartPreeditComposition(
    ITfContext* context,
    TfEditCookie edit_cookie,
    ITfRange* range,
    ITfComposition** tsf_composition,
    ITfCompositionSink* composition_sink)
{
    if (!context || !range || !tsf_composition || *tsf_composition) {
        return S_OK;
    }

    ITfContextComposition* context_composition = nullptr;
    HRESULT hr = context->QueryInterface(IID_PPV_ARGS(&context_composition));
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit QI ContextComposition failed", hr);
        return hr;
    }

    hr = context_composition->StartComposition(edit_cookie, range, composition_sink, tsf_composition);
    context_composition->Release();
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit StartComposition failed", hr);
    }
    return hr;
}

void EndPreeditComposition(TfEditCookie edit_cookie, ITfComposition** tsf_composition)
{
    if (!tsf_composition || !*tsf_composition) {
        return;
    }
    HRESULT hr = (*tsf_composition)->EndComposition(edit_cookie);
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit EndComposition failed", hr);
    }
    (*tsf_composition)->Release();
    *tsf_composition = nullptr;
}

HRESULT InsertNewPreedit(
    ITfContext* context,
    TfEditCookie edit_cookie,
    ITfRange** composition_range,
    ITfRange** preedit_anchor_range,
    ITfComposition** tsf_composition,
    ITfCompositionSink* composition_sink,
    size_t* preedit_text_length,
    const std::wstring& text)
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

    hr = StorePreeditAnchor(edit_cookie, inserted_range, preedit_anchor_range);
    if (FAILED(hr)) {
        inserted_range->Release();
        return hr;
    }

    ReleaseRange(composition_range);
    hr = CreatePreeditRangeFromAnchor(edit_cookie, *preedit_anchor_range, text.size(), composition_range);
    inserted_range->Release();
    if (FAILED(hr)) {
        ReleaseRange(preedit_anchor_range);
        return hr;
    }

    ApplyPreeditDisplayAttribute(context, edit_cookie, *composition_range);
    StartPreeditComposition(context, edit_cookie, *composition_range, tsf_composition, composition_sink);
    hr = CollapseRangeToEnd(context, edit_cookie, *composition_range);
    if (FAILED(hr)) {
        return hr;
    }

    if (preedit_text_length) {
        *preedit_text_length = text.size();
    }
    return S_OK;
}

HRESULT ReplacePreedit(
    ITfContext* context,
    TfEditCookie edit_cookie,
    ITfRange** composition_range,
    ITfRange** preedit_anchor_range,
    ITfComposition** tsf_composition,
    ITfCompositionSink* composition_sink,
    size_t* preedit_text_length,
    const std::wstring& text)
{
    if (!composition_range || !preedit_anchor_range) {
        return E_POINTER;
    }

    if (!*preedit_anchor_range || !*composition_range) {
        if (text.empty()) {
            ReleaseRange(composition_range);
            ReleaseRange(preedit_anchor_range);
            EndPreeditComposition(edit_cookie, tsf_composition);
            if (preedit_text_length) {
                *preedit_text_length = 0;
            }
            return S_OK;
        }
        return InsertNewPreedit(context, edit_cookie, composition_range, preedit_anchor_range, tsf_composition, composition_sink, preedit_text_length, text);
    }

    ITfRange* old_range = nullptr;
    const size_t old_length = preedit_text_length ? *preedit_text_length : 0;
    HRESULT hr = CreatePreeditRangeFromAnchor(edit_cookie, *preedit_anchor_range, old_length, &old_range);
    if (FAILED(hr)) {
        return hr;
    }

    ClearDisplayAttribute(context, edit_cookie, old_range);
    hr = old_range->SetText(edit_cookie, 0, text.c_str(), static_cast<LONG>(text.size()));
    old_range->Release();
    if (FAILED(hr)) {
        DebugLogHresult(L"preedit SetText failed", hr);
        return hr;
    }

    ReleaseRange(composition_range);
    if (text.empty()) {
        ReleaseRange(preedit_anchor_range);
        EndPreeditComposition(edit_cookie, tsf_composition);
        if (preedit_text_length) {
            *preedit_text_length = 0;
        }
        return S_OK;
    }

    hr = CreatePreeditRangeFromAnchor(edit_cookie, *preedit_anchor_range, text.size(), composition_range);
    if (FAILED(hr)) {
        ReleaseRange(preedit_anchor_range);
        return hr;
    }

    ApplyPreeditDisplayAttribute(context, edit_cookie, *composition_range);
    StartPreeditComposition(context, edit_cookie, *composition_range, tsf_composition, composition_sink);
    hr = CollapseRangeToEnd(context, edit_cookie, *composition_range);
    if (FAILED(hr)) {
        return hr;
    }

    if (preedit_text_length) {
        *preedit_text_length = text.size();
    }
    return S_OK;
}

HRESULT CommitText(
    ITfContext* context,
    TfEditCookie edit_cookie,
    ITfRange** composition_range,
    ITfRange** preedit_anchor_range,
    ITfComposition** tsf_composition,
    ITfCompositionSink* composition_sink,
    size_t* preedit_text_length,
    const std::wstring& commit)
{
    if (commit.empty()) {
        return ReplacePreedit(context, edit_cookie, composition_range, preedit_anchor_range, tsf_composition, composition_sink, preedit_text_length, L"");
    }

    if (preedit_anchor_range && *preedit_anchor_range) {
        ITfRange* commit_range = nullptr;
        const size_t old_length = preedit_text_length ? *preedit_text_length : 0;
        HRESULT hr = CreatePreeditRangeFromAnchor(edit_cookie, *preedit_anchor_range, old_length, &commit_range);
        if (FAILED(hr)) {
            return hr;
        }
        ClearDisplayAttribute(context, edit_cookie, commit_range);
        hr = commit_range->SetText(edit_cookie, 0, commit.c_str(), static_cast<LONG>(commit.size()));
        if (FAILED(hr)) {
            DebugLogHresult(L"commit SetText over preedit failed", hr);
            commit_range->Release();
            return hr;
        }
        hr = CollapseRangeToEnd(context, edit_cookie, commit_range);
        EndPreeditComposition(edit_cookie, tsf_composition);
        commit_range->Release();
        ReleaseRange(composition_range);
        ReleaseRange(preedit_anchor_range);
        if (preedit_text_length) {
            *preedit_text_length = 0;
        }
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
    ITfRange** preedit_anchor_range,
    ITfComposition** tsf_composition,
    ITfCompositionSink* composition_sink,
    CandidateWindow* candidate_window,
    UINT key,
    size_t* preedit_text_length)
    : ref_count_(1),
      context_(context),
      engine_(engine),
      composition_range_(composition_range),
      preedit_anchor_range_(preedit_anchor_range),
      tsf_composition_(tsf_composition),
      composition_sink_(composition_sink),
      preedit_text_length_(preedit_text_length),
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
    ITfRange** preedit_anchor_range,
    ITfComposition** tsf_composition,
    ITfCompositionSink* composition_sink,
    CandidateWindow* candidate_window,
    size_t candidate_index,
    size_t* preedit_text_length)
    : ref_count_(1),
      context_(context),
      engine_(engine),
      composition_range_(composition_range),
      preedit_anchor_range_(preedit_anchor_range),
      tsf_composition_(tsf_composition),
      composition_sink_(composition_sink),
      preedit_text_length_(preedit_text_length),
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
    ITfRange** preedit_anchor_range,
    ITfComposition** tsf_composition,
    ITfCompositionSink* composition_sink,
    CandidateWindow* candidate_window,
    size_t candidate_index,
    bool highlight_candidate,
    size_t* preedit_text_length)
    : ref_count_(1),
      context_(context),
      engine_(engine),
      composition_range_(composition_range),
      preedit_anchor_range_(preedit_anchor_range),
      tsf_composition_(tsf_composition),
      composition_sink_(composition_sink),
      preedit_text_length_(preedit_text_length),
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
    ITfRange** preedit_anchor_range,
    ITfComposition** tsf_composition,
    ITfCompositionSink* composition_sink,
    CandidateWindow* candidate_window,
    const std::wstring& commit_suffix,
    bool commit_current_candidate,
    size_t* preedit_text_length)
    : ref_count_(1),
      context_(context),
      engine_(engine),
      composition_range_(composition_range),
      preedit_anchor_range_(preedit_anchor_range),
      tsf_composition_(tsf_composition),
      composition_sink_(composition_sink),
      preedit_text_length_(preedit_text_length),
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
    ITfRange** preedit_anchor_range,
    ITfComposition** tsf_composition,
    ITfCompositionSink* composition_sink,
    CandidateWindow* candidate_window,
    const std::wstring& commit_text,
    size_t* preedit_text_length)
    : ref_count_(1),
      context_(context),
      engine_(nullptr),
      composition_range_(composition_range),
      preedit_anchor_range_(preedit_anchor_range),
      tsf_composition_(tsf_composition),
      composition_sink_(composition_sink),
      preedit_text_length_(preedit_text_length),
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
        HRESULT hr = CommitText(context_, edit_cookie, composition_range_, preedit_anchor_range_, tsf_composition_, composition_sink_, preedit_text_length_, commit_text_);
        if (candidate_window_) {
            candidate_window_->Hide();
        }
        if (FAILED(hr)) {
            DebugLog(L"edit session: direct commit failed");
        }
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
        HRESULT hr = CommitText(context_, edit_cookie, composition_range_, preedit_anchor_range_, tsf_composition_, composition_sink_, preedit_text_length_, result.commit);
        if (candidate_window_) {
            candidate_window_->Hide();
        }
        if (FAILED(hr)) {
            DebugLog(L"edit session: commit failed");
        }
        return hr;
    }

    if (result.clear_composition) {
        if (candidate_window_) {
            candidate_window_->Hide();
        }
        return ReplacePreedit(context_, edit_cookie, composition_range_, preedit_anchor_range_, tsf_composition_, composition_sink_, preedit_text_length_, L"");
    }

    if (result.composition_changed) {
        HRESULT hr = ReplacePreedit(context_, edit_cookie, composition_range_, preedit_anchor_range_, tsf_composition_, composition_sink_, preedit_text_length_, result.composition);
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
        if (FAILED(hr)) {
            DebugLog(L"edit session: preedit update failed");
        }
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
        return S_OK;
    }

    return S_OK;
}

}  // namespace arrowinput
