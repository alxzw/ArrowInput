#include "PinyinParser.h"

#include <algorithm>
#include <array>
#include <unordered_set>
#include <vector>

namespace arrowinput {

namespace {

constexpr const wchar_t* kSyllables[] = {
    L"a", L"ai", L"an", L"ang", L"ao",
    L"ba", L"bai", L"ban", L"bang", L"bao", L"bei", L"ben", L"beng", L"bi", L"bian", L"biao", L"bie", L"bin", L"bing", L"bo", L"bu",
    L"ca", L"cai", L"can", L"cang", L"cao", L"ce", L"cen", L"ceng", L"cha", L"chai", L"chan", L"chang", L"chao", L"che", L"chen", L"cheng", L"chi", L"chong", L"chou", L"chu", L"chua", L"chuai", L"chuan", L"chuang", L"chui", L"chun", L"chuo", L"ci", L"cong", L"cou", L"cu", L"cuan", L"cui", L"cun", L"cuo",
    L"da", L"dai", L"dan", L"dang", L"dao", L"de", L"dei", L"den", L"deng", L"di", L"dia", L"dian", L"diao", L"die", L"ding", L"diu", L"dong", L"dou", L"du", L"duan", L"dui", L"dun", L"duo",
    L"e", L"ei", L"en", L"eng", L"er",
    L"fa", L"fan", L"fang", L"fei", L"fen", L"feng", L"fo", L"fou", L"fu",
    L"ga", L"gai", L"gan", L"gang", L"gao", L"ge", L"gei", L"gen", L"geng", L"gong", L"gou", L"gu", L"gua", L"guai", L"guan", L"guang", L"gui", L"gun", L"guo",
    L"ha", L"hai", L"han", L"hang", L"hao", L"he", L"hei", L"hen", L"heng", L"hong", L"hou", L"hu", L"hua", L"huai", L"huan", L"huang", L"hui", L"hun", L"huo",
    L"ji", L"jia", L"jian", L"jiang", L"jiao", L"jie", L"jin", L"jing", L"jiong", L"jiu", L"ju", L"juan", L"jue", L"jun",
    L"ka", L"kai", L"kan", L"kang", L"kao", L"ke", L"kei", L"ken", L"keng", L"kong", L"kou", L"ku", L"kua", L"kuai", L"kuan", L"kuang", L"kui", L"kun", L"kuo",
    L"la", L"lai", L"lan", L"lang", L"lao", L"le", L"lei", L"leng", L"li", L"lia", L"lian", L"liang", L"liao", L"lie", L"lin", L"ling", L"liu", L"lo", L"long", L"lou", L"lu", L"luan", L"lun", L"luo", L"lv", L"lve",
    L"ma", L"mai", L"man", L"mang", L"mao", L"me", L"mei", L"men", L"meng", L"mi", L"mian", L"miao", L"mie", L"min", L"ming", L"miu", L"mo", L"mou", L"mu",
    L"na", L"nai", L"nan", L"nang", L"nao", L"ne", L"nei", L"nen", L"neng", L"ni", L"nian", L"niang", L"niao", L"nie", L"nin", L"ning", L"niu", L"nong", L"nou", L"nu", L"nuan", L"nun", L"nuo", L"nv", L"nve",
    L"o", L"ou",
    L"pa", L"pai", L"pan", L"pang", L"pao", L"pei", L"pen", L"peng", L"pi", L"pian", L"piao", L"pie", L"pin", L"ping", L"po", L"pou", L"pu",
    L"qi", L"qia", L"qian", L"qiang", L"qiao", L"qie", L"qin", L"qing", L"qiong", L"qiu", L"qu", L"quan", L"que", L"qun",
    L"ran", L"rang", L"rao", L"re", L"ren", L"reng", L"ri", L"rong", L"rou", L"ru", L"ruan", L"rui", L"run", L"ruo",
    L"sa", L"sai", L"san", L"sang", L"sao", L"se", L"sen", L"seng", L"sha", L"shai", L"shan", L"shang", L"shao", L"she", L"shen", L"sheng", L"shi", L"shou", L"shu", L"shua", L"shuai", L"shuan", L"shuang", L"shui", L"shun", L"shuo", L"si", L"song", L"sou", L"su", L"suan", L"sui", L"sun", L"suo",
    L"ta", L"tai", L"tan", L"tang", L"tao", L"te", L"teng", L"ti", L"tian", L"tiao", L"tie", L"ting", L"tong", L"tou", L"tu", L"tuan", L"tui", L"tun", L"tuo",
    L"wa", L"wai", L"wan", L"wang", L"wei", L"wen", L"weng", L"wo", L"wu",
    L"xi", L"xia", L"xian", L"xiang", L"xiao", L"xie", L"xin", L"xing", L"xiong", L"xiu", L"xu", L"xuan", L"xue", L"xun",
    L"ya", L"yan", L"yang", L"yao", L"ye", L"yi", L"yin", L"ying", L"yo", L"yong", L"you", L"yu", L"yuan", L"yue", L"yun",
    L"za", L"zai", L"zan", L"zang", L"zao", L"ze", L"zei", L"zen", L"zeng", L"zha", L"zhai", L"zhan", L"zhang", L"zhao", L"zhe", L"zhen", L"zheng", L"zhi", L"zhong", L"zhou", L"zhu", L"zhua", L"zhuai", L"zhuan", L"zhuang", L"zhui", L"zhun", L"zhuo", L"zi", L"zong", L"zou", L"zu", L"zuan", L"zui", L"zun", L"zuo"
};

bool IsSyllable(const std::wstring& text)
{
    for (const wchar_t* syllable : kSyllables) {
        if (text == syllable) {
            return true;
        }
    }
    return false;
}

bool IsSyllablePrefix(const std::wstring& text)
{
    if (text.empty()) {
        return false;
    }
    for (const wchar_t* syllable : kSyllables) {
        const std::wstring value = syllable;
        if (value.size() >= text.size() && value.compare(0, text.size(), text) == 0) {
            return true;
        }
    }
    return false;
}

std::wstring Join(const std::vector<std::wstring>& parts)
{
    std::wstring joined;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) {
            joined.push_back(L' ');
        }
        joined += parts[i];
    }
    return joined;
}

