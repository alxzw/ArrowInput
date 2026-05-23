#pragma once

#include "ArrowInput.h"

namespace arrowinput {

struct Config {
    std::wstring mode = L"wubi";
    int candidate_page_size = 5;
    int max_candidates_per_query = 30;
    bool prefix_candidates = true;
    bool fuzzy_pinyin = false;
    bool chinese_punctuation = true;
    bool full_shape = false;
    std::wstring dictionary_type = L"tsv";
    std::wstring dictionary_path;
    bool capture_keys = false;
    bool preedit_markers = true;
    std::wstring config_path;
};

Config LoadConfig();
HRESULT EnsureUserConfig();
std::wstring GetUserConfigPath();
void DebugLog(const wchar_t* message);
void DebugLogHresult(const wchar_t* message, HRESULT hr);

}  // namespace arrowinput
