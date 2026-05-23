#include "../../src/InputAlgorithm.h"

#include <iostream>
#include <sstream>

namespace {

void WriteText(const std::wstring& text)
{
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output && output != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(output, &mode)) {
            DWORD written = 0;
            WriteConsoleW(output, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
            return;
        }
    }

    const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (utf8_length <= 0) {
        return;
    }
    std::string utf8(static_cast<size_t>(utf8_length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), utf8_length, nullptr, nullptr);
    DWORD written = 0;
    WriteFile(output, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
}

void PrintUsage()
{
    WriteText(L"Usage: AlgorithmProbe.exe [--type tsv] [--dict path] [--limit n] [--no-prefix] [--fuzzy-pinyin] [--stats] [--interactive] [--select code index] [--delete code index] <code> [code...]\n");
    WriteText(L"Example: AlgorithmProbe.exe --type tsv --dict tools\\AlgorithmProbe\\sample_dictionary.tsv ni\n");
    WriteText(L"Example: AlgorithmProbe.exe --dict tools\\AlgorithmProbe\\sample_dictionary.tsv --stats\n");
    WriteText(L"Example: AlgorithmProbe.exe --dict tools\\AlgorithmProbe\\sample_dictionary.tsv --interactive\n");
}

void SelectCandidate(arrowinput::IInputAlgorithm* algorithm, const std::wstring& code, size_t index)
{
    if (!algorithm || index == 0) {
        return;
    }

    std::vector<arrowinput::Candidate> candidates = algorithm->QueryCandidates(code);
    if (index > candidates.size()) {
        std::wstringstream line;
        line << L"selection failed: [" << code << L"] has " << candidates.size() << L" candidate(s)\n";
        WriteText(line.str());
        return;
    }

    algorithm->RecordSelection(code, candidates[index - 1]);
    std::wstringstream line;
    line << L"selected [" << code << L"] " << index << L". " << candidates[index - 1].text << L"\n";
    WriteText(line.str());
}

void DeleteCandidate(arrowinput::IInputAlgorithm* algorithm, const std::wstring& code, size_t index)
{
    if (!algorithm || index == 0) {
        return;
    }

    std::vector<arrowinput::Candidate> candidates = algorithm->QueryCandidates(code);
    if (index > candidates.size()) {
        std::wstringstream line;
        line << L"delete failed: [" << code << L"] has " << candidates.size() << L" candidate(s)\n";
        WriteText(line.str());
        return;
    }

    algorithm->DeleteCandidate(code, candidates[index - 1]);
    std::wstringstream line;
    line << L"deleted [" << code << L"] " << index << L". " << candidates[index - 1].text << L"\n";
    WriteText(line.str());
}

void PrintCandidates(arrowinput::IInputAlgorithm* algorithm, const std::wstring& code)
{
    if (!algorithm) {
        return;
    }

    const std::vector<arrowinput::Candidate> candidates = algorithm->QueryCandidates(code);

    std::wstringstream header;
    header << L"[" << code << L"] " << candidates.size() << L" candidate(s)\n";
    WriteText(header.str());
    for (size_t i = 0; i < candidates.size(); ++i) {
        std::wstringstream line;
        line << i + 1 << L". " << candidates[i].text;
        if (!candidates[i].comment.empty()) {
            line << L" " << candidates[i].comment;
        }
        line << L"\n";
        WriteText(line.str());
    }
}

void PrintStats(arrowinput::DemoInputAlgorithm* algorithm)
{
    if (!algorithm) {
        return;
    }

    const arrowinput::DictionaryStats stats = algorithm->GetDictionaryStats();
    std::wstringstream line;
    line << L"source type: " << stats.source_type << L"\n";
    line << L"source status: " << stats.status << L"\n";
    line << L"dictionary configured: " << (stats.configured ? L"yes" : L"no") << L"\n";
    line << L"dictionary loaded: " << (stats.loaded ? L"yes" : L"no") << L"\n";
    line << L"data line count: " << stats.line_count << L"\n";
    line << L"skipped line count: " << stats.skipped_line_count << L"\n";
    line << L"invalid line count: " << stats.invalid_line_count << L"\n";
    line << L"code count: " << stats.code_count << L"\n";
    line << L"candidate count: " << stats.candidate_count << L"\n";
    if (!stats.issues.empty()) {
        line << L"issues:\n";
        for (const arrowinput::DictionaryIssue& issue : stats.issues) {
            line << L"  line " << issue.line_number << L": " << issue.reason << L"\n";
        }
    }
    WriteText(line.str());
}

std::wstring TrimInput(std::wstring text)
{
    if (!text.empty() && text.front() == 0xFEFF) {
        text.erase(text.begin());
    }
    if (text.size() >= 3 && text[0] == 0x00EF && text[1] == 0x00BB && text[2] == 0x00BF) {
        text.erase(0, 3);
    }
    while (!text.empty() && (text.front() == L' ' || text.front() == L'\t')) {
        text.erase(text.begin());
    }
    while (!text.empty() && (text.back() == L' ' || text.back() == L'\t' || text.back() == L'\r')) {
        text.pop_back();
    }
    return text;
}

}  // namespace

