#include "InputAlgorithm.h"
#include "PinyinParser.h"

#include <unordered_set>

namespace arrowinput {

namespace {

bool EqualsIgnoreCase(const std::wstring& left, const std::wstring& right)
{
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
}

std::unique_ptr<ICandidateSource> CreateCandidateSource(const std::wstring& type)
{
    if (type.empty() || EqualsIgnoreCase(type, L"tsv")) {
        return std::make_unique<TsvCandidateSource>();
    }
    if (EqualsIgnoreCase(type, L"sqlite")) {
        return std::make_unique<SqliteCandidateSource>();
    }
    return std::make_unique<EmptyCandidateSource>(type);
}

bool IsAsciiLowerCode(const std::wstring& code)
{
    if (code.empty()) {
        return false;
    }
    for (wchar_t ch : code) {
        if (ch < L'a' || ch > L'z') {
            return false;
        }
    }
    return true;
}

void AppendUniqueCandidates(
    std::vector<Candidate>* target,
    std::vector<Candidate> source,
    size_t max_candidates)
{
    if (!target) {
        return;
    }

    std::unordered_set<std::wstring> seen;
    for (const Candidate& candidate : *target) {
        seen.insert(candidate.text + L"\t" + candidate.comment);
    }
    for (Candidate& candidate : source) {
        if (max_candidates != 0 && target->size() >= max_candidates) {
            break;
        }
        if (seen.insert(candidate.text + L"\t" + candidate.comment).second) {
            target->push_back(std::move(candidate));
        }
    }
}

std::wstring RemoveSpaces(const std::wstring& text)
{
    std::wstring compact;
    compact.reserve(text.size());
    for (wchar_t ch : text) {
        if (ch != L' ') {
            compact.push_back(ch);
        }
    }
    return compact;
}

}  // namespace

DemoInputAlgorithm::DemoInputAlgorithm()
{
}

void DemoInputAlgorithm::Reset(const Config& config)
{
    config_ = config;
    source_ = CreateCandidateSource(config_.dictionary_type);
    if (source_) {
        source_->Reset(config_.dictionary_path);
    }
}

std::vector<Candidate> DemoInputAlgorithm::QueryCandidates(const std::wstring& code)
{
    if (source_) {
        source_->ReloadIfChanged();
        if (source_->IsAvailable()) {
            const size_t max_candidates = static_cast<size_t>(config_.max_candidates_per_query);
            std::vector<Candidate> candidates = source_->QueryCandidates(
                code,
                max_candidates,
                config_.prefix_candidates);
            PinyinParseResult parsed;
            bool parsed_checked = false;
            if (candidates.size() < max_candidates) {
                parsed = ParsePinyinCode(code);
                parsed_checked = true;
                if (parsed.complete && !parsed.spaced_code.empty() && parsed.spaced_code != code) {
                    std::vector<Candidate> parsed_candidates = source_->QueryCandidates(
                        parsed.spaced_code,
                        max_candidates,
                        config_.prefix_candidates);
                    for (Candidate& candidate : parsed_candidates) {
                        if (candidate.comment.empty()) {
                            candidate.comment = parsed.spaced_code;
                        }
                    }
                    AppendUniqueCandidates(&candidates, std::move(parsed_candidates), max_candidates);
                }
                if (config_.fuzzy_pinyin && parsed.complete && !parsed.spaced_code.empty()) {
                    const std::vector<std::wstring> fuzzy_codes = BuildFuzzyPinyinCodes(parsed.spaced_code, 16);
                    for (const std::wstring& fuzzy_code : fuzzy_codes) {
                        if (candidates.size() >= max_candidates) {
                            break;
                        }
                        std::vector<Candidate> fuzzy_candidates = source_->QueryCandidates(
                            fuzzy_code,
                            max_candidates,
                            config_.prefix_candidates);
                        const std::wstring compact_fuzzy_code = RemoveSpaces(fuzzy_code);
                        if (compact_fuzzy_code != fuzzy_code) {
                            AppendUniqueCandidates(
                                &fuzzy_candidates,
                                source_->QueryCandidates(compact_fuzzy_code, max_candidates, config_.prefix_candidates),
                                max_candidates);
                        }
                        for (Candidate& candidate : fuzzy_candidates) {
                            if (candidate.comment.empty()) {
                                candidate.comment = fuzzy_code;
                            }
                        }
                        AppendUniqueCandidates(&candidates, std::move(fuzzy_candidates), max_candidates);
                    }
                }
            }
            if (!parsed_checked) {
                parsed = ParsePinyinCode(code);
                parsed_checked = true;
            }
            if (config_.prefix_candidates && !parsed.complete && candidates.size() < max_candidates && IsAsciiLowerCode(code) && code.size() >= 2 && code.size() <= 8) {
                const std::vector<std::vector<std::wstring>> patterns = BuildPinyinPrefixPatterns(code);
                for (const std::vector<std::wstring>& prefixes : patterns) {
                    if (candidates.size() >= max_candidates) {
                        break;
                    }
                    AppendUniqueCandidates(
                        &candidates,
                        source_->QueryPinyinPrefixCandidates(prefixes, max_candidates),
                        max_candidates);
                }
            }
            return candidates;
        }
        if (source_->GetStats().configured) {
            return std::vector<Candidate>();
        }
    }
    return QueryDemoCandidates(code);
}

DictionaryStats DemoInputAlgorithm::GetDictionaryStats()
{
    if (!source_) {
        return DictionaryStats();
    }

    source_->ReloadIfChanged();
    return source_->GetStats();
}

void DemoInputAlgorithm::RecordSelection(const std::wstring& code, const Candidate& candidate)
{
    if (!source_) {
        return;
    }
    source_->ReloadIfChanged();
    if (source_->IsAvailable()) {
        source_->RecordSelection(code, candidate);
    }
}

void DemoInputAlgorithm::DeleteCandidate(const std::wstring& code, const Candidate& candidate)
{
    if (!source_) {
        return;
    }
    source_->ReloadIfChanged();
    if (source_->IsAvailable()) {
        source_->DeleteCandidate(code, candidate);
    }
}

std::vector<Candidate> DemoInputAlgorithm::QueryDemoCandidates(const std::wstring& code) const
{
    std::vector<Candidate> candidates;
    if (code == L"ni") {
        candidates.push_back({L"ni", L"\x4F60", L"ni"});
        candidates.push_back({L"ni", L"\x5462", L"ne"});
        candidates.push_back({L"ni", L"\x5C3C", L"ni"});
        candidates.push_back({L"ni", L"\x59B3", L"ni"});
        candidates.push_back({L"ni", L"\x62DF", L"ni"});
        candidates.push_back({L"ni", L"\x502A", L"ni"});
        candidates.push_back({L"ni", L"\x6CE5", L"ni"});
    }
    return candidates;
}

}  // namespace arrowinput
