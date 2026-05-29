#include "Engine.h"

namespace arrowinput {

Engine::Engine()
    : algorithm_(std::make_unique<DemoInputAlgorithm>())
{
}

void Engine::Reset(const Config& config)
{
    config_ = config;
    if (algorithm_) {
        algorithm_->Reset(config);
    }
    composition_.clear();
    candidates_.clear();
    selected_candidate_ = 0;
    page_start_ = 0;
}

DictionaryStats Engine::GetDictionaryStats() const
{
    return algorithm_ ? algorithm_->GetDictionaryStats() : DictionaryStats();
}

std::wstring Engine::FormatCompositionForDisplay() const
{
    return composition_;
}

std::wstring Engine::CommitCandidate(size_t index)
{
    RefreshCandidates();
    std::wstring commit;
    if (index < candidates_.size()) {
        commit = candidates_[index].text;
        if (algorithm_) {
            algorithm_->RecordSelection(composition_, candidates_[index]);
        }
    } else if (index == 0) {
        commit = composition_;
    }
    composition_.clear();
    candidates_.clear();
    selected_candidate_ = 0;
    page_start_ = 0;
    return commit;
}

size_t Engine::CandidatePageSize() const
{
    return config_.candidate_page_size > 0 ? static_cast<size_t>(config_.candidate_page_size) : 5;
}

void Engine::FillCandidatePage(KeyResult* result) const
{
    if (!result) {
        return;
    }

    const size_t page_size = CandidatePageSize();
    const size_t total = candidates_.size();
    const size_t start = total == 0 ? 0 : (page_start_ / page_size) * page_size;
    const size_t end = min(total, start + page_size);

    result->candidates.clear();
    for (size_t i = start; i < end; ++i) {
        result->candidates.push_back(candidates_[i]);
    }
    result->page_start = start;
    result->page_size = page_size;
    result->total_candidates = total;
    result->selected_candidate =
        selected_candidate_ >= start && selected_candidate_ < end ? selected_candidate_ - start : 0;
}

KeyResult Engine::HandleVirtualKey(UINT key)
{
    KeyResult result;

    if (!composition_.empty() && key >= L'1' && key <= L'9') {
        RefreshCandidates();
        const size_t index = page_start_ + static_cast<size_t>(key - L'1');
        if (index < candidates_.size()) {
            result.commit = CommitCandidate(index);
            result.clear_composition = true;
            result.candidates_changed = true;
            return result;
        }
        result.composition = FormatCompositionForDisplay();
        FillCandidatePage(&result);
        result.candidates_changed = true;
        return result;
    }

    if (!composition_.empty() && (key == VK_TAB || key == VK_RIGHT || key == VK_DOWN)) {
        RefreshCandidates();
        if (!candidates_.empty()) {
            selected_candidate_ = (selected_candidate_ + 1) % candidates_.size();
            page_start_ = (selected_candidate_ / CandidatePageSize()) * CandidatePageSize();
        }
        result.composition = FormatCompositionForDisplay();
        FillCandidatePage(&result);
        result.candidates_changed = true;
        return result;
    }

    if (!composition_.empty() && (key == VK_LEFT || key == VK_UP)) {
        RefreshCandidates();
        if (!candidates_.empty()) {
            selected_candidate_ = selected_candidate_ == 0 ? candidates_.size() - 1 : selected_candidate_ - 1;
            page_start_ = (selected_candidate_ / CandidatePageSize()) * CandidatePageSize();
        }
        result.composition = FormatCompositionForDisplay();
        FillCandidatePage(&result);
        result.candidates_changed = true;
        return result;
    }

    if (!composition_.empty() && (key == VK_HOME || key == VK_END)) {
        RefreshCandidates();
        if (!candidates_.empty()) {
            selected_candidate_ = key == VK_HOME ? 0 : candidates_.size() - 1;
            page_start_ = (selected_candidate_ / CandidatePageSize()) * CandidatePageSize();
        }
        result.composition = FormatCompositionForDisplay();
        FillCandidatePage(&result);
        result.candidates_changed = true;
        return result;
    }

    if (!composition_.empty() && (key == VK_NEXT || key == VK_PRIOR || key == VK_OEM_PLUS || key == VK_OEM_MINUS)) {
        RefreshCandidates();
        const size_t page_size = CandidatePageSize();
        if (!candidates_.empty()) {
            if (key == VK_NEXT || key == VK_OEM_PLUS) {
                page_start_ += page_size;
                if (page_start_ >= candidates_.size()) {
                    page_start_ = 0;
                }
            } else {
                if (page_start_ == 0) {
                    page_start_ = ((candidates_.size() - 1) / page_size) * page_size;
                } else {
                    page_start_ -= min(page_start_, page_size);
                }
            }
            selected_candidate_ = page_start_;
        }
        result.composition = FormatCompositionForDisplay();
        FillCandidatePage(&result);
        result.candidates_changed = true;
        return result;
    }

    if (!composition_.empty() && key == VK_DELETE) {
        return DeleteCurrentCandidate();
    }

    if (key >= L'A' && key <= L'Z') {
        composition_.push_back(static_cast<wchar_t>(towlower(static_cast<wint_t>(key))));
        result.composition_changed = true;
        selected_candidate_ = 0;
        page_start_ = 0;
    } else if (key == VK_BACK) {
        if (!composition_.empty()) {
            composition_.pop_back();
            result.composition_changed = true;
            result.clear_composition = composition_.empty();
            selected_candidate_ = 0;
            page_start_ = 0;
        }
    } else if (key == VK_ESCAPE) {
        composition_.clear();
        candidates_.clear();
        selected_candidate_ = 0;
        page_start_ = 0;
        result.clear_composition = true;
        result.candidates_changed = true;
    } else if (key == VK_SPACE) {
        result.commit = CommitCandidate(selected_candidate_);
        result.clear_composition = true;
        result.candidates_changed = true;
        return result;
    } else if (key == VK_RETURN) {
        result.commit = composition_;
        composition_.clear();
        candidates_.clear();
        selected_candidate_ = 0;
        page_start_ = 0;
        result.clear_composition = true;
        result.candidates_changed = true;
        return result;
    }

    RefreshCandidates();
    result.composition = FormatCompositionForDisplay();
    FillCandidatePage(&result);
    result.candidates_changed = true;
    return result;
}

KeyResult Engine::SelectCandidate(size_t index)
{
    KeyResult result;
    if (composition_.empty()) {
        return result;
    }
    RefreshCandidates();
    const size_t absolute_index = page_start_ + index;
    if (absolute_index >= candidates_.size()) {
        return result;
    }

    result.commit = CommitCandidate(absolute_index);
    result.clear_composition = true;
    result.candidates_changed = true;
    return result;
}

KeyResult Engine::HighlightCandidate(size_t index)
{
    KeyResult result;
    if (composition_.empty()) {
        return result;
    }

    RefreshCandidates();
    const size_t absolute_index = page_start_ + index;
    if (absolute_index >= candidates_.size()) {
        return result;
    }

    selected_candidate_ = absolute_index;
    page_start_ = (selected_candidate_ / CandidatePageSize()) * CandidatePageSize();
    result.composition = FormatCompositionForDisplay();
    FillCandidatePage(&result);
    result.candidates_changed = true;
    return result;
}

KeyResult Engine::DeleteCurrentCandidate()
{
    KeyResult result;
    if (composition_.empty()) {
        return result;
    }

    RefreshCandidates();
    if (selected_candidate_ >= candidates_.size()) {
        result.composition = FormatCompositionForDisplay();
        FillCandidatePage(&result);
        result.candidates_changed = true;
        return result;
    }

    if (algorithm_) {
        algorithm_->DeleteCandidate(composition_, candidates_[selected_candidate_]);
    }

    RefreshCandidates();
    if (selected_candidate_ >= candidates_.size()) {
        selected_candidate_ = candidates_.empty() ? 0 : candidates_.size() - 1;
    }
    page_start_ = candidates_.empty() ? 0 : (selected_candidate_ / CandidatePageSize()) * CandidatePageSize();
    result.composition = FormatCompositionForDisplay();
    FillCandidatePage(&result);
    result.candidates_changed = true;
    return result;
}

KeyResult Engine::CommitCurrentCandidateWithSuffix(const std::wstring& suffix)
{
    KeyResult result;
    if (composition_.empty()) {
        return result;
    }

    result.commit = CommitCandidate(selected_candidate_) + suffix;
    result.clear_composition = true;
    result.candidates_changed = true;
    return result;
}

void Engine::RefreshCandidates()
{
    candidates_.clear();
    if (composition_.empty()) {
        return;
    }

    if (algorithm_) {
        candidates_ = algorithm_->QueryCandidates(composition_);
        if (selected_candidate_ >= candidates_.size()) {
            selected_candidate_ = 0;
        }
        page_start_ = (selected_candidate_ / CandidatePageSize()) * CandidatePageSize();
        return;
    }
    selected_candidate_ = 0;
    page_start_ = 0;
}

}  // namespace arrowinput