std::vector<std::wstring> SplitTokens(const std::wstring& text)
{
    std::vector<std::wstring> tokens;
    size_t start = 0;
    while (start < text.size()) {
        while (start < text.size() && text[start] == L' ') {
            ++start;
        }
        if (start >= text.size()) {
            break;
        }
        size_t end = start;
        while (end < text.size() && text[end] != L' ') {
            ++end;
        }
        tokens.push_back(text.substr(start, end - start));
        start = end;
    }
    return tokens;
}

void AddFuzzyToken(std::vector<std::wstring>* target, const std::wstring& token)
{
    if (!target || token.empty()) {
        return;
    }

    auto add_if_valid = [target](const std::wstring& value) {
        if (IsSyllable(value) && std::find(target->begin(), target->end(), value) == target->end()) {
            target->push_back(value);
        }
    };

    if (token.rfind(L"zh", 0) == 0) {
        add_if_valid(L"z" + token.substr(2));
    } else if (token.rfind(L"z", 0) == 0) {
        add_if_valid(L"zh" + token.substr(1));
    }
    if (token.rfind(L"ch", 0) == 0) {
        add_if_valid(L"c" + token.substr(2));
    } else if (token.rfind(L"c", 0) == 0) {
        add_if_valid(L"ch" + token.substr(1));
    }
    if (token.rfind(L"sh", 0) == 0) {
        add_if_valid(L"s" + token.substr(2));
    } else if (token.rfind(L"s", 0) == 0) {
        add_if_valid(L"sh" + token.substr(1));
    }

    const struct {
        const wchar_t* left;
        const wchar_t* right;
    } finals[] = {
        {L"ang", L"an"},
        {L"eng", L"en"},
        {L"ing", L"in"},
    };
    for (const auto& pair : finals) {
        const std::wstring left = pair.left;
        const std::wstring right = pair.right;
        if (token.size() > left.size() && token.compare(token.size() - left.size(), left.size(), left) == 0) {
            add_if_valid(token.substr(0, token.size() - left.size()) + right);
        } else if (token.size() > right.size() && token.compare(token.size() - right.size(), right.size(), right) == 0) {
            add_if_valid(token.substr(0, token.size() - right.size()) + left);
        }
    }
}

}  // namespace

