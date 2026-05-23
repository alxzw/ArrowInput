#pragma once

#include "CandidateSource.h"
#include "Config.h"

#include <memory>

namespace arrowinput {

class IInputAlgorithm {
public:
    virtual ~IInputAlgorithm() = default;
    virtual void Reset(const Config& config) = 0;
    virtual std::vector<Candidate> QueryCandidates(const std::wstring& code) = 0;
    virtual void RecordSelection(const std::wstring& code, const Candidate& candidate) = 0;
    virtual void DeleteCandidate(const std::wstring& code, const Candidate& candidate) = 0;
    virtual DictionaryStats GetDictionaryStats() = 0;
};

class DemoInputAlgorithm final : public IInputAlgorithm {
public:
    DemoInputAlgorithm();
    void Reset(const Config& config) override;
    std::vector<Candidate> QueryCandidates(const std::wstring& code) override;
    void RecordSelection(const std::wstring& code, const Candidate& candidate) override;
    void DeleteCandidate(const std::wstring& code, const Candidate& candidate) override;
    DictionaryStats GetDictionaryStats() override;

private:
    std::vector<Candidate> QueryDemoCandidates(const std::wstring& code) const;

    Config config_;
    std::unique_ptr<ICandidateSource> source_;
};

}  // namespace arrowinput
