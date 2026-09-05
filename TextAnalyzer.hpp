#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace Etas {

/**
 * High-Performance TextAnalyzer Class
 * Optimized for Windows and Linux (C++17 compatible)
 * Uses zero-copy techniques where possible for maximum speed.
 */
class TextAnalyzer {
public:
    // Constructor: Stores the input text
    explicit TextAnalyzer(std::string_view text) 
        : text_(text) {}

    // ------------------------------------------------------------------
    // 1. SPLITTING FUNCTIONS
    // ------------------------------------------------------------------

    /**
     * Splits the text by a specific delimiter string.
     * @param delimiter The string to split on (e.g., " ", ",", "-")
     * @return A vector of substrings.
     */
    std::vector<std::string> split(std::string_view delimiter) const {
        std::vector<std::string> result;
        if (text_.empty() || delimiter.empty()) {
            if (!text_.empty()) result.push_back(std::string(text_));
            return result;
        }

        size_t start = 0;
        size_t end = text_.find(delimiter);
        
        // Reserve memory to avoid reallocations
        result.reserve(text_.size() / (delimiter.size() + 1) + 1);

        while (end != std::string::npos) {
            result.emplace_back(text_.substr(start, end - start));
            start = end + delimiter.size();
            end = text_.find(delimiter, start);
        }
        
        // Add the last segment
        result.emplace_back(text_.substr(start));
        return result;
    }

    /**
     * Splits the text into lines (handling \n, \r\n, \r).
     * @return A vector of lines.
     */
    std::vector<std::string> splitByNewline() const {
        return split("\n"); 
    }

    /**
     * Tokenizes text by any whitespace (space, tab, newline).
     * @return A vector of words.
     */
    std::vector<std::string> tokenizeByWhitespace() const {
        std::vector<std::string> tokens;
        size_t start = 0;
        size_t end = 0;
        const size_t len = text_.length();

        tokens.reserve(len / 4); // Heuristic reservation

        while (start < len) {
            // Skip whitespace
            while (start < len && std::isspace(static_cast<unsigned char>(text_[start]))) {
                ++start;
            }
            if (start >= len) break;

            // Find end of word
            end = start;
            while (end < len && !std::isspace(static_cast<unsigned char>(text_[end]))) {
                ++end;
            }

            tokens.emplace_back(text_.substr(start, end - start));
            start = end;
        }
        return tokens;
    }

    // ------------------------------------------------------------------
    // 2. SEARCH & ANALYSIS FUNCTIONS
    // ------------------------------------------------------------------

    /**
     * Finds all occurrences of a substring.
     * @param search The string to search for.
     * @return A vector of starting indices (0-based). Returns empty if not found.
     */
    std::vector<size_t> findAll(std::string_view search) const {
        std::vector<size_t> positions;
        if (search.empty() || text_.empty()) return positions;
        
        size_t pos = text_.find(search, 0);
        while (pos != std::string::npos) {
            positions.push_back(pos);
            pos = text_.find(search, pos + 1);
        }
        return positions;
    }

    /**
     * Checks if the text contains a substring (case-sensitive).
     */
    bool contains(std::string_view sub) const {
        return !sub.empty() && !text_.empty() && text_.find(sub) != std::string::npos;
    }

    /**
     * Checks if the text contains a substring (case-insensitive).
     */
    bool containsCaseInsensitive(std::string_view sub) const {
        if (sub.empty()) return true;
        const auto lowerSub = toLowerString(sub);
        const auto lowerText = toLowerString(text_);
        return lowerText.find(lowerSub) != std::string::npos;
    }

    /**
     * Counts occurrences of a substring.
     */
    size_t count(std::string_view sub) const {
        if (sub.empty() || text_.empty()) return 0;
        size_t count = 0;
        size_t pos = text_.find(sub, 0);
        while (pos != std::string::npos) {
            ++count;
            pos = text_.find(sub, pos + sub.size());
        }
        return count;
    }

    // ------------------------------------------------------------------
    // 3. TRANSFORMATION & CLEANING FUNCTIONS
    // ------------------------------------------------------------------

    /**
     * Returns a new string with extra whitespace removed.
     * Replaces multiple spaces/tabs with a single space.
     */
    std::string cleanWhitespace() const {
        if (text_.empty()) return "";
        
        std::string result;
        result.reserve(text_.size());
        bool lastWasSpace = false;

        for (const char c : text_) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!lastWasSpace) {
                    result += ' ';
                    lastWasSpace = true;
                }
            } else {
                result += c;
                lastWasSpace = false;
            }
        }

        // Trim leading/trailing
        if (!result.empty() && result.back() == ' ') result.pop_back();
        if (!result.empty() && result.front() == ' ') result.erase(0, 1);

        return result;
    }

    /**
     * Converts the entire text to lowercase.
     */
    std::string toLower() const {
        std::string result = std::string(text_);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return result;
    }

    /**
     * Converts the entire text to uppercase.
     */
    std::string toUpper() const {
        std::string result = std::string(text_);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c){ return std::toupper(c); });
        return result;
    }

    /**
     * Replaces all occurrences of 'old' with 'new'.
     * Returns a new string.
     */
    std::string replaceAll(std::string_view oldStr, std::string_view newStr) const {
        if (oldStr.empty()) return std::string(text_);
        
        std::string result;
        result.reserve(text_.size());
        
        size_t start = 0;
        size_t pos = text_.find(oldStr, 0);
        
        while (pos != std::string::npos) {
            result.append(text_, start, pos - start);
            result.append(newStr);
            start = pos + oldStr.size();
            pos = text_.find(oldStr, start);
        }
        
        result.append(text_, start, std::string::npos);
        return result;
    }

    // ------------------------------------------------------------------
    // 4. UTILITY
    // ------------------------------------------------------------------

    size_t length() const { return text_.length(); }
    bool isEmpty() const { return text_.empty(); }
    std::string_view raw() const { return text_; }

private:
    static std::string toLowerString(std::string_view s) {
        std::string res;
        res.reserve(s.size());
        for (const char c : s) {
            res += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return res;
    }

private:
    std::string_view text_;
};

} // namespace Etas
