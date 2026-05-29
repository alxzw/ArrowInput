#include "CandidateSource.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cwchar>
#include <sqlite3.h>

namespace arrowinput {

namespace {

struct RankedCandidate {
    Candidate candidate;
    bool exact = false;
    std::wstring source_code;
    size_t source_index = 0;
};

void SourceLog(const wchar_t* message)
{
    OutputDebugStringW(L"ArrowInput candidate source: ");
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty()) {
        return L"";
    }

    int wide_length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (wide_length <= 0) {
        return L"";
    }
    std::wstring wide(static_cast<size_t>(wide_length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), wide_length);
    return wide;
}

std::vector<std::wstring> SplitLine(const std::wstring& line)
{
    std::vector<std::wstring> fields;
    size_t start = 0;
    while (start <= line.size()) {
        const size_t tab = line.find(L'\t', start);
        if (tab == std::wstring::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    return fields;
}

void AddIssue(std::vector<DictionaryIssue>* issues, size_t line_number, const wchar_t* reason)
{
    if (!issues || issues->size() >= 16) {
        return;
    }

    DictionaryIssue issue;
    issue.line_number = line_number;
    issue.reason = reason;
    issues->push_back(issue);
}

bool TryParseInt(const std::wstring& text, int* value)
{
    if (!value) {
        return false;
    }
    if (text.empty()) {
        *value = 0;
        return true;
    }

    wchar_t* end = nullptr;
    errno = 0;
    const long parsed = std::wcstol(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != L'\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }

    *value = static_cast<int>(parsed);
    return true;
}

std::wstring SqliteColumnText(sqlite3_stmt* statement, int column)
{
    const void* value = sqlite3_column_text16(statement, column);
    const int byte_count = sqlite3_column_bytes16(statement, column);
    if (!value || byte_count <= 0) {
        return L"";
    }
    return std::wstring(static_cast<const wchar_t*>(value), static_cast<size_t>(byte_count / sizeof(wchar_t)));
}

bool PrepareSql(sqlite3* database, const wchar_t* sql, sqlite3_stmt** statement)
{
    return database && statement && sqlite3_prepare16_v2(database, sql, -1, statement, nullptr) == SQLITE_OK;
}

bool StartsWith(const std::wstring& text, const std::wstring& prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::wstring NextPrefixUpperBound(const std::wstring& prefix)
{
    std::wstring upper = prefix;
    for (size_t index = upper.size(); index > 0; --index) {
        wchar_t& ch = upper[index - 1];
        if (ch < 0xFFFF) {
            ++ch;
            upper.resize(index);
            return upper;
        }
    }
    return L"";
}

std::vector<std::wstring> SplitTokens(const std::wstring& text)
{
    std::vector<std::wstring> tokens;
    size_t start = 0;
    while (start < text.size()) {
        while (start < text.size() && (text[start] == L' ' || text[start] == L'\'')) {
            ++start;
        }
        if (start >= text.size()) {
            break;
        }
        size_t end = start;
        while (end < text.size() && text[end] != L' ' && text[end] != L'\'') {
            ++end;
        }
        tokens.push_back(text.substr(start, end - start));
        start = end;
    }
    return tokens;
}

bool TokenPrefixesMatch(const std::wstring& text, const std::vector<std::wstring>& prefixes)
{
    if (prefixes.empty()) {
        return false;
    }
    const std::vector<std::wstring> tokens = SplitTokens(text);
    if (tokens.size() != prefixes.size()) {
        return false;
    }
    for (size_t i = 0; i < prefixes.size(); ++i) {
        if (prefixes[i].empty() || tokens[i].size() < prefixes[i].size() ||
            tokens[i].compare(0, prefixes[i].size(), prefixes[i]) != 0) {
            return false;
        }
    }
    return true;
}

std::wstring LikePrefixPattern(const std::wstring& prefix)
{
    if (prefix.empty()) {
        return L"%";
    }
    return prefix + L"%";
}

bool RankedCandidateLess(const RankedCandidate& left, const RankedCandidate& right)
{
    if (left.candidate.weight != right.candidate.weight) {
        return left.candidate.weight > right.candidate.weight;
    }
    if (left.candidate.user_weight != right.candidate.user_weight) {
        return left.candidate.user_weight > right.candidate.user_weight;
    }
    if (left.source_code != right.source_code) {
        return left.source_code < right.source_code;
    }
    return left.source_index < right.source_index;
}

void AppendLimitedCandidates(
    std::vector<Candidate>* target,
    const std::vector<Candidate>& source,
    size_t max_candidates)
{
    if (!target) {
        return;
    }
    for (const Candidate& candidate : source) {
        if (max_candidates != 0 && target->size() >= max_candidates) {
            return;
        }
        target->push_back(candidate);
    }
}

int LimitToSqliteInt(size_t max_candidates, size_t already_loaded)
{
    if (max_candidates == 0 || max_candidates > static_cast<size_t>(INT_MAX)) {
        return INT_MAX;
    }
    if (already_loaded >= max_candidates) {
        return 0;
    }
    return static_cast<int>(max_candidates - already_loaded);
}

void AppendSqliteRows(sqlite3_stmt* statement, std::vector<Candidate>* candidates)
{
    if (!statement || !candidates) {
        return;
    }
    while (sqlite3_step(statement) == SQLITE_ROW) {
        Candidate candidate;
        candidate.code = SqliteColumnText(statement, 0);
        candidate.text = SqliteColumnText(statement, 1);
        candidate.comment = SqliteColumnText(statement, 2);
        candidate.weight = sqlite3_column_int(statement, 3);
        candidate.user_weight = sqlite3_column_int(statement, 4);
        candidates->push_back(candidate);
    }
}

}  // namespace

void TsvCandidateSource::Reset(const std::wstring& path)
{
    path_ = path;
    Load();
}

void TsvCandidateSource::ReloadIfChanged()
{
    if (path_.empty()) {
        return;
    }

    FILETIME current_write_time = {};
    if (!ReadWriteTime(&current_write_time)) {
        if (available_ || !dictionary_.empty()) {
            ClearLoadedState();
            SourceLog(L"dictionary unavailable; unloaded");
        }
        return;
    }

    if (CompareFileTime(&current_write_time, &write_time_) != 0) {
        Load();
    }
}

std::vector<Candidate> TsvCandidateSource::QueryCandidates(
    const std::wstring& code,
    size_t max_candidates,
    bool prefix_candidates) const
{
    if (!prefix_candidates) {
        auto found = dictionary_.find(code);
        if (found == dictionary_.end()) {
            return std::vector<Candidate>();
        }
        if (max_candidates == 0 || found->second.size() <= max_candidates) {
            return found->second;
        }
        return std::vector<Candidate>(found->second.begin(), found->second.begin() + max_candidates);
    }

    std::vector<Candidate> candidates;
    auto exact = dictionary_.find(code);
    if (exact != dictionary_.end()) {
        AppendLimitedCandidates(&candidates, exact->second, max_candidates);
        if (max_candidates != 0 && candidates.size() >= max_candidates) {
            return candidates;
        }
    }

    std::vector<RankedCandidate> ranked;
    for (const auto& entry : dictionary_) {
        if (entry.first == code || !StartsWith(entry.first, code)) {
            continue;
        }
        for (size_t i = 0; i < entry.second.size(); ++i) {
            ranked.push_back({entry.second[i], false, entry.first, i});
        }
    }

    std::stable_sort(ranked.begin(), ranked.end(), RankedCandidateLess);

    const size_t remaining = max_candidates == 0 || max_candidates > candidates.size()
                                 ? max_candidates == 0 ? ranked.size() : max_candidates - candidates.size()
                                 : 0;
    const size_t limit = remaining > ranked.size() ? ranked.size() : remaining;
    candidates.reserve(candidates.size() + limit);
    for (size_t i = 0; i < limit; ++i) {
        candidates.push_back(ranked[i].candidate);
    }
    return candidates;
}

DictionaryStats TsvCandidateSource::GetStats() const
{
    DictionaryStats stats;
    stats.source_type = L"tsv";
    stats.status = available_ ? L"loaded" : (path_.empty() ? L"not configured" : L"not loaded");
    stats.configured = !path_.empty();
    stats.loaded = available_;
    stats.code_count = dictionary_.size();
    stats.line_count = line_count_;
    stats.skipped_line_count = skipped_line_count_;
    stats.invalid_line_count = invalid_line_count_;
    stats.issues = issues_;
    for (const auto& entry : dictionary_) {
        stats.candidate_count += entry.second.size();
    }
    return stats;
}

bool TsvCandidateSource::ReadWriteTime(FILETIME* write_time) const
{
    if (!write_time || path_.empty()) {
        return false;
    }

    HANDLE file = CreateFileW(
        path_.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    const BOOL ok = GetFileTime(file, nullptr, nullptr, write_time);
    CloseHandle(file);
    return ok != FALSE;
}

void TsvCandidateSource::Load()
{
    ClearLoadedState();
    if (path_.empty()) {
        return;
    }

    HANDLE file = CreateFileW(
        path_.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        SourceLog(L"dictionary open failed");
        return;
    }

    GetFileTime(file, nullptr, nullptr, &write_time_);

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(file);
        SourceLog(L"dictionary size invalid");
        return;
    }

    std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    BOOL ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok) {
        SourceLog(L"dictionary read failed");
        return;
    }
    bytes.resize(read);

    std::wstring text = Utf8ToWide(bytes);
    if (!text.empty() && text.front() == 0xFEFF) {
        text.erase(text.begin());
    }

    size_t start = 0;
    size_t line_number = 1;
    while (start < text.size()) {
        size_t end = text.find(L'\n', start);
        std::wstring line = end == std::wstring::npos ? text.substr(start) : text.substr(start, end - start);
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }

        if (line.empty() || line.front() == L'#') {
            ++skipped_line_count_;
        } else {
            ++line_count_;
            std::vector<std::wstring> fields = SplitLine(line);
            if (fields.size() >= 2 && !fields[0].empty() && !fields[1].empty()) {
                Candidate candidate;
                candidate.code = fields[0];
                candidate.text = fields[1];
                if (fields.size() >= 3) {
                    candidate.comment = fields[2];
                }
                if (fields.size() >= 4 && !TryParseInt(fields[3], &candidate.weight)) {
                    ++invalid_line_count_;
                    AddIssue(&issues_, line_number, L"invalid weight field");
                    if (end == std::wstring::npos) {
                        break;
                    }
                    start = end + 1;
                    ++line_number;
                    continue;
                }
                if (fields.size() >= 5 && !TryParseInt(fields[4], &candidate.user_weight)) {
                    ++invalid_line_count_;
                    AddIssue(&issues_, line_number, L"invalid user_weight field");
                    if (end == std::wstring::npos) {
                        break;
                    }
                    start = end + 1;
                    ++line_number;
                    continue;
                }
                dictionary_[fields[0]].push_back(candidate);
            } else {
                ++invalid_line_count_;
                AddIssue(&issues_, line_number, L"expected non-empty code and text fields");
            }
        }

        if (end == std::wstring::npos) {
            break;
        }
        start = end + 1;
        ++line_number;
    }

    for (auto& entry : dictionary_) {
        std::stable_sort(entry.second.begin(), entry.second.end(), [](const Candidate& left, const Candidate& right) {
            if (left.weight != right.weight) {
                return left.weight > right.weight;
            }
            return left.user_weight > right.user_weight;
        });
    }

    available_ = true;
    SourceLog(dictionary_.empty() ? L"dictionary loaded: empty" : L"dictionary loaded");
}

void TsvCandidateSource::ClearLoadedState()
{
    dictionary_.clear();
    write_time_ = {};
    available_ = false;
    line_count_ = 0;
    skipped_line_count_ = 0;
    invalid_line_count_ = 0;
    issues_.clear();
}

std::vector<Candidate> TsvCandidateSource::QueryPinyinPrefixCandidates(const std::vector<std::wstring>& prefixes, size_t max_candidates) const
{
    std::vector<Candidate> candidates;
    if (prefixes.size() < 2) {
        return candidates;
    }

    std::vector<RankedCandidate> ranked;
    for (const auto& entry : dictionary_) {
        for (size_t i = 0; i < entry.second.size(); ++i) {
            if (TokenPrefixesMatch(entry.first, prefixes) || TokenPrefixesMatch(entry.second[i].comment, prefixes)) {
                ranked.push_back({entry.second[i], false, entry.first, i});
            }
        }
    }

    std::stable_sort(ranked.begin(), ranked.end(), RankedCandidateLess);
    const size_t limit = max_candidates == 0 || max_candidates > ranked.size() ? ranked.size() : max_candidates;
    for (size_t i = 0; i < limit; ++i) {
        candidates.push_back(ranked[i].candidate);
    }
    return candidates;
}

void TsvCandidateSource::RecordSelection(const std::wstring&, const Candidate&)
{
}

void TsvCandidateSource::DeleteCandidate(const std::wstring&, const Candidate&)
{
}

std::vector<Candidate> EmptyCandidateSource::QueryCandidates(const std::wstring&, size_t, bool) const
{
    return std::vector<Candidate>();
}

std::vector<Candidate> EmptyCandidateSource::QueryPinyinPrefixCandidates(const std::vector<std::wstring>&, size_t) const
{
    return std::vector<Candidate>();
}

void EmptyCandidateSource::RecordSelection(const std::wstring&, const Candidate&)
{
}

void EmptyCandidateSource::DeleteCandidate(const std::wstring&, const Candidate&)
{
}

DictionaryStats EmptyCandidateSource::GetStats() const
{
    DictionaryStats stats;
    stats.source_type = source_type_;
    stats.status = path_.empty() ? L"not configured" : L"unsupported source type";
    stats.configured = !path_.empty();
    stats.loaded = false;
    return stats;
}

SqliteCandidateSource::~SqliteCandidateSource()
{
    Close();
}

void SqliteCandidateSource::Reset(const std::wstring& path)
{
    path_ = path;
    Load();
}

void SqliteCandidateSource::ReloadIfChanged()
{
    if (path_.empty()) {
        return;
    }

    const DWORD now = GetTickCount();
    if (last_reload_check_tick_ != 0 && now - last_reload_check_tick_ < 500) {
        return;
    }
    last_reload_check_tick_ = now;

    FILETIME current_write_time = {};
    if (!ReadWriteTime(&current_write_time)) {
        if (available_) {
            Close();
            status_ = L"not loaded";
            SourceLog(L"sqlite dictionary unavailable; unloaded");
        }
        return;
    }

    if (CompareFileTime(&current_write_time, &write_time_) != 0) {
        Load();
    }
}

std::vector<Candidate> SqliteCandidateSource::QueryCandidates(
    const std::wstring& code,
    size_t max_candidates,
    bool prefix_candidates) const
{
    std::vector<Candidate> candidates;
    if (!database_ || !available_) {
        return candidates;
    }

    sqlite3_stmt* statement = nullptr;
    const wchar_t* exact_sql =
        L"SELECT code, text, comment, weight, effective_user_weight "
        L"FROM ("
        L"SELECT e.code AS code, e.text AS text, e.comment AS comment, e.weight AS weight, "
        L"e.user_weight + COALESCE(u.user_weight, 0) AS effective_user_weight, e.id AS sort_id "
        L"FROM entries e "
        L"LEFT JOIN user_entries u ON u.code = e.code AND u.text = e.text AND u.deleted = 0 "
        L"WHERE e.code = ? AND NOT EXISTS ("
        L"SELECT 1 FROM user_entries d WHERE d.code = e.code AND d.text = e.text AND d.deleted = 1"
        L") "
        L"UNION ALL "
        L"SELECT u.code AS code, u.text AS text, u.comment AS comment, u.weight AS weight, "
        L"u.user_weight AS effective_user_weight, u.id AS sort_id "
        L"FROM user_entries u "
        L"WHERE u.code = ? AND u.deleted = 0 AND NOT EXISTS ("
        L"SELECT 1 FROM entries e WHERE e.code = u.code AND e.text = u.text"
        L")"
        L") "
        L"ORDER BY effective_user_weight DESC, weight DESC, sort_id ASC "
        L"LIMIT ?";
    if (!PrepareSql(database_, exact_sql, &statement)) {
        return candidates;
    }

    sqlite3_bind_text16(statement, 1, code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(statement, 2, code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, LimitToSqliteInt(max_candidates, 0));
    AppendSqliteRows(statement, &candidates);
    sqlite3_finalize(statement);

    if (!prefix_candidates || (max_candidates != 0 && candidates.size() >= max_candidates)) {
        return candidates;
    }

    statement = nullptr;
    const std::wstring upper_bound = NextPrefixUpperBound(code);
    const wchar_t* prefix_sql = upper_bound.empty()
                                    ? L"SELECT code, text, comment, weight, effective_user_weight "
                                      L"FROM ("
                                      L"SELECT e.code AS code, e.text AS text, e.comment AS comment, e.weight AS weight, "
                                      L"e.user_weight + COALESCE(u.user_weight, 0) AS effective_user_weight, e.id AS sort_id "
                                      L"FROM entries e "
                                      L"LEFT JOIN user_entries u ON u.code = e.code AND u.text = e.text AND u.deleted = 0 "
                                      L"WHERE e.code >= ? AND e.code <> ? AND NOT EXISTS ("
                                      L"SELECT 1 FROM user_entries d WHERE d.code = e.code AND d.text = e.text AND d.deleted = 1"
                                      L") "
                                      L"UNION ALL "
                                      L"SELECT u.code AS code, u.text AS text, u.comment AS comment, u.weight AS weight, "
                                      L"u.user_weight AS effective_user_weight, u.id AS sort_id "
                                      L"FROM user_entries u "
                                      L"WHERE u.code >= ? AND u.code <> ? AND u.deleted = 0 AND NOT EXISTS ("
                                      L"SELECT 1 FROM entries e WHERE e.code = u.code AND e.text = u.text"
                                      L")"
                                      L") "
                                      L"ORDER BY effective_user_weight DESC, weight DESC, code ASC, sort_id ASC "
                                      L"LIMIT ?"
                                    : L"SELECT code, text, comment, weight, effective_user_weight "
                                      L"FROM ("
                                      L"SELECT e.code AS code, e.text AS text, e.comment AS comment, e.weight AS weight, "
                                      L"e.user_weight + COALESCE(u.user_weight, 0) AS effective_user_weight, e.id AS sort_id "
                                      L"FROM entries e "
                                      L"LEFT JOIN user_entries u ON u.code = e.code AND u.text = e.text AND u.deleted = 0 "
                                      L"WHERE e.code >= ? AND e.code < ? AND e.code <> ? AND NOT EXISTS ("
                                      L"SELECT 1 FROM user_entries d WHERE d.code = e.code AND d.text = e.text AND d.deleted = 1"
                                      L") "
                                      L"UNION ALL "
                                      L"SELECT u.code AS code, u.text AS text, u.comment AS comment, u.weight AS weight, "
                                      L"u.user_weight AS effective_user_weight, u.id AS sort_id "
                                      L"FROM user_entries u "
                                      L"WHERE u.code >= ? AND u.code < ? AND u.code <> ? AND u.deleted = 0 AND NOT EXISTS ("
                                      L"SELECT 1 FROM entries e WHERE e.code = u.code AND e.text = u.text"
                                      L")"
                                      L") "
                                      L"ORDER BY effective_user_weight DESC, weight DESC, code ASC, sort_id ASC "
                                      L"LIMIT ?";
    if (!PrepareSql(database_, prefix_sql, &statement)) {
        return candidates;
    }

    const int prefix_limit = LimitToSqliteInt(max_candidates, candidates.size());
    sqlite3_bind_text16(statement, 1, code.c_str(), -1, SQLITE_TRANSIENT);
    if (upper_bound.empty()) {
        sqlite3_bind_text16(statement, 2, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(statement, 3, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(statement, 4, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 5, prefix_limit);
    } else {
        sqlite3_bind_text16(statement, 2, upper_bound.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(statement, 3, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(statement, 4, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(statement, 5, upper_bound.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(statement, 6, code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 7, prefix_limit);
    }
    AppendSqliteRows(statement, &candidates);
    sqlite3_finalize(statement);
    return candidates;
}

std::vector<Candidate> SqliteCandidateSource::QueryPinyinPrefixCandidates(const std::vector<std::wstring>& prefixes, size_t max_candidates) const
{
    std::vector<Candidate> candidates;
    if (!database_ || !available_ || prefixes.size() < 2) {
        return candidates;
    }

    const std::wstring first_prefix = prefixes.front();
    if (first_prefix.empty()) {
        return candidates;
    }

    sqlite3_stmt* statement = nullptr;
    const wchar_t* sql =
        L"SELECT code, text, comment, weight, effective_user_weight "
        L"FROM ("
        L"SELECT e.code AS code, e.text AS text, e.comment AS comment, e.weight AS weight, "
        L"e.user_weight + COALESCE(u.user_weight, 0) AS effective_user_weight, e.id AS sort_id "
        L"FROM entries e "
        L"LEFT JOIN user_entries u ON u.code = e.code AND u.text = e.text AND u.deleted = 0 "
        L"WHERE e.comment LIKE ? AND NOT EXISTS ("
        L"SELECT 1 FROM user_entries d WHERE d.code = e.code AND d.text = e.text AND d.deleted = 1"
        L") "
        L"UNION ALL "
        L"SELECT u.code AS code, u.text AS text, u.comment AS comment, u.weight AS weight, "
        L"u.user_weight AS effective_user_weight, u.id AS sort_id "
        L"FROM user_entries u "
        L"WHERE u.comment LIKE ? AND u.deleted = 0 AND NOT EXISTS ("
        L"SELECT 1 FROM entries e WHERE e.code = u.code AND e.text = u.text"
        L")"
        L") "
        L"ORDER BY effective_user_weight DESC, weight DESC, code ASC, sort_id ASC "
        L"LIMIT ?";
    if (!PrepareSql(database_, sql, &statement)) {
        return candidates;
    }

    const std::wstring first_pattern = LikePrefixPattern(first_prefix);
    const size_t scan_limit = max_candidates == 0 ? 10000 : std::min<size_t>(10000, std::max<size_t>(1000, max_candidates * 100));
    sqlite3_bind_text16(statement, 1, first_pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(statement, 2, first_pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, LimitToSqliteInt(scan_limit, 0));

    std::vector<Candidate> scanned;
    AppendSqliteRows(statement, &scanned);
    sqlite3_finalize(statement);

    for (Candidate& candidate : scanned) {
        if (max_candidates != 0 && candidates.size() >= max_candidates) {
            break;
        }
        if (TokenPrefixesMatch(candidate.comment, prefixes)) {
            candidates.push_back(std::move(candidate));
        }
    }
    return candidates;
}

void SqliteCandidateSource::RecordSelection(const std::wstring& code, const Candidate& candidate)
{
    if (!database_ || !available_ || code.empty() || candidate.text.empty()) {
        return;
    }

    const std::wstring effective_code = candidate.code.empty() ? code : candidate.code;
    sqlite3_exec(database_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);

    sqlite3_stmt* statement = nullptr;
    const wchar_t* sql =
        L"INSERT INTO user_entries(code, text, comment, weight, user_weight, updated_at_utc) "
        L"VALUES (?, ?, ?, 0, 1000, datetime('now')) "
        L"ON CONFLICT(code, text) DO UPDATE SET "
        L"comment = excluded.comment, "
        L"user_weight = user_entries.user_weight + 1000, "
        L"deleted = 0, "
        L"updated_at_utc = excluded.updated_at_utc";
    if (!PrepareSql(database_, sql, &statement)) {
        return;
    }
    sqlite3_bind_text16(statement, 1, effective_code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(statement, 2, candidate.text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(statement, 3, candidate.comment.c_str(), -1, SQLITE_TRANSIENT);
    const bool selection_recorded = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);

    if (!selection_recorded) {
        sqlite3_exec(database_, "ROLLBACK", nullptr, nullptr, nullptr);
        return;
    }

    statement = nullptr;
    const wchar_t* event_sql =
        L"INSERT INTO user_learning_events(code, text, comment, delta_user_weight, created_at_utc) "
        L"VALUES (?, ?, ?, 1000, datetime('now'))";
    if (PrepareSql(database_, event_sql, &statement)) {
        sqlite3_bind_text16(statement, 1, effective_code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(statement, 2, candidate.text.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(statement, 3, candidate.comment.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }
    sqlite3_exec(database_, "COMMIT", nullptr, nullptr, nullptr);
}

void SqliteCandidateSource::DeleteCandidate(const std::wstring& code, const Candidate& candidate)
{
    if (!database_ || !available_ || code.empty() || candidate.text.empty()) {
        return;
    }

    const std::wstring effective_code = candidate.code.empty() ? code : candidate.code;
    sqlite3_stmt* statement = nullptr;
    const wchar_t* sql =
        L"INSERT INTO user_entries(code, text, comment, weight, user_weight, deleted, updated_at_utc) "
        L"VALUES (?, ?, ?, 0, 0, 1, datetime('now')) "
        L"ON CONFLICT(code, text) DO UPDATE SET "
        L"comment = excluded.comment, "
        L"user_weight = 0, "
        L"deleted = 1, "
        L"updated_at_utc = excluded.updated_at_utc";
    if (!PrepareSql(database_, sql, &statement)) {
        return;
    }
    sqlite3_bind_text16(statement, 1, effective_code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(statement, 2, candidate.text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(statement, 3, candidate.comment.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(statement);
    sqlite3_finalize(statement);
}

DictionaryStats SqliteCandidateSource::GetStats() const
{
    DictionaryStats stats;
    stats.source_type = L"sqlite";
    stats.status = path_.empty() ? L"not configured" : status_;
    stats.configured = !path_.empty();
    stats.loaded = available_;
    stats.code_count = code_count_;
    stats.candidate_count = candidate_count_;
    return stats;
}

bool SqliteCandidateSource::ReadWriteTime(FILETIME* write_time) const
{
    if (!write_time || path_.empty()) {
        return false;
    }

    HANDLE file = CreateFileW(
        path_.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    const BOOL ok = GetFileTime(file, nullptr, nullptr, write_time);
    CloseHandle(file);
    return ok != FALSE;
}

void SqliteCandidateSource::Load()
{
    Close();
    status_ = path_.empty() ? L"not configured" : L"not loaded";
    code_count_ = 0;
    candidate_count_ = 0;
    write_time_ = {};
    last_reload_check_tick_ = GetTickCount();

    if (path_.empty()) {
        return;
    }
    if (!ReadWriteTime(&write_time_)) {
        SourceLog(L"sqlite dictionary file unavailable");
        return;
    }
    if (sqlite3_open16(path_.c_str(), &database_) != SQLITE_OK) {
        Close();
        status_ = L"open failed";
        SourceLog(L"sqlite dictionary open failed");
        return;
    }
    sqlite3_busy_timeout(database_, 1000);
    sqlite3_exec(database_, "PRAGMA temp_store=MEMORY;", nullptr, nullptr, nullptr);
    sqlite3_exec(database_, "PRAGMA cache_size=-32768;", nullptr, nullptr, nullptr);
    sqlite3_exec(database_, "PRAGMA mmap_size=268435456;", nullptr, nullptr, nullptr);

    sqlite3_stmt* statement = nullptr;
    const char* migration_sql =
        "CREATE TABLE IF NOT EXISTS user_entries ("
        "id INTEGER PRIMARY KEY,"
        "code TEXT NOT NULL,"
        "text TEXT NOT NULL,"
        "comment TEXT NOT NULL DEFAULT '',"
        "weight INTEGER NOT NULL DEFAULT 0,"
        "user_weight INTEGER NOT NULL DEFAULT 0,"
        "deleted INTEGER NOT NULL DEFAULT 0,"
        "created_at_utc TEXT NOT NULL DEFAULT (datetime('now')),"
        "updated_at_utc TEXT NOT NULL DEFAULT (datetime('now')),"
        "UNIQUE(code, text)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_user_entries_code_rank "
        "ON user_entries(code, deleted, weight DESC, user_weight DESC, id ASC);"
        "CREATE TABLE IF NOT EXISTS user_learning_events ("
        "id INTEGER PRIMARY KEY,"
        "code TEXT NOT NULL,"
        "text TEXT NOT NULL,"
        "comment TEXT NOT NULL DEFAULT '',"
        "delta_user_weight INTEGER NOT NULL,"
        "created_at_utc TEXT NOT NULL DEFAULT (datetime('now')),"
        "reverted_at_utc TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_user_learning_events_active "
        "ON user_learning_events(reverted_at_utc, id DESC);";
    char* error = nullptr;
    const int migration_result = sqlite3_exec(database_, migration_sql, nullptr, nullptr, &error);
    if (migration_result != SQLITE_OK) {
        if (error) {
            sqlite3_free(error);
        }
        Close();
        status_ = L"user schema invalid";
        SourceLog(L"sqlite user schema migration failed");
        return;
    }

    if (!PrepareSql(database_, L"SELECT e.code, e.text, e.comment, e.weight, e.user_weight FROM entries e WHERE e.code = ? ORDER BY e.weight DESC, e.user_weight DESC, e.id ASC", &statement)) {
        Close();
        status_ = L"schema invalid";
        SourceLog(L"sqlite dictionary schema invalid");
        return;
    }
    sqlite3_finalize(statement);

    if (PrepareSql(database_, L"SELECT COUNT(DISTINCT code), COUNT(*) FROM entries", &statement)) {
        if (sqlite3_step(statement) == SQLITE_ROW) {
            code_count_ = static_cast<size_t>(sqlite3_column_int64(statement, 0));
            candidate_count_ = static_cast<size_t>(sqlite3_column_int64(statement, 1));
        }
        sqlite3_finalize(statement);
    }

    available_ = true;
    status_ = L"loaded";
    SourceLog(L"sqlite dictionary loaded");
}

void SqliteCandidateSource::Close()
{
    if (database_) {
        sqlite3_close(database_);
        database_ = nullptr;
    }
    available_ = false;
}

}  // namespace arrowinput