PinyinParseResult ParsePinyinCode(const std::wstring& code)
{
    PinyinParseResult result;
    result.compact_code = code;
    if (code.empty()) {
        return result;
    }

    for (wchar_t ch : code) {
        if (ch < L'a' || ch > L'z') {
            return result;
        }
    }

    const size_t n = code.size();
    std::vector<int> previous(n + 1, -1);
    std::vector<int> syllable_count(n + 1, 1000000);
    previous[0] = 0;
    syllable_count[0] = 0;

    for (size_t start = 0; start < n; ++start) {
        if (previous[start] < 0) {
            continue;
        }
        for (size_t len = 1; len <= 6 && start + len <= n; ++len) {
            std::wstring part = code.substr(start, len);
            if (!IsSyllable(part)) {
                continue;
            }
            const size_t end = start + len;
            const int candidate_count = syllable_count[start] + 1;
            if (candidate_count < syllable_count[end]) {
                syllable_count[end] = candidate_count;
                previous[end] = static_cast<int>(start);
            }
        }
    }

    if (previous[n] < 0) {
        return result;
    }

    std::vector<std::wstring> parts;
    for (size_t end = n; end > 0;) {
        const int start = previous[end];
        if (start < 0) {
            return result;
        }
        parts.push_back(code.substr(static_cast<size_t>(start), end - static_cast<size_t>(start)));
        end = static_cast<size_t>(start);
    }
    std::reverse(parts.begin(), parts.end());

    result.spaced_code = Join(parts);
    result.complete = true;
    return result;
}

void BuildPinyinPrefixPatternsRecursive(
    const std::wstring& code,
    size_t start,
    std::vector<std::wstring>* current,
    std::vector<std::vector<std::wstring>>* patterns)
{
    if (!current || !patterns || patterns->size() >= 24) {
        return;
    }
    if (start == code.size()) {
        if (current->size() >= 2) {
            patterns->push_back(*current);
        }
        return;
    }

    const size_t remaining = code.size() - start;
    const size_t max_len = remaining > 6 ? 6 : remaining;
    for (size_t len = max_len; len >= 1; --len) {
        std::wstring part = code.substr(start, len);
        if (IsSyllablePrefix(part)) {
            current->push_back(part);
            BuildPinyinPrefixPatternsRecursive(code, start + len, current, patterns);
            current->pop_back();
            if (patterns->size() >= 24) {
                return;
            }
        }
        if (len == 1) {
            break;
        }
    }
}

std::vector<std::vector<std::wstring>> BuildPinyinPrefixPatterns(const std::wstring& code)
{
    std::vector<std::vector<std::wstring>> patterns;
    if (code.size() < 2 || code.size() > 8) {
        return patterns;
    }
    for (wchar_t ch : code) {
        if (ch < L'a' || ch > L'z') {
            return patterns;
        }
    }

    std::vector<std::wstring> current;
    BuildPinyinPrefixPatternsRecursive(code, 0, &current, &patterns);
    return patterns;
}

std::vector<std::wstring> BuildFuzzyPinyinCodes(const std::wstring& spaced_code, size_t max_results)
{
    std::vector<std::wstring> results;
    if (spaced_code.empty() || max_results == 0) {
        return results;
    }

    const std::vector<std::wstring> tokens = SplitTokens(spaced_code);
    if (tokens.empty()) {
        return results;
    }

    std::vector<std::vector<std::wstring>> token_options;
    token_options.reserve(tokens.size());
    bool has_fuzzy = false;
    for (const std::wstring& token : tokens) {
        if (!IsSyllable(token)) {
            return results;
        }
        std::vector<std::wstring> options = {token};
        AddFuzzyToken(&options, token);
        has_fuzzy = has_fuzzy || options.size() > 1;
        token_options.push_back(std::move(options));
    }
    if (!has_fuzzy) {
        return results;
    }

    std::unordered_set<std::wstring> seen;
    std::vector<std::wstring> current(tokens.size());
    auto build = [&](auto&& self, size_t index) -> void {
        if (results.size() >= max_results) {
            return;
        }
        if (index == token_options.size()) {
            const std::wstring value = Join(current);
            if (value != spaced_code && seen.insert(value).second) {
                results.push_back(value);
            }
            return;
        }
        for (const std::wstring& option : token_options[index]) {
            current[index] = option;
            self(self, index + 1);
            if (results.size() >= max_results) {
                return;
            }
        }
    };

    build(build, 0);
    return results;
}

}  // namespace arrowinput
