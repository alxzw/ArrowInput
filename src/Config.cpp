#include "Config.h"

#include <shlwapi.h>

namespace arrowinput {

namespace {

bool ReadBool(const wchar_t* section, const wchar_t* key, bool fallback, const std::wstring& path)
{
    return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, path.c_str()) != 0;
}

int ReadClampedInt(const wchar_t* section, const wchar_t* key, int fallback, int minimum, int maximum, const std::wstring& path)
{
    int value = GetPrivateProfileIntW(section, key, fallback, path.c_str());
    if (value < minimum) {
        value = minimum;
    }
    if (value > maximum) {
        value = maximum;
    }
    return value;
}

std::wstring ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* fallback, const std::wstring& path)
{
    wchar_t value[MAX_PATH] = {};
    GetPrivateProfileStringW(section, key, fallback, value, ARRAYSIZE(value), path.c_str());
    return value;
}

std::wstring ResolvePathRelativeToConfig(const std::wstring& path, const std::wstring& config_path)
{
    if (path.empty() || PathIsRelativeW(path.c_str()) == FALSE) {
        return path;
    }

    wchar_t dir[MAX_PATH] = {};
    StringCchCopyW(dir, ARRAYSIZE(dir), config_path.c_str());
    PathRemoveFileSpecW(dir);

    wchar_t resolved[MAX_PATH] = {};
    StringCchCopyW(resolved, ARRAYSIZE(resolved), dir);
    PathAppendW(resolved, path.c_str());
    return resolved;
}

}  // namespace

std::wstring GetUserConfigPath()
{
    wchar_t app_data[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, app_data))) {
        return L".\\config.ini";
    }

    wchar_t dir[MAX_PATH] = {};
    StringCchCopyW(dir, ARRAYSIZE(dir), app_data);
    PathAppendW(dir, kConfigDirectoryName);
    CreateDirectoryW(dir, nullptr);

    wchar_t path[MAX_PATH] = {};
    StringCchCopyW(path, ARRAYSIZE(path), dir);
    PathAppendW(path, kConfigFileName);
    return path;
}

void DebugLog(const wchar_t* message)
{
    wchar_t app_data[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, app_data))) {
        return;
    }

    wchar_t dir[MAX_PATH] = {};
    StringCchCopyW(dir, ARRAYSIZE(dir), app_data);
    PathAppendW(dir, kConfigDirectoryName);
    CreateDirectoryW(dir, nullptr);

    wchar_t path[MAX_PATH] = {};
    StringCchCopyW(path, ARRAYSIZE(path), dir);
    PathAppendW(path, L"debug.log");

    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    SYSTEMTIME time = {};
    GetLocalTime(&time);
    wchar_t line[512] = {};
    StringCchPrintfW(line, ARRAYSIZE(line), L"%04u-%02u-%02u %02u:%02u:%02u %s\r\n",
                     time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, message);

    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, line, -1, nullptr, 0, nullptr, nullptr);
    if (utf8_len > 1) {
        std::string utf8(static_cast<size_t>(utf8_len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8.data(), utf8_len, nullptr, nullptr);
        DWORD bytes = 0;
        WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &bytes, nullptr);
    }
    CloseHandle(file);
}

void DebugLogHresult(const wchar_t* message, HRESULT hr)
{
    wchar_t line[256] = {};
    StringCchPrintfW(line, ARRAYSIZE(line), L"%s hr=0x%08X", message, static_cast<unsigned int>(hr));
    DebugLog(line);
}