int wmain(int argc, wchar_t** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        PrintUsage();
        return 0;
    }

    arrowinput::Config config;
    std::vector<std::wstring> codes;
    struct Selection {
        std::wstring code;
        size_t index = 0;
    };
    std::vector<Selection> selections;
    std::vector<Selection> deletions;
    bool interactive = false;
    bool print_stats = false;
    for (int arg = 1; arg < argc; ++arg) {
        if (wcscmp(argv[arg], L"--type") == 0) {
            if (arg + 1 >= argc) {
                PrintUsage();
                return 1;
            }
            config.dictionary_type = argv[++arg];
        } else if (wcscmp(argv[arg], L"--dict") == 0) {
            if (arg + 1 >= argc) {
                PrintUsage();
                return 1;
            }
            config.dictionary_path = argv[++arg];
        } else if (wcscmp(argv[arg], L"--limit") == 0) {
            if (arg + 1 >= argc) {
                PrintUsage();
                return 1;
            }
            wchar_t* end = nullptr;
            const long parsed = wcstol(argv[++arg], &end, 10);
            if (end == argv[arg] || *end != L'\0' || parsed < 1 || parsed > 1000) {
                PrintUsage();
                return 1;
            }
            config.max_candidates_per_query = static_cast<int>(parsed);
        } else if (wcscmp(argv[arg], L"--stats") == 0) {
            print_stats = true;
        } else if (wcscmp(argv[arg], L"--no-prefix") == 0) {
            config.prefix_candidates = false;
        } else if (wcscmp(argv[arg], L"--prefix") == 0) {
            config.prefix_candidates = true;
        } else if (wcscmp(argv[arg], L"--fuzzy-pinyin") == 0) {
            config.fuzzy_pinyin = true;
        } else if (wcscmp(argv[arg], L"--no-fuzzy-pinyin") == 0) {
            config.fuzzy_pinyin = false;
        } else if (wcscmp(argv[arg], L"--interactive") == 0 || wcscmp(argv[arg], L"-i") == 0) {
            interactive = true;
        } else if (wcscmp(argv[arg], L"--select") == 0) {
            if (arg + 2 >= argc) {
                PrintUsage();
                return 1;
            }
            Selection selection;
            selection.code = argv[++arg];
            wchar_t* end = nullptr;
            const long parsed = wcstol(argv[++arg], &end, 10);
            if (end == argv[arg] || *end != L'\0' || parsed < 1 || parsed > 1000) {
                PrintUsage();
                return 1;
            }
            selection.index = static_cast<size_t>(parsed);
            selections.push_back(selection);
        } else if (wcscmp(argv[arg], L"--delete") == 0) {
            if (arg + 2 >= argc) {
                PrintUsage();
                return 1;
            }
            Selection deletion;
            deletion.code = argv[++arg];
            wchar_t* end = nullptr;
            const long parsed = wcstol(argv[++arg], &end, 10);
            if (end == argv[arg] || *end != L'\0' || parsed < 1 || parsed > 1000) {
                PrintUsage();
                return 1;
            }
            deletion.index = static_cast<size_t>(parsed);
            deletions.push_back(deletion);
        } else {
            codes.push_back(argv[arg]);
        }
    }

    arrowinput::DemoInputAlgorithm algorithm;
    algorithm.Reset(config);

    if (codes.empty() && selections.empty() && deletions.empty() && !interactive && !print_stats) {
        PrintUsage();
        return 0;
    }

    if (print_stats) {
        PrintStats(&algorithm);
    }

    for (const Selection& selection : selections) {
        SelectCandidate(&algorithm, selection.code, selection.index);
    }

    for (const Selection& deletion : deletions) {
        DeleteCandidate(&algorithm, deletion.code, deletion.index);
    }

    for (const std::wstring& code : codes) {
        PrintCandidates(&algorithm, code);
    }

    if (interactive) {
        WriteText(L"Interactive mode. Type a code and press Enter; empty line or Ctrl+Z exits.\n");
        std::wstring line;
        while (true) {
            WriteText(L"> ");
            if (!std::getline(std::wcin, line) || line.empty()) {
                break;
            }
            line = TrimInput(line);
            if (line.empty()) {
                break;
            }
            PrintCandidates(&algorithm, line);
        }
    }

    return 0;
}
