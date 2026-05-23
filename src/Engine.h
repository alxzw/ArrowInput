#pragma once

#include "ArrowInput.h"
#include "Config.h"
#include "InputAlgorithm.h"

#include <memory>

namespace arrowinput {

struct KeyResult {
    std::wstring composition;
    std::wstring commit;
    std::vector<Candidate> candidates;
    size_t selected_candidate = 0;
    size_t page_start = 0;
    size_t page_size = 5;
    size_t total_candidates = 0;
    bool composition_changed = false;
    bool clear_composition = false;
    bool candidates_changed = false;
};

class Engine {
public:
    Engine();
    void Reset(const Config& config);
    KeyResult HandleVirtualKey(UINT key);
    KeyResult HighlightCandidate(size_t index);
    KeyResult SelectCandidate(size_t index);
    KeyResult DeleteCurrentCandidate();
    KeyResult CommitCurrentCandidateWithSuffix(const std::wstring& suffix);
    bool HasComposition() const { return !composition_.empty(); }
    DictionaryStats GetDictionaryStats() const;

    const std::wstring& composition() const { return composition_; }
    const std::vector<Candidate>& candidates() const { return candidates_; }

private:
    std::wstring FormatCompositionForDisplay() const;
    std::wstring CommitCandidate(size_t index);
    void RefreshCandidates();
    void FillCandidatePage(KeyResult* result) const;
    size_t CandidatePageSize() const;

    Config config_;
    std::unique_ptr<IInputAlgorithm> algorithm_;
    std::wstring composition_;
    std::vector<Candidate> candidates_;
    size_t selected_candidate_ = 0;
    size_t page_start_ = 0;
};

}  // namespace arrowinput
