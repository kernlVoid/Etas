#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_map>
#include <cstring>
#include <cstdint>
#include <optional>

namespace Etas {

/**
 * Ultra-fast, safer and more feature-rich TextAnalyzer
 * - Minimizes allocations by offering string_view-based accessors
 * - Additional useful tools: frequency analysis, top-N words, trim/startsWith/endsWith
 * - Marked noexcept where appropriate and optimized loops
 * - Header-only, C++17 compatible
 */
class TextAnalyzer {
public:
    using sv = std::string_view;
    explicit TextAnalyzer(sv text) noexcept : text_(text) {}

    // ------------------------------------------------------------------
    // 1. SPLITTING FUNCTIONS (allocation-free variants)
    // ------------------------------------------------------------------

    // Split into substrings (owning strings) - kept for compatibility
    std::vector<std::string> split(sv delimiter) const {
        if (text_.empty()) return {};
        if (delimiter.empty()) return { std::string(text_) };

        std::vector<std::string> result;
        result.reserve(text_.size() / (delimiter.size() + 1) + 1);

        size_t start = 0;
        size_t pos = text_.find(delimiter, start);
        while (pos != sv::npos) {
            result.emplace_back(text_.substr(start, pos - start));
            start = pos + delimiter.size();
            pos = text_.find(delimiter, start);
        }
        result.emplace_back(text_.substr(start));
        return result;
    }

    // Split into string_views (no allocations, fastest)
    std::vector<sv> splitView(sv delimiter) const noexcept {
        std::vector<sv> result;
        if (text_.empty()) return result;
        if (delimiter.empty()) { result.push_back(text_); return result; }

        result.reserve(text_.size() / (delimiter.size() + 1) + 1);

        size_t start = 0;
        size_t pos = text_.find(delimiter, start);
        while (pos != sv::npos) {
            result.emplace_back(text_.substr(start, pos - start));
            start = pos + delimiter.size();
            pos = text_.find(delimiter, start);
        }
        result.emplace_back(text_.substr(start));
        return result;
    }

    // Split by newline, handling \n, \r\n, \r efficiently and returning views
    std::vector<sv> splitByNewlineView() const noexcept {
        std::vector<sv> lines;
        const char* data = text_.data();
        size_t len = text_.size();
        lines.reserve(std::min<size_t>(256, std::max<size_t>(1, len / 40)));

        size_t i = 0, start = 0;
        while (i < len) {
            if (data[i] == '\n') {
                lines.emplace_back(data + start, i - start);
                ++i;
                start = i;
            } else if (data[i] == '\r') {
                lines.emplace_back(data + start, i - start);
                ++i;
                if (i < len && data[i] == '\n') ++i; // consume \n in CRLF
                start = i;
            } else {
                ++i;
            }
        }
        if (start <= len) lines.emplace_back(data + start, len - start);
        return lines;
    }

    // Owning version for compatibility
    std::vector<std::string> splitByNewline() const { auto v = splitByNewlineView(); std::vector<std::string> out; out.reserve(v.size()); for (auto &s: v) out.emplace_back(s); return out; }

    // Tokenize by any whitespace (view variant - no allocations)
    std::vector<sv> tokenizeByWhitespaceView() const noexcept {
        std::vector<sv> tokens;
        const char* ptr = text_.data();
        size_t len = text_.size();
        tokens.reserve(len / 4 + 1);

        size_t i = 0;
        while (i < len) {
            // skip whitespace
            while (i < len && std::isspace(static_cast<unsigned char>(ptr[i]))) ++i;
            if (i >= len) break;
            size_t start = i;
            while (i < len && !std::isspace(static_cast<unsigned char>(ptr[i]))) ++i;
            tokens.emplace_back(ptr + start, i - start);
        }
        return tokens;
    }

    // Owning version
    std::vector<std::string> tokenizeByWhitespace() const { auto v = tokenizeByWhitespaceView(); std::vector<std::string> out; out.reserve(v.size()); for (auto &s: v) out.emplace_back(s); return out; }

    // ------------------------------------------------------------------
    // 2. SEARCH & ANALYSIS FUNCTIONS
    // ------------------------------------------------------------------

    // Find all occurrences (returns indices)
    std::vector<size_t> findAll(sv search) const noexcept {
        std::vector<size_t> positions;
        if (search.empty() || text_.empty()) return positions;
        positions.reserve(16);
        size_t pos = text_.find(search, 0);
        while (pos != sv::npos) {
            positions.push_back(pos);
            pos = text_.find(search, pos + 1);
        }
        return positions;
    }

    bool contains(sv sub) const noexcept { return !sub.empty() && !text_.empty() && text_.find(sub) != sv::npos; }

    bool containsCaseInsensitive(sv sub) const {
        if (sub.empty()) return true;
        auto lt = toLowerString(text_);
        auto ls = toLowerString(sub);
        return lt.find(ls) != sv::npos;
    }

