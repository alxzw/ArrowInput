#pragma once

#include "ArrowInput.h"
#include "CandidateWindow.h"
#include "Engine.h"

namespace arrowinput {

class KeyEditSession final : public ITfEditSession {
public:
    KeyEditSession(ITfContext* context, Engine* engine, ITfRange** composition_range, ITfRange** preedit_anchor_range, ITfComposition** tsf_composition, ITfCompositionSink* composition_sink, CandidateWindow* candidate_window, UINT key, size_t* preedit_text_length = nullptr);
    KeyEditSession(ITfContext* context, Engine* engine, ITfRange** composition_range, ITfRange** preedit_anchor_range, ITfComposition** tsf_composition, ITfCompositionSink* composition_sink, CandidateWindow* candidate_window, size_t candidate_index, size_t* preedit_text_length = nullptr);
    KeyEditSession(
        ITfContext* context,
        Engine* engine,
        ITfRange** composition_range,
        ITfRange** preedit_anchor_range,
        ITfComposition** tsf_composition,
        ITfCompositionSink* composition_sink,
        CandidateWindow* candidate_window,
        size_t candidate_index,
        bool highlight_candidate,
        size_t* preedit_text_length = nullptr);
    KeyEditSession(
        ITfContext* context,
        Engine* engine,
        ITfRange** composition_range,
        ITfRange** preedit_anchor_range,
        ITfComposition** tsf_composition,
        ITfCompositionSink* composition_sink,
        CandidateWindow* candidate_window,
        const std::wstring& commit_suffix,
        bool commit_current_candidate,
        size_t* preedit_text_length = nullptr);
    KeyEditSession(ITfContext* context, ITfRange** composition_range, ITfRange** preedit_anchor_range, ITfComposition** tsf_composition, ITfCompositionSink* composition_sink, CandidateWindow* candidate_window, const std::wstring& commit_text, size_t* preedit_text_length = nullptr);
    ~KeyEditSession();

    IFACEMETHODIMP QueryInterface(REFIID iid, void** result) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    IFACEMETHODIMP DoEditSession(TfEditCookie edit_cookie) override;

private:
    std::atomic<ULONG> ref_count_;
    ITfContext* context_;
    Engine* engine_;
    ITfRange** composition_range_;
    ITfRange** preedit_anchor_range_;
    ITfComposition** tsf_composition_;
    ITfCompositionSink* composition_sink_;
    size_t* preedit_text_length_;
    CandidateWindow* candidate_window_;
    UINT key_;
    bool select_candidate_;
    bool direct_commit_;
    bool commit_current_candidate_;
    bool highlight_candidate_;
    size_t candidate_index_;
    std::wstring commit_text_;
};

}  // namespace arrowinput
