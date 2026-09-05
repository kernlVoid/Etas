/*
*     /////////  /////////    //\\        ///////
*     //            //       //  \\      //
*     /////////     //      //    \\     ///////
*     //            //     ////////\\          //
*     /////////     //    //        \\   ///////
*/

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
 *   fast, safer and more feature-rich TextAnalyzer
 * - Minimizes allocations by offering string_view-based accessors
 * - Additional useful tools: frequency analysis, top-N words, trim/startsWith/endsWith
 * - Marked noexcept where appropriate and optimized loops
 * - Header-only, C++17 compatible
 * - Output-iterator variants for zero-allocation transformations
 * - Safe type casting (unsigned char) for all character functions
 * - Allocation-free case-insensitive search using std::search
 */
class TextAnalyzer {
public:
    using sv = std::string_view;
    explicit TextAnalyzer(sv text) noexcept : text_(text) {}

    // ------------------------------------------------------------------
    // 1. SPLITTING FUNCTIONS (allocation-free variants)
    // ------------------------------------------------------------------

    // Split into substrings (owning strings) - kept for compatibility
    [[nodiscard]] std::vector<std::string> split(sv delimiter) const {
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
    [[nodiscard]] std::vector<sv> splitView(sv delimiter) const noexcept {
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
    [[nodiscard]] std::vector<sv> splitByNewlineView() const noexcept {
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
    [[nodiscard]] std::vector<std::string> splitByNewline() const {
        auto v = splitByNewlineView();
        std::vector<std::string> out;
        out.reserve(v.size());
        for (auto &s: v) out.emplace_back(s);
        return out;
    }

    // Tokenize by any whitespace (view variant - no allocations)
    [[nodiscard]] std::vector<sv> tokenizeByWhitespaceView() const noexcept {
        std::vector<sv> tokens;
        const char* ptr = text_.data();
        size_t len = text_.size();
        tokens.reserve(len / 4 + 1);

        size_t i = 0;
        while (i < len) {
            // skip whitespace
            while (i < len && isWhitespace(ptr[i])) ++i;
            if (i >= len) break;
            size_t start = i;
            while (i < len && !isWhitespace(ptr[i])) ++i;
            tokens.emplace_back(ptr + start, i - start);
        }
        return tokens;
    }

    // Owning version
    [[nodiscard]] std::vector<std::string> tokenizeByWhitespace() const {
        auto v = tokenizeByWhitespaceView();
        std::vector<std::string> out;
        out.reserve(v.size());
        for (auto &s: v) out.emplace_back(s);
        return out;
    }

    // ------------------------------------------------------------------
    // 2. SEARCH & ANALYSIS FUNCTIONS
    // ------------------------------------------------------------------

    // Find all occurrences (returns indices)
    [[nodiscard]] std::vector<size_t> findAll(sv search) const noexcept {
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

    [[nodiscard]] bool contains(sv sub) const noexcept {
        return !sub.empty() && !text_.empty() && text_.find(sub) != sv::npos;
    }

    // Case-insensitive search with zero allocations using std::search
    [[nodiscard]] bool containsCaseInsensitive(sv sub) const noexcept {
        if (sub.empty()) return true;
        if (text_.empty()) return sub.empty();

        auto char_comp = [](char a, char b) noexcept {
            return toLowerChar(a) == toLowerChar(b);
        };
        return std::search(text_.begin(), text_.end(), sub.begin(), sub.end(), char_comp) != text_.end();
    }

    // Count non-overlapping occurrences (safe and fast)
    [[nodiscard]] size_t count(sv sub) const noexcept {
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
    [[nodiscard]] std::string cleanWhitespace() const {
        if (text_.empty()) return {};
        std::string out;
        out.reserve(text_.size());
        bool lastSpace = false;
        const char* ptr = text_.data();
        size_t start = 0;
        size_t end = text_.size();

        // Skip leading whitespace
        while (start < end && isWhitespace(ptr[start])) {
            ++start;
        }

        // Skip trailing whitespace
        while (end > start && isWhitespace(ptr[end - 1])) {
            --end;
        }

        for (size_t i = start; i < end; ++i) {
            char c = ptr[i];
            if (isWhitespace(c)) {
                if (!lastSpace) {
                    out.push_back(' ');
                    lastSpace = true;
                }
            } else {
                out.push_back(c);
                lastSpace = false;
            }
        }
        return out;
    }

    // Zero-allocation variant using output iterator
    template <typename OutputIterator>
    OutputIterator cleanWhitespaceTo(OutputIterator result) const noexcept {
        if (text_.empty()) return result;

        const char* ptr = text_.data();
        size_t start = 0;
        size_t end = text_.size();

        // Skip leading whitespace
        while (start < end && isWhitespace(ptr[start])) {
            ++start;
        }

        // Skip trailing whitespace
        while (end > start && isWhitespace(ptr[end - 1])) {
            --end;
        }

        bool lastSpace = false;
        for (size_t i = start; i < end; ++i) {
            char c = ptr[i];
            if (isWhitespace(c)) {
                if (!lastSpace) {
                    *result++ = ' ';
                    lastSpace = true;
                }
            } else {
                *result++ = c;
                lastSpace = false;
            }
        }
        return result;
    }

    [[nodiscard]] std::string toLower() const {
        std::string res(text_);
        std::transform(res.begin(), res.end(), res.begin(), 
                      [](char c) { return toLowerChar(c); });
        return res;
    }

    // Zero-allocation variant using output iterator
    template <typename OutputIterator>
    OutputIterator toLowerTo(OutputIterator result) const noexcept {
        const char* ptr = text_.data();
        const size_t len = text_.size();
        for (size_t i = 0; i < len; ++i) {
            *result++ = toLowerChar(ptr[i]);
        }
        return result;
    }

    [[nodiscard]] std::string toUpper() const {
        std::string res(text_);
        std::transform(res.begin(), res.end(), res.begin(), 
                      [](char c) { return toUpperChar(c); });
        return res;
    }

    // Zero-allocation variant using output iterator
    template <typename OutputIterator>
    OutputIterator toUpperTo(OutputIterator result) const noexcept {
        const char* ptr = text_.data();
        const size_t len = text_.size();
        for (size_t i = 0; i < len; ++i) {
            *result++ = toUpperChar(ptr[i]);
        }
        return result;
    }

    [[nodiscard]] std::string replaceAll(sv oldStr, sv newStr) const {
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
    [[nodiscard]] std::unordered_map<std::string, size_t> 
    wordFrequencies(bool caseInsensitive = false) const {
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
    [[nodiscard]] std::vector<std::pair<std::string, size_t>> 
    topNWords(size_t N = 10, bool caseInsensitive = false) const {
        auto freq = wordFrequencies(caseInsensitive);
        std::vector<std::pair<std::string, size_t>> vec;
        vec.reserve(freq.size());
        for (auto &p : freq) vec.emplace_back(std::move(p));
        
        if (N == 0 || vec.empty()) return {};
        if (vec.size() <= N) {
            std::sort(vec.begin(), vec.end(), [](const auto &a, const auto &b){
                return a.second > b.second;
            });
            return vec;
        }
        
        std::nth_element(vec.begin(), vec.begin() + N, vec.end(), 
                        [](const auto &a, const auto &b){ return a.second > b.second; });
        vec.resize(N);
        std::sort(vec.begin(), vec.end(), [](const auto &a, const auto &b){
            return a.second > b.second;
        });
        return vec;
    }

    // Trim helpers
    [[nodiscard]] sv trimView() const noexcept {
        size_t b = 0, e = text_.size();
        while (b < e && isWhitespace(text_[b])) ++b;
        while (e > b && isWhitespace(text_[e-1])) --e;
        return text_.substr(b, e - b);
    }

    [[nodiscard]] std::string trim() const {
        auto v = trimView();
        return std::string(v);
    }

    [[nodiscard]] bool startsWith(sv prefix) const noexcept {
        return prefix.size() <= text_.size() && 
               text_.compare(0, prefix.size(), prefix) == 0;
    }

    [[nodiscard]] bool endsWith(sv suffix) const noexcept {
        return suffix.size() <= text_.size() && 
               text_.compare(text_.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    // ------------------------------------------------------------------
    // 5. UTILITY
    // ------------------------------------------------------------------

    [[nodiscard]] size_t length() const noexcept { return text_.size(); }
    [[nodiscard]] bool isEmpty() const noexcept { return text_.empty(); }
    [[nodiscard]] sv raw() const noexcept { return text_; }

private:
    // ------------------------------------------------------------------
    // PRIVATE HELPERS - Safe character operations with unsigned char cast
    // ------------------------------------------------------------------

    // Safe whitespace check with unsigned char cast
    static constexpr bool isWhitespace(char c) noexcept {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    }

    // Safe lowercase conversion with unsigned char cast
    static constexpr char toLowerChar(char c) noexcept {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Safe uppercase conversion with unsigned char cast
    static constexpr char toUpperChar(char c) noexcept {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    // Helper: produce lowercase string from a view with safe casting
    static std::string toLowerString(sv s) {
        std::string out;
        out.resize(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            out[i] = toLowerChar(s[i]);
        }
        return out;
    }

private:
    sv text_;
};

} // namespace Etas