    // Count non-overlapping occurrences (safe and fast)
    size_t count(sv sub) const noexcept {
        if (sub.empty() || text_.empty()) return 0;
        size_t cnt = 0;
        size_t pos = text_.find(sub, 0);
        while (pos != sv::npos) {
            ++cnt;
            pos = text_.find(sub, pos + sub.size());
        }
        return cnt;
    }

    // ------------------------------------------------------------------
    // 3. TRANSFORMATION & CLEANING
    // ------------------------------------------------------------------

    // Remove duplicate whitespace (owning)
    std::string cleanWhitespace() const {
        if (text_.empty()) return {};
        std::string out;
        out.reserve(text_.size());
        bool lastSpace = false;
        for (unsigned char uc : svToUnsignedChars(text_)) {
            if (std::isspace(uc)) {
                if (!lastSpace) { out.push_back(' '); lastSpace = true; }
            } else {
                out.push_back(static_cast<char>(uc));
                lastSpace = false;
            }
        }
        // trim
        if (!out.empty() && out.front() == ' ') out.erase(out.begin());
        if (!out.empty() && out.back() == ' ') out.pop_back();
        return out;
    }

    std::string toLower() const {
        std::string res(text_);
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return res;
    }

    std::string toUpper() const {
        std::string res(text_);
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
        return res;
    }

    std::string replaceAll(sv oldStr, sv newStr) const {
        if (oldStr.empty()) return std::string(text_);
        std::string out;
        out.reserve(text_.size());
        size_t start = 0;
        size_t pos = text_.find(oldStr, start);
        while (pos != sv::npos) {
            out.append(text_.data() + start, pos - start);
            out.append(newStr.data(), newStr.size());
            start = pos + oldStr.size();
            pos = text_.find(oldStr, start);
        }
        out.append(text_.data() + start, text_.size() - start);
        return out;
    }

    // ------------------------------------------------------------------
    // 4. FREQUENCY & TOOLS
    // ------------------------------------------------------------------

    // Returns word frequency map (owning strings). This is safe and simple.
    std::unordered_map<std::string, size_t> wordFrequencies(bool caseInsensitive = false) const {
        std::unordered_map<std::string, size_t> freq;
        auto tokens = tokenizeByWhitespaceView();
        freq.reserve(tokens.size() * 2 + 1);
        for (auto &t : tokens) {
            if (t.empty()) continue;
            if (caseInsensitive) {
                std::string lower = toLowerString(t);
                ++freq[lower];
            } else {
                ++freq[std::string(t)];
            }
        }
        return freq;
    }

    // Return top N words (owning strings) - efficient for large maps
    std::vector<std::pair<std::string, size_t>> topNWords(size_t N = 10, bool caseInsensitive = false) const {
        auto freq = wordFrequencies(caseInsensitive);
        std::vector<std::pair<std::string, size_t>> vec;
        vec.reserve(freq.size());
        for (auto &p : freq) vec.emplace_back(std::move(p));
        if (N == 0 || vec.empty()) return {};
        if (vec.size() <= N) {
            std::sort(vec.begin(), vec.end(), [](auto &a, auto &b){ return a.second > b.second; });
            return vec;
        }
        std::nth_element(vec.begin(), vec.begin() + N, vec.end(), [](auto &a, auto &b){ return a.second > b.second; });
        vec.resize(N);
        std::sort(vec.begin(), vec.end(), [](auto &a, auto &b){ return a.second > b.second; });
        return vec;
    }

    // Trim helpers
    sv trimView() const noexcept {
        size_t b = 0, e = text_.size();
        while (b < e && std::isspace(static_cast<unsigned char>(text_[b]))) ++b;
        while (e > b && std::isspace(static_cast<unsigned char>(text_[e-1]))) --e;
        return text_.substr(b, e - b);
    }

    std::string trim() const { auto v = trimView(); return std::string(v); }

    bool startsWith(sv prefix) const noexcept { return prefix.size() <= text_.size() && text_.compare(0, prefix.size(), prefix) == 0; }
    bool endsWith(sv suffix) const noexcept { return suffix.size() <= text_.size() && text_.compare(text_.size()-suffix.size(), suffix.size(), suffix) == 0; }

    // ------------------------------------------------------------------
    // 5. UTILITY
    // ------------------------------------------------------------------

    size_t length() const noexcept { return text_.size(); }
    bool isEmpty() const noexcept { return text_.empty(); }
    sv raw() const noexcept { return text_; }

private:
    // helper: produce lowercase string from a view
    static std::string toLowerString(sv s) {
        std::string out;
        out.resize(s.size());
        for (size_t i = 0; i < s.size(); ++i) out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
        return out;
    }

    // helper: iterate as unsigned chars
    static std::vector<unsigned char> svToUnsignedChars(sv s) {
        std::vector<unsigned char> v;
        v.reserve(s.size());
        for (unsigned char c : svToBytes(s)) v.push_back(c);
        return v;
    }

    // helper: raw bytes of the view
    static std::vector<unsigned char> svToBytes(sv s) {
        return std::vector<unsigned char>(reinterpret_cast<const unsigned char*>(s.data()), reinterpret_cast<const unsigned char*>(s.data() + s.size()));
    }

private:
    sv text_;
};

} // namespace Etas
