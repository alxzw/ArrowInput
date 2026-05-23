#pragma once

#include "Candidate.h"

#include <unordered_map>
#include <utility>

struct sqlite3;

namespace arrowinput {

struct DictionaryIssue {
    size_t line_number = 0;
    std::wstring reason;
};

struct DictionaryStats {
    std::wstring source_type;
    std::wstring status;
    bool configured = false;
    bool loaded = false;
    size_t code_count = 0;
    size_t candidate_count = 0;
    size_t line_count = 0;
    size_t skipped_line_count = 0;
    size_t invalid_line_count = 0;
    std::vector<DictionaryIssue> issues;
};

class ICandidateSource {
public:
    virtual ~ICandidateSource() = default;
    virtual void Reset(const std::wstring& path) = 0;
    virtual void ReloadIfChanged() = 0;
    virtual bool IsAvailable() const = 0;
    virtual std::vector<Candidate> QueryCandidates(const std::wstring& code, size_t max_candidates, bool prefix_candidates) const = 0;
    virtual std::vector<Candidate> QueryPinyinPrefixCandidates(const std::vector<std::wstring>& prefixes, size_t max_candidates) const = 0;
    virtual void RecordSelection(const std::wstring& code, const Candidate& candidate) = 0;
    virtual void DeleteCandidate(const std::wstring& code, const Candidate& candidate) = 0;
    virtual DictionaryStats GetStats() const = 0;
};

class TsvCandidateSource final : public ICandidateSource {
public:
    void Reset(const std::wstring& path) override;
    void ReloadIfChanged() override;
    bool IsAvailable() const override { return available_; }
    std::vector<Candidate> QueryCandidates(const std::wstring& code, size_t max_candidates, bool prefix_candidates) const override;
    std::vector<Candidate> QueryPinyinPrefixCandidates(const std::vector<std::wstring>& prefixes, size_t max_candidates) const override;
    void RecordSelection(const std::wstring& code, const Candidate& candidate) override;
    void DeleteCandidate(const std::wstring& code, const Candidate& candidate) override;
    DictionaryStats GetStats() const override;

private:
    bool ReadWriteTime(FILETIME* write_time) const;
    void Load();
    void ClearLoadedState();

    std::wstring path_;
    std::unordered_map<std::wstring, std::vector<Candidate>> dictionary_;
    FILETIME write_time_ = {};
    bool available_ = false;
    size_t line_count_ = 0;
    size_t skipped_line_count_ = 0;
    size_t invalid_line_count_ = 0;
    std::vector<DictionaryIssue> issues_;
};

class EmptyCandidateSource final : public ICandidateSource {
public:
    EmptyCandidateSource() = default;
    explicit EmptyCandidateSource(std::wstring source_type) : source_type_(std::move(source_type)) {}
    void Reset(const std::wstring& path) override { path_ = path; }
    void ReloadIfChanged() override {}
    bool IsAvailable() const override { return false; }
    std::vector<Candidate> QueryCandidates(const std::wstring& code, size_t max_candidates, bool prefix_candidates) const override;
    std::vector<Candidate> QueryPinyinPrefixCandidates(const std::vector<std::wstring>& prefixes, size_t max_candidates) const override;
    void RecordSelection(const std::wstring& code, const Candidate& candidate) override;
    void DeleteCandidate(const std::wstring& code, const Candidate& candidate) override;
    DictionaryStats GetStats() const override;

private:
    std::wstring source_type_ = L"none";
    std::wstring path_;
};

class SqliteCandidateSource final : public ICandidateSource {
public:
    ~SqliteCandidateSource() override;
    void Reset(const std::wstring& path) override;
    void ReloadIfChanged() override;
    bool IsAvailable() const override { return available_; }
    std::vector<Candidate> QueryCandidates(const std::wstring& code, size_t max_candidates, bool prefix_candidates) const override;
    std::vector<Candidate> QueryPinyinPrefixCandidates(const std::vector<std::wstring>& prefixes, size_t max_candidates) const override;
    void RecordSelection(const std::wstring& code, const Candidate& candidate) override;
    void DeleteCandidate(const std::wstring& code, const Candidate& candidate) override;
    DictionaryStats GetStats() const override;

private:
    bool ReadWriteTime(FILETIME* write_time) const;
    void Load();
    void Close();

    std::wstring path_;
    std::wstring status_;
    sqlite3* database_ = nullptr;
    FILETIME write_time_ = {};
    DWORD last_reload_check_tick_ = 0;
    bool available_ = false;
    size_t code_count_ = 0;
    size_t candidate_count_ = 0;
};

}  // namespace arrowinput
