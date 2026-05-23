#pragma once

#include "ArrowInput.h"

#include <vector>

namespace arrowinput {

struct PinyinParseResult {
    std::wstring compact_code;
    std::wstring spaced_code;
    bool complete = false;
};

PinyinParseResult ParsePinyinCode(const std::wstring& code);
std::vector<std::vector<std::wstring>> BuildPinyinPrefixPatterns(const std::wstring& code);
std::vector<std::wstring> BuildFuzzyPinyinCodes(const std::wstring& spaced_code, size_t max_results);

}  // namespace arrowinput