HRESULT EnsureUserConfig()
{
    const std::wstring path = GetUserConfigPath();
    if (!PathFileExistsW(path.c_str())) {
        WritePrivateProfileStringW(L"general", L"mode", L"wubi", path.c_str());
        WritePrivateProfileStringW(L"general", L"candidate_page_size", L"5", path.c_str());
        WritePrivateProfileStringW(L"general", L"max_candidates_per_query", L"30", path.c_str());
        WritePrivateProfileStringW(L"general", L"prefix_candidates", L"1", path.c_str());
        WritePrivateProfileStringW(L"general", L"fuzzy_pinyin", L"0", path.c_str());
        WritePrivateProfileStringW(L"general", L"chinese_punctuation", L"1", path.c_str());
        WritePrivateProfileStringW(L"general", L"full_shape", L"0", path.c_str());
        WritePrivateProfileStringW(L"general", L"dictionary_type", L"tsv", path.c_str());
        WritePrivateProfileStringW(L"general", L"dictionary_path", L"", path.c_str());
        WritePrivateProfileStringW(L"debug", L"capture_keys", L"0", path.c_str());
        WritePrivateProfileStringW(L"debug", L"preedit_markers", L"1", path.c_str());
        return S_OK;
    }

    wchar_t probe[16] = {};
    if (!GetPrivateProfileStringW(L"general", L"mode", nullptr, probe, ARRAYSIZE(probe), path.c_str())) {
        WritePrivateProfileStringW(L"general", L"mode", L"wubi", path.c_str());
    }
    if (!GetPrivateProfileStringW(L"general", L"candidate_page_size", nullptr, probe, ARRAYSIZE(probe), path.c_str())) {
        WritePrivateProfileStringW(L"general", L"candidate_page_size", L"5", path.c_str());
    }
    if (!GetPrivateProfileStringW(L"general", L"max_candidates_per_query", nullptr, probe, ARRAYSIZE(probe), path.c_str())) {
        WritePrivateProfileStringW(L"general", L"max_candidates_per_query", L"30", path.c_str());
    }
    if (!GetPrivateProfileStringW(L"general", L"prefix_candidates", nullptr, probe, ARRAYSIZE(probe), path.c_str())) {
        WritePrivateProfileStringW(L"general", L"prefix_candidates", L"1", path.c_str());
    }
    if (!GetPrivateProfileStringW(L"general", L"fuzzy_pinyin", nullptr, probe, ARRAYSIZE(probe), path.c_str())) {
        WritePrivateProfileStringW(L"general", L"fuzzy_pinyin", L"0", path.c_str());
    }
    if (!GetPrivateProfileStringW(L"general", L"chinese_punctuation", nullptr, probe, ARRAYSIZE(probe), path.c_str())) {
        WritePrivateProfileStringW(L"general", L"chinese_punctuation", L"1", path.c_str());
    }
    if (!GetPrivateProfileStringW(L"general", L"full_shape", nullptr, probe, ARRAYSIZE(probe), path.c_str())) {
        WritePrivateProfileStringW(L"general", L"full_shape", L"0", path.c_str());
    }
    if (!GetPrivateProfileStringW(L"general", L"dictionary_type", nullptr, probe, ARRAYSIZE(probe), path.c_str())) {
        WritePrivateProfileStringW(L"general", L"dictionary_type", L"tsv", path.c_str());
    }
    if (!GetPrivateProfileStringW(L"general", L"dictionary_path", nullptr, probe, ARRAYSIZE(probe), path.c_str())) {
        WritePrivateProfileStringW(L"general", L"dictionary_path", L"", path.c_str());
    }
    if (!GetPrivateProfileStringW(L"debug", L"capture_keys", nullptr, probe, ARRAYSIZE(probe), path.c_str())) {
        WritePrivateProfileStringW(L"debug", L"capture_keys", L"0", path.c_str());
    }
    if (!GetPrivateProfileStringW(L"debug", L"preedit_markers", nullptr, probe, ARRAYSIZE(probe), path.c_str())) {
        WritePrivateProfileStringW(L"debug", L"preedit_markers", L"1", path.c_str());
    }
    return S_OK;
}

Config LoadConfig()
{
    EnsureUserConfig();

    Config config;
    config.config_path = GetUserConfigPath();
    config.mode = ReadString(L"general", L"mode", L"wubi", config.config_path);
    config.candidate_page_size = ReadClampedInt(L"general", L"candidate_page_size", 5, 1, 9, config.config_path);
    config.max_candidates_per_query = ReadClampedInt(L"general", L"max_candidates_per_query", 30, 1, 1000, config.config_path);
    config.prefix_candidates = ReadBool(L"general", L"prefix_candidates", true, config.config_path);
    config.fuzzy_pinyin = ReadBool(L"general", L"fuzzy_pinyin", false, config.config_path);
    config.chinese_punctuation = ReadBool(L"general", L"chinese_punctuation", true, config.config_path);
    config.full_shape = ReadBool(L"general", L"full_shape", false, config.config_path);
    config.dictionary_type = ReadString(L"general", L"dictionary_type", L"tsv", config.config_path);
    config.dictionary_path = ReadString(L"general", L"dictionary_path", L"", config.config_path);
    config.dictionary_path = ResolvePathRelativeToConfig(config.dictionary_path, config.config_path);
    config.capture_keys = ReadBool(L"debug", L"capture_keys", false, config.config_path);
    config.preedit_markers = ReadBool(L"debug", L"preedit_markers", true, config.config_path);
    DebugLog(config.capture_keys ? L"config loaded: capture_keys=1" : L"config loaded: capture_keys=0");
    return config;
}

}  // namespace arrowinput
